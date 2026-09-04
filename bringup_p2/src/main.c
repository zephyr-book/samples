/**
 * @file main.c
 * @brief bringup -- one application that exercises every module on the ZBook
 *        P2, presented as an encoder-driven menu on the on-board OLED.
 *
 *        Board target: zbook@p2/rp2350b/m33. Add --shield zbook_wifi to
 *        include the Wi-Fi screen; everything else is on the board itself.
 *
 * @copyright Copyright (c) Centro de Inovacao EDGE
 */

#include "clock_sync.h"
#include "module.h"

#include <lvgl.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#if !DT_NODE_EXISTS(DT_NODELABEL(encoder_qdec))
#error "bringup needs the ZBook P2 encoder: build with -b zbook@p2/rp2350b/m33"
#endif

/* Cap on how long the loop may sleep between lv_timer_handler() calls. LVGL
 * returns LV_NO_TIMER_READY (UINT32_MAX) when it has nothing pending, which
 * would otherwise park the thread indefinitely.
 */
#define LVGL_MAX_SLEEP_MS 50

int main(void)
{
	int ret;
	const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

	if (!device_is_ready(display_dev)) {
		LOG_ERR("Display %s is not ready", display_dev->name);
		return -ENODEV;
	}

	ret = menu_init();
	if (ret < 0) {
		LOG_ERR("Failed to build the menu (%d)", ret);
		return ret;
	}

	/* Asks Wi-Fi to connect with the credentials in flash, then sets the
	 * clock from SNTP once the link comes up. A no-op without networking.
	 */
	clock_sync_init();

	/* Render the first frame before unblanking so the panel never shows the
	 * controller's power-on garbage.
	 */
	lv_timer_handler();
	display_blanking_off(display_dev);

	LOG_INF("Turn the encoder to move, press to select");

	while (true) {
		uint32_t next_ms = lv_timer_handler();

		k_msleep(MIN(next_ms, LVGL_MAX_SLEEP_MS));
	}

	return 0;
}
