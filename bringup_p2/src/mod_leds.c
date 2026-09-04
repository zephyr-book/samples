/**
 * @file mod_leds.c
 * @brief The four discrete LEDs plus the white one behind Q6, which the
 *        silkscreen calls LAMP.
 *
 *        led0..led3 sink current from +3V3 and are active-low; the white LED
 *        is switched by a low-side MOSFET and is active-high. Both polarities
 *        are described in the devicetree, so this file treats them alike.
 *
 * @copyright Copyright (c) Centro de Inovacao EDGE
 */

#include "module.h"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

LOG_MODULE_REGISTER(mod_leds, LOG_LEVEL_INF);

#define LED_COUNT 5

/* The white LED is taken by its white_led node label rather than through the
 * led4 alias: "led4" says nothing about which LED it is, and this is the file
 * someone reads to find out whether it is covered at all. It is the only one
 * with a MOSFET in front of it, so also the only active-high one -- but that
 * is described in the devicetree, so nothing here has to special-case it.
 */
static const struct gpio_dt_spec leds[LED_COUNT] = {
	GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios),          GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios),          GPIO_DT_SPEC_GET(DT_ALIAS(led3), gpios),
	GPIO_DT_SPEC_GET(DT_NODELABEL(white_led), gpios),
};

/* Named LAMP, not WHITE, to match what is printed on the board next to it. */
static const char *const names[LED_COUNT] = {"LED0", "LED1", "LED2", "LED3", "LAMP"};

static struct oled_menu_screen screen;
static lv_obj_t *rows[LED_COUNT];
static bool states[LED_COUNT];

static void row_update(size_t index)
{
	if (!gpio_is_ready_dt(&leds[index])) {
		oled_menu_row_set_text_fmt(rows[index], "%-5s  n/a", names[index]);
		return;
	}

	oled_menu_row_set_text_fmt(rows[index], "%-5s  [%c]", names[index],
				   states[index] ? '*' : ' ');
}

static void on_toggle(lv_event_t *event)
{
	int ret;
	size_t index = (size_t)(uintptr_t)lv_event_get_user_data(event);

	if (!gpio_is_ready_dt(&leds[index])) {
		return;
	}

	ret = gpio_pin_set_dt(&leds[index], states[index] ? 0 : 1);
	if (ret < 0) {
		LOG_ERR("%s: set failed (%d)", names[index], ret);
		return;
	}

	states[index] = !states[index];
	row_update(index);
}

/** Light every LED, or none: the quickest way to spot a dead one. */
static void on_all(lv_event_t *event)
{
	bool on = (bool)(uintptr_t)lv_event_get_user_data(event);

	for (size_t i = 0; i < LED_COUNT; i++) {
		if (!gpio_is_ready_dt(&leds[i])) {
			continue;
		}

		if (gpio_pin_set_dt(&leds[i], on ? 1 : 0) == 0) {
			states[i] = on;
		}

		row_update(i);
	}
}

static int create(const struct oled_menu_screen *home)
{
	int ret = oled_menu_screen_create(&screen, "LEDs", OLED_MENU_LAYOUT_LIST);

	if (ret < 0) {
		return ret;
	}

	for (size_t i = 0; i < LED_COUNT; i++) {
		if (gpio_is_ready_dt(&leds[i])) {
			ret = gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
			if (ret < 0) {
				LOG_ERR("%s: configure failed (%d)", names[i], ret);
			}
		} else {
			LOG_WRN("%s is not ready", names[i]);
		}

		rows[i] = oled_menu_row_add(&screen, "", on_toggle, (void *)(uintptr_t)i);
		row_update(i);
	}

	oled_menu_row_add(&screen, "All on", on_all, (void *)(uintptr_t) true);
	oled_menu_row_add(&screen, "All off", on_all, (void *)(uintptr_t) false);
	oled_menu_back_row_add(&screen, home);

	return 0;
}

static const struct oled_menu_screen *get_screen(void)
{
	return &screen;
}

const struct bringup_module module_leds = {
	.label = "LEDs",
	.create = create,
	.screen = get_screen,
	.refresh = NULL,
};
