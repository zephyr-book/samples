/**
 * @file mod_sense.c
 * @brief The two plain sense lines: the hall sensor and the TMP1075 ALERT.
 *
 *        Both are described with the board's "edge,gpio-inputs" binding, which
 *        exists precisely so signals like these are not mis-described as keys.
 *        No driver binds them, so they are read straight from the GPIO.
 *
 *        The hall sensor can only pull low and has no external pull-up, so the
 *        devicetree asks for an internal one; ALERT is open-drain with a
 *        pull-up already fitted. Both read active-low.
 *
 * @copyright Copyright (c) Centro de Inovacao EDGE
 */

#include "module.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mod_sense, LOG_LEVEL_INF);

#define SENSE_COUNT 3

static const struct gpio_dt_spec lines[SENSE_COUNT] = {
	GPIO_DT_SPEC_GET(DT_NODELABEL(mag_sense), gpios),
	GPIO_DT_SPEC_GET(DT_NODELABEL(temp_alert), gpios),
	GPIO_DT_SPEC_GET(DT_NODELABEL(sd_cd), gpios),
};

static const char *const names[SENSE_COUNT] = {"HALL", "ALRT", "SDCD"};

static struct oled_menu_screen screen;
static lv_obj_t *rows[SENSE_COUNT];

/* Transitions seen while this screen has been up. A magnet swiped past the
 * hall sensor is over in well under a second, so the count is what makes it
 * testable: swipe, then read how many times the line moved. Polling at the
 * refresh rate is plenty for that -- and the TMAG5213's output can latch, so
 * the level alone may not return on its own.
 */
static uint32_t changes[SENSE_COUNT];
static int last[SENSE_COUNT];

static void refresh(void)
{
	for (size_t i = 0; i < SENSE_COUNT; i++) {
		int value;

		if (!gpio_is_ready_dt(&lines[i])) {
			lv_label_set_text_fmt(rows[i], "%-4s not ready", names[i]);
			continue;
		}

		value = gpio_pin_get_dt(&lines[i]);
		if (value < 0) {
			lv_label_set_text_fmt(rows[i], "%-4s err %d", names[i], value);
			continue;
		}

		if (last[i] >= 0 && value != last[i]) {
			changes[i]++;
		}
		last[i] = value;

		/* Logical, not electrical: the devicetree already accounts for
		 * the active-low wiring.
		 */
		lv_label_set_text_fmt(rows[i], "%-4s %-6s n=%u", names[i],
				      value ? "ACTIVE" : "idle", (unsigned int)changes[i]);
	}
}

static void on_reset(lv_event_t *event)
{
	ARG_UNUSED(event);

	for (size_t i = 0; i < SENSE_COUNT; i++) {
		changes[i] = 0;
	}
}

static int create(const struct oled_menu_screen *home)
{
	int ret = oled_menu_screen_create(&screen, "Sense lines", OLED_MENU_LAYOUT_LIST);

	if (ret < 0) {
		return ret;
	}

	for (size_t i = 0; i < SENSE_COUNT; i++) {
		last[i] = -1;

		if (gpio_is_ready_dt(&lines[i])) {
			ret = gpio_pin_configure_dt(&lines[i], GPIO_INPUT);
			if (ret < 0) {
				LOG_ERR("%s: configure failed (%d)", names[i], ret);
			}
		} else {
			LOG_WRN("%s is not ready", names[i]);
		}

		rows[i] = oled_menu_label_add(&screen, names[i]);
	}

	oled_menu_row_add(&screen, "Reset counts", on_reset, NULL);
	oled_menu_back_row_add(&screen, home);

	return 0;
}

static const struct oled_menu_screen *get_screen(void)
{
	return &screen;
}

const struct bringup_module module_sense = {
	.label = "Sense lines",
	.create = create,
	.screen = get_screen,
	.refresh = refresh,
};
