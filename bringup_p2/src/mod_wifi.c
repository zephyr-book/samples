/**
 * @file mod_wifi.c
 * @brief The ESP8266 on the H2 Wi-Fi header (uart1), via the zbook_wifi shield.
 *
 *        The row is always in the menu, struck through when Wi-Fi is not
 *        usable, so the absence is visible rather than silent. There are two
 *        different reasons it can be unusable, and they need different
 *        answers:
 *
 *        - Built without --shield zbook_wifi. There is then no esp8266 node
 *          and no networking stack to call into, so the stub at the bottom of
 *          this file stands in and says so.
 *        - Built with the shield, but the module does not answer. The ESP-AT
 *          driver sends AT commands during init and fails to probe if nothing
 *          replies, so device_is_ready() is a real check on the hardware, not
 *          just on the devicetree.
 *
 * @copyright Copyright (c) Centro de Inovacao EDGE
 */

#include <zephyr/kernel.h>

#include "module.h"

#ifdef CONFIG_WIFI

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>

LOG_MODULE_REGISTER(mod_wifi, LOG_LEVEL_INF);

static const struct device *const esp = DEVICE_DT_GET(DT_NODELABEL(esp8266));

static struct oled_menu_screen screen;
static lv_obj_t *state_label;
static lv_obj_t *result_label;

static struct net_mgmt_event_callback wifi_cb;

/* Written from the net_mgmt thread, read by the LVGL refresh. Single words, so
 * no lock is needed -- the screen only ever shows a slightly stale count.
 */
static volatile uint32_t ap_count;
static volatile bool scanning;
static volatile int scan_status;

/* Strongest AP seen, a quick sanity check on the antenna. Starts at the
 * weakest representable value so the first real reading always wins.
 */
static volatile int8_t best_rssi = INT8_MIN;

static void on_wifi_event(struct net_mgmt_event_callback *cb, uint64_t event, struct net_if *iface)
{
	ARG_UNUSED(iface);

	switch (event) {
	case NET_EVENT_WIFI_SCAN_RESULT: {
		const struct wifi_scan_result *result = cb->info;

		/* A scan can be started from outside this screen too -- `wifi
		 * scan` on the net shell in the debug build, for one. Treat the
		 * first result as the start of a scan so the counters describe
		 * that scan and not a mix of it and the previous one.
		 */
		if (!scanning) {
			scanning = true;
			ap_count = 0;
			best_rssi = INT8_MIN;
			scan_status = 0;
		}

		ap_count++;
		if (result->rssi > best_rssi) {
			best_rssi = result->rssi;
		}
		break;
	}
	case NET_EVENT_WIFI_SCAN_DONE: {
		const struct wifi_status *status = cb->info;

		scan_status = status->status;
		scanning = false;
		break;
	}
	default:
		break;
	}
}

/**
 * @brief Start a scan.
 *
 * A scan is the useful bring-up test: it proves the AT link and that the radio
 * receives, without needing credentials for any particular network.
 */
static void on_scan(lv_event_t *event)
{
	int ret;
	struct net_if *iface = net_if_get_first_wifi();

	ARG_UNUSED(event);

	if (iface == NULL) {
		lv_label_set_text(result_label, "no wifi iface");
		return;
	}

	if (scanning) {
		return;
	}

	ap_count = 0;
	best_rssi = INT8_MIN;
	scan_status = 0;
	scanning = true;

	ret = net_mgmt(NET_REQUEST_WIFI_SCAN, iface, NULL, 0);
	if (ret < 0) {
		scanning = false;
		LOG_ERR("scan request failed (%d)", ret);
		lv_label_set_text_fmt(result_label, "req err %d", ret);
	}
}

static void refresh(void)
{
	struct net_if *iface;

	if (!device_is_ready(esp)) {
		lv_label_set_text(state_label, "no response");
		lv_label_set_text(result_label, "check H2 header");
		return;
	}

	iface = net_if_get_first_wifi();
	if (iface == NULL) {
		lv_label_set_text(state_label, "no iface");
		return;
	}

	lv_label_set_text_fmt(state_label, "iface %s", net_if_is_up(iface) ? "up" : "down");

	if (scanning) {
		lv_label_set_text_fmt(result_label, "scan.. %u", (unsigned int)ap_count);
		return;
	}

	if (scan_status != 0) {
		lv_label_set_text_fmt(result_label, "scan err %d", scan_status);
		return;
	}

	if (ap_count == 0) {
		lv_label_set_text(result_label, "no scan yet");
		return;
	}

	lv_label_set_text_fmt(result_label, "%u APs\nbest %d dBm", (unsigned int)ap_count,
			      (int)best_rssi);
}

static int create(const struct oled_menu_screen *home)
{
	int ret = oled_menu_screen_create(&screen, "Wi-Fi", OLED_MENU_LAYOUT_LIST);

	if (ret < 0) {
		return ret;
	}

	if (!device_is_ready(esp)) {
		LOG_WRN("%s did not answer: shield not fitted?", esp->name);
	}

	net_mgmt_init_event_callback(&wifi_cb, on_wifi_event,
				     NET_EVENT_WIFI_SCAN_RESULT | NET_EVENT_WIFI_SCAN_DONE);
	net_mgmt_add_event_callback(&wifi_cb);

	state_label = oled_menu_label_add(&screen, "iface ?");
	result_label = oled_menu_label_add(&screen, "no scan yet");

	oled_menu_row_add(&screen, "Scan", on_scan, NULL);
	oled_menu_back_row_add(&screen, home);

	return 0;
}

static bool present(void)
{
	return device_is_ready(esp);
}

#else /* !CONFIG_WIFI */

#include <stdbool.h>

/* No shield in this build, so there is no ESP-AT driver and no networking
 * stack to call. The row still appears, struck through, and says why.
 */
static struct oled_menu_screen screen;

static int create(const struct oled_menu_screen *home)
{
	int ret = oled_menu_screen_create(&screen, "Wi-Fi", OLED_MENU_LAYOUT_PANEL);

	if (ret < 0) {
		return ret;
	}

	oled_menu_label_add(&screen, "not in build\n\nrebuild with\n--shield\nzbook_wifi");
	oled_menu_back_row_add(&screen, home);

	return 0;
}

static void refresh(void)
{
	/* Nothing changes: the shield cannot appear without a rebuild. */
}

static bool present(void)
{
	return false;
}

#endif /* CONFIG_WIFI */

static const struct oled_menu_screen *get_screen(void)
{
	return &screen;
}

const struct bringup_module module_wifi = {
	.label = "Wi-Fi",
	.create = create,
	.screen = get_screen,
	.refresh = refresh,
	.present = present,
};
