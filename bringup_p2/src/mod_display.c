/**
 * @file mod_display.c
 * @brief The OLED itself.
 *
 *        Reading this screen already proves the panel works, so what is left
 *        to exercise is the parts a static image does not touch: the contrast
 *        register, hardware blanking, and both pixel polarities. The reported
 *        geometry comes from the driver rather than from constants here, so a
 *        mis-described panel shows up as wrong numbers.
 *
 * @copyright Copyright (c) Centro de Inovacao EDGE
 */

#include "module.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mod_display, LOG_LEVEL_INF);

#define BLANK_MS 700

static const struct device *const display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

static const uint8_t contrasts[] = {255, 128, 64, 16};

static struct oled_menu_screen screen;
static lv_obj_t *info;
static lv_obj_t *status;
static size_t contrast_index;

static void on_contrast(lv_event_t *event)
{
	int ret;

	ARG_UNUSED(event);

	contrast_index = (contrast_index + 1U) % ARRAY_SIZE(contrasts);

	ret = display_set_contrast(display, contrasts[contrast_index]);
	if (ret < 0) {
		lv_label_set_text_fmt(status, "contrast %d", ret);
		return;
	}

	lv_label_set_text_fmt(status, "contrast %u", (unsigned int)contrasts[contrast_index]);
}

/** Blank and unblank: exercises the panel-on/off command path. */
static void on_blank(lv_event_t *event)
{
	int ret;

	ARG_UNUSED(event);

	ret = display_blanking_on(display);
	if (ret < 0) {
		lv_label_set_text_fmt(status, "blank %d", ret);
		return;
	}

	k_msleep(BLANK_MS);

	ret = display_blanking_off(display);
	if (ret < 0) {
		lv_label_set_text_fmt(status, "unblank %d", ret);
		return;
	}

	lv_label_set_text(status, "blank ok");
}

static int create(const struct oled_menu_screen *home)
{
	struct display_capabilities caps;
	int ret = oled_menu_screen_create(&screen, "Display", OLED_MENU_LAYOUT_LIST);

	if (ret < 0) {
		return ret;
	}

	display_get_capabilities(display, &caps);

	info = oled_menu_label_add(&screen, "");
	lv_label_set_text_fmt(info, "%ux%u pf=%u", (unsigned int)caps.x_resolution,
			      (unsigned int)caps.y_resolution,
			      (unsigned int)caps.current_pixel_format);

	status = oled_menu_label_add(&screen, "contrast 128");

	oled_menu_row_add(&screen, "Contrast next", on_contrast, NULL);
	oled_menu_row_add(&screen, "Blank test", on_blank, NULL);
	oled_menu_back_row_add(&screen, home);

	return 0;
}

static const struct oled_menu_screen *get_screen(void)
{
	return &screen;
}

const struct bringup_module module_display = {
	.label = "Display",
	.create = create,
	.screen = get_screen,
	.refresh = NULL,
};
