/**
 * @file clock_sync.c
 * @brief Sets the wall clock from SNTP when the device comes online.
 *
 * @copyright Copyright (c) Centro de Inovacao EDGE
 */

#include "clock_sync.h"

#include <time.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/clock.h>

LOG_MODULE_REGISTER(clock_sync, LOG_LEVEL_INF);

#ifdef CONFIG_SNTP

#include <errno.h>

#include <zephyr/net/net_event.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/sntp.h>
#include <zephyr/net/wifi_mgmt.h>

/* A failed query is worth retrying soon; a successful one holds for an hour. */
#define RETRY_DELAY  K_SECONDS(20)
#define RESYNC_DELAY K_MINUTES(CONFIG_BRINGUP_SNTP_RESYNC_MINUTES)

/* Long enough for the ESP-AT link to finish associating and get a DHCP lease
 * before the first query goes out.
 */
#define FIRST_QUERY_DELAY K_SECONDS(2)

/* The query runs on a queue of its own rather than the system workqueue, for
 * two reasons: name resolution plus a socket needs far more stack than the
 * system queue's default 1 KiB (it overflows and faults), and a query that
 * times out blocks its thread for the whole timeout, which is not something to
 * inflict on a queue the rest of the system shares.
 */
#define SNTP_STACK_SIZE 4096
#define SNTP_PRIORITY   K_PRIO_PREEMPT(10)

static K_THREAD_STACK_DEFINE(sntp_stack, SNTP_STACK_SIZE);
static struct k_work_q sntp_workq;

static struct net_mgmt_event_callback l4_cb;
static struct k_work_delayable sync_work;

static bool link_up;
static bool valid;

static void sync_handler(struct k_work *work)
{
	int ret;
	struct sntp_time sntp;
	struct timespec wall;

	ARG_UNUSED(work);

	if (!link_up) {
		return;
	}

	/* sntp_simple() blocks on a socket, which is why this runs on a
	 * workqueue rather than straight from the net_mgmt callback.
	 */
	ret = sntp_simple(CONFIG_BRINGUP_SNTP_SERVER, CONFIG_BRINGUP_SNTP_TIMEOUT_MS, &sntp);
	if (ret < 0) {
		/* -EAGAIN here is usually name resolution, not the NTP server:
		 * check `net dns` lists a server.
		 */
		LOG_WRN("%s: no time (%d), retrying", CONFIG_BRINGUP_SNTP_SERVER, ret);
		k_work_reschedule_for_queue(&sntp_workq, &sync_work, RETRY_DELAY);
		return;
	}

	wall.tv_sec = (time_t)sntp.seconds;
	wall.tv_nsec = 0;

	ret = sys_clock_settime(SYS_CLOCK_REALTIME, &wall);
	if (ret < 0) {
		LOG_ERR("could not set the clock (%d)", ret);
		k_work_reschedule_for_queue(&sntp_workq, &sync_work, RETRY_DELAY);
		return;
	}

	valid = true;
	LOG_INF("clock set from %s: %llu", CONFIG_BRINGUP_SNTP_SERVER,
		(unsigned long long)sntp.seconds);

	k_work_reschedule_for_queue(&sntp_workq, &sync_work, RESYNC_DELAY);
}

static void on_l4_event(struct net_mgmt_event_callback *cb, uint64_t event, struct net_if *iface)
{
	ARG_UNUSED(cb);
	ARG_UNUSED(iface);

	switch (event) {
	case NET_EVENT_L4_CONNECTED:
		LOG_INF("online, syncing the clock");
		link_up = true;
		k_work_reschedule_for_queue(&sntp_workq, &sync_work, FIRST_QUERY_DELAY);
		break;
	case NET_EVENT_L4_DISCONNECTED:
		LOG_INF("offline");
		link_up = false;
		/* The time already set stays valid and keeps ticking off the
		 * kernel clock; only re-syncing has to wait for the link.
		 */
		break;
	default:
		break;
	}
}

void clock_sync_init(void)
{
	const struct k_work_queue_config config = {
		.name = "sntp",
	};

	k_work_queue_start(&sntp_workq, sntp_stack, K_THREAD_STACK_SIZEOF(sntp_stack),
			   SNTP_PRIORITY, &config);

	k_work_init_delayable(&sync_work, sync_handler);

	net_mgmt_init_event_callback(&l4_cb, on_l4_event,
				     NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED);
	net_mgmt_add_event_callback(&l4_cb);

#ifdef CONFIG_WIFI_CREDENTIALS_CONNECT_STORED
	{
		int ret;
		struct net_if *iface = net_if_get_first_wifi();

		if (iface == NULL) {
			LOG_WRN("no Wi-Fi interface to connect");
			return;
		}

		/* Uses whatever `wifi cred add` put in flash. Absent
		 * credentials are not an error worth shouting about: the board
		 * is then simply offline and the title bar carries no clock.
		 */
		ret = net_mgmt(NET_REQUEST_WIFI_CONNECT_STORED, iface, NULL, 0);
		if (ret < 0) {
			LOG_INF("no stored credentials to connect with (%d)", ret);
		}
	}
#endif /* CONFIG_WIFI_CREDENTIALS_CONNECT_STORED */
}

const char *clock_sync_state(void)
{
	if (valid) {
		return "synced";
	}

	return link_up ? "syncing" : "no link";
}

#else /* !CONFIG_SNTP */

static const bool valid;

void clock_sync_init(void)
{
	/* No networking in this build, so there is nothing to sync against. */
}

const char *clock_sync_state(void)
{
	return "not in build";
}

#endif /* CONFIG_SNTP */

bool clock_sync_is_valid(void)
{
	return valid;
}

bool clock_sync_format(char *buffer, size_t size)
{
	struct timespec now;
	struct tm parts;
	time_t local;

	if (!valid) {
		if (size > 0) {
			buffer[0] = '\0';
		}
		return false;
	}

	if (sys_clock_gettime(SYS_CLOCK_REALTIME, &now) < 0) {
		if (size > 0) {
			buffer[0] = '\0';
		}
		return false;
	}

	/* SNTP hands out UTC and there is no timezone database here, so the
	 * offset is applied to the epoch seconds and the result read back with
	 * gmtime_r -- localtime_r would apply nothing on top of it anyway.
	 */
	local = (time_t)now.tv_sec + (CONFIG_BRINGUP_UTC_OFFSET_MINUTES * 60);

	if (gmtime_r(&local, &parts) == NULL) {
		if (size > 0) {
			buffer[0] = '\0';
		}
		return false;
	}

	snprintk(buffer, size, "%02d:%02d", parts.tm_hour, parts.tm_min);

	return true;
}
