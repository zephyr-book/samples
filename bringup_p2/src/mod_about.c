/**
 * @file mod_about.c
 * @brief Build identification: board revision, Zephyr and LVGL versions.
 *
 * @copyright Copyright (c) Centro de Inovacao EDGE
 */

#include "module.h"

#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/version.h>

LOG_MODULE_REGISTER(mod_about, LOG_LEVEL_INF);

static struct oled_menu_screen screen;

static int create(const struct oled_menu_screen *home)
{
	lv_obj_t *text;
	int ret = oled_menu_screen_create(&screen, "About", OLED_MENU_LAYOUT_PANEL);

	if (ret < 0) {
		return ret;
	}

	text = oled_menu_label_add(
		&screen, CONFIG_BOARD_TARGET
		"\n"
		"Zephyr " KERNEL_VERSION_STRING "\n"
		"LVGL " STRINGIFY(LVGL_VERSION_MAJOR) "." STRINGIFY(
							  LVGL_VERSION_MINOR));
	lv_obj_set_style_pad_bottom(text, 4, LV_PART_MAIN);

	oled_menu_back_row_add(&screen, home);

	return 0;
}

static const struct oled_menu_screen *get_screen(void)
{
	return &screen;
}

const struct bringup_module module_about = {
	.label = "About",
	.create = create,
	.screen = get_screen,
	.refresh = NULL,
};
