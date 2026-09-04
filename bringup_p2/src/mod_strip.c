/**
 * @file mod_strip.c
 * @brief The addressable LED chain (4x FZ2812-5050 on PIO1).
 *
 *        The chain length comes from the devicetree, so the same screen works
 *        if a later revision lengthens it. Colours are set as whole-chain
 *        presets rather than per pixel: for a bring-up test what matters is
 *        that every LED lights and that all three channels work.
 *
 * @copyright Copyright (c) Centro de Inovacao EDGE
 */

#include "module.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led_strip.h>
#include <stdbool.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mod_strip, LOG_LEVEL_INF);

#define STRIP_NODE   DT_ALIAS(led_strip)
#define STRIP_LENGTH DT_PROP(STRIP_NODE, chain_length)

/* Kept dim on purpose: at full scale four 5050 packages are unpleasant to look
 * at from 30 cm away, and a dim LED still proves the channel works.
 */
#define LEVEL 0x20

static const struct device *const strip = DEVICE_DT_GET(STRIP_NODE);

static struct oled_menu_screen screen;
static lv_obj_t *status;
static struct led_rgb pixels[STRIP_LENGTH];

static const struct led_rgb presets[] = {
	{.r = LEVEL, .g = 0, .b = 0}, {.r = 0, .g = LEVEL, .b = 0},
	{.r = 0, .g = 0, .b = LEVEL}, {.r = LEVEL, .g = LEVEL, .b = LEVEL},
	{.r = 0, .g = 0, .b = 0},
};

static const char *const preset_names[] = {"Red", "Green", "Blue", "White", "Off"};

static void on_set_colour(lv_event_t *event)
{
	int ret;
	size_t index = (size_t)(uintptr_t)lv_event_get_user_data(event);

	if (!device_is_ready(strip)) {
		lv_label_set_text(status, "not ready");
		return;
	}

	for (size_t i = 0; i < STRIP_LENGTH; i++) {
		pixels[i] = presets[index];
	}

	ret = led_strip_update_rgb(strip, pixels, STRIP_LENGTH);
	if (ret < 0) {
		LOG_ERR("update failed (%d)", ret);
		lv_label_set_text_fmt(status, "error %d", ret);
		return;
	}

	lv_label_set_text_fmt(status, "%u LEDs: %s", (unsigned int)STRIP_LENGTH,
			      preset_names[index]);
}

/** Walk one lit pixel along the chain, which is what proves the wiring order. */
static void on_chase(lv_event_t *event)
{
	ARG_UNUSED(event);

	if (!device_is_ready(strip)) {
		lv_label_set_text(status, "not ready");
		return;
	}

	for (size_t lit = 0; lit < STRIP_LENGTH; lit++) {
		for (size_t i = 0; i < STRIP_LENGTH; i++) {
			pixels[i] = (i == lit) ? presets[3] : presets[4];
		}

		if (led_strip_update_rgb(strip, pixels, STRIP_LENGTH) < 0) {
			break;
		}

		k_msleep(150);
	}

	for (size_t i = 0; i < STRIP_LENGTH; i++) {
		pixels[i] = presets[4];
	}
	(void)led_strip_update_rgb(strip, pixels, STRIP_LENGTH);

	lv_label_set_text(status, "chase done");
}

static int create(const struct oled_menu_screen *home)
{
	int ret = oled_menu_screen_create(&screen, "RGB strip", OLED_MENU_LAYOUT_LIST);

	if (ret < 0) {
		return ret;
	}

	if (!device_is_ready(strip)) {
		LOG_WRN("%s is not ready", strip->name);
	}

	status = oled_menu_label_add(&screen, device_is_ready(strip) ? "ready" : "not ready");

	for (size_t i = 0; i < ARRAY_SIZE(presets); i++) {
		oled_menu_row_add(&screen, preset_names[i], on_set_colour, (void *)(uintptr_t)i);
	}

	oled_menu_row_add(&screen, "Chase", on_chase, NULL);
	oled_menu_back_row_add(&screen, home);

	return 0;
}

static bool present(void)
{
	return device_is_ready(strip);
}

static const struct oled_menu_screen *get_screen(void)
{
	return &screen;
}

const struct bringup_module module_strip = {
	.label = "RGB strip",
	.create = create,
	.screen = get_screen,
	.refresh = NULL,
	.present = present,
};
