/**
 * @file mod_accel.c
 * @brief The BMI323 accelerometer / gyroscope on spi1.
 *
 *        Not populated on every P2 board; like the temperature sensor, an
 *        absent part is reported rather than hidden. The BMI323 driver takes
 *        int-gpios as mandatory, so INT1 (GPIO15) has to be wired for it to
 *        probe at all.
 *
 * @copyright Copyright (c) Centro de Inovacao EDGE
 */

#include "module.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <stdbool.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mod_accel, LOG_LEVEL_INF);

static const struct device *const sensor = DEVICE_DT_GET(DT_NODELABEL(bmi323));
static const struct gpio_dt_spec int2 = GPIO_DT_SPEC_GET(DT_NODELABEL(int2), gpios);

static struct oled_menu_screen screen;
static lv_obj_t *axes;
static lv_obj_t *interrupt;

static void refresh(void)
{
	int ret;
	struct sensor_value values[3];

	if (!device_is_ready(sensor)) {
		lv_label_set_text(axes, "not fitted");
	} else {
		ret = sensor_sample_fetch_chan(sensor, SENSOR_CHAN_ACCEL_XYZ);
		if (ret < 0) {
			lv_label_set_text_fmt(axes, "fetch %d", ret);
		} else {
			ret = sensor_channel_get(sensor, SENSOR_CHAN_ACCEL_XYZ, values);
			if (ret < 0) {
				lv_label_set_text_fmt(axes, "get %d", ret);
			} else {
				/* m/s^2 to two decimals, one axis per line: three
				 * numbers do not fit across 16 characters.
				 */
				lv_label_set_text_fmt(axes, "X %5d.%02d\nY %5d.%02d\nZ %5d.%02d",
						      values[0].val1, abs(values[0].val2) / 10000,
						      values[1].val1, abs(values[1].val2) / 10000,
						      values[2].val1, abs(values[2].val2) / 10000);
			}
		}
	}

	if (!gpio_is_ready_dt(&int2)) {
		lv_label_set_text(interrupt, "INT2 n/a");
		return;
	}

	lv_label_set_text_fmt(interrupt, "INT2 %s", gpio_pin_get_dt(&int2) > 0 ? "ACTIVE" : "idle");
}

static int create(const struct oled_menu_screen *home)
{
	int ret = oled_menu_screen_create(&screen, "Accel", OLED_MENU_LAYOUT_LIST);

	if (ret < 0) {
		return ret;
	}

	if (!device_is_ready(sensor)) {
		LOG_WRN("%s did not probe: not fitted on this board?", sensor->name);
	}

	if (gpio_is_ready_dt(&int2)) {
		ret = gpio_pin_configure_dt(&int2, GPIO_INPUT);
		if (ret < 0) {
			LOG_ERR("INT2 configure failed (%d)", ret);
		}
	}

	axes = oled_menu_label_add(&screen, "X --\nY --\nZ --");
	interrupt = oled_menu_label_add(&screen, "INT2 idle");

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

const struct bringup_module module_accel = {
	.label = "Accelerometer",
	.create = create,
	.screen = get_screen,
	.refresh = refresh,
	.present = present,
};
