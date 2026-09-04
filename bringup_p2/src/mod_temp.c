/**
 * @file mod_temp.c
 * @brief The TMP1075 temperature sensor on i2c0.
 *
 *        Not populated on every P2 board. Reporting that plainly is part of
 *        the job of a bring-up tool, so an absent sensor gets "not fitted"
 *        rather than being left out of the menu.
 *
 * @copyright Copyright (c) Centro de Inovacao EDGE
 */

#include "module.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <stdbool.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mod_temp, LOG_LEVEL_INF);

/* Room temperature to hot-to-the-touch: enough range for the bar to move
 * visibly when a finger is put on the package.
 */
#define BAR_MIN_C 10
#define BAR_MAX_C 45

static const struct device *const sensor = DEVICE_DT_GET(DT_NODELABEL(tmp1075));

static struct oled_menu_screen screen;
static lv_obj_t *value;
static lv_obj_t *bar;

static void refresh(void)
{
	int ret;
	struct sensor_value reading;

	if (!device_is_ready(sensor)) {
		/* The driver failed to probe, which for an I2C part means it
		 * did not answer at its address at all.
		 */
		lv_label_set_text(value, "not fitted");
		return;
	}

	ret = sensor_sample_fetch_chan(sensor, SENSOR_CHAN_AMBIENT_TEMP);
	if (ret < 0) {
		lv_label_set_text_fmt(value, "fetch %d", ret);
		return;
	}

	ret = sensor_channel_get(sensor, SENSOR_CHAN_AMBIENT_TEMP, &reading);
	if (ret < 0) {
		lv_label_set_text_fmt(value, "get %d", ret);
		return;
	}

	lv_label_set_text_fmt(value, "%d.%02d C", reading.val1, abs(reading.val2) / 10000);
	lv_bar_set_value(bar, reading.val1, LV_ANIM_OFF);
}

static int create(const struct oled_menu_screen *home)
{
	int ret = oled_menu_screen_create(&screen, "Temperature", OLED_MENU_LAYOUT_PANEL);

	if (ret < 0) {
		return ret;
	}

	if (!device_is_ready(sensor)) {
		LOG_WRN("%s did not probe: not fitted on this board?", sensor->name);
	}

	value = oled_menu_label_add(&screen, "-- C");
	lv_obj_set_style_pad_bottom(value, 4, LV_PART_MAIN);

	bar = oled_menu_bar_add(&screen, BAR_MIN_C, BAR_MAX_C);

	oled_menu_back_row_add(&screen, home);

	return 0;
}

static bool present(void)
{
	return device_is_ready(sensor);
}

static const struct oled_menu_screen *get_screen(void)
{
	return &screen;
}

const struct bringup_module module_temp = {
	.label = "Temperature",
	.create = create,
	.screen = get_screen,
	.refresh = refresh,
	.present = present,
};
