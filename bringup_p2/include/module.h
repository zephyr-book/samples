/**
 * @file module.h
 * @brief The interface every hardware module in the bring-up menu implements.
 *
 *        One application covers the whole board: each on-board module
 *        contributes one entry to the table in menu.c, which builds one row on
 *        the home screen and one screen behind it. A single LVGL timer drives
 *        the refresh of whichever screen is showing, so a module that displays
 *        live values only has to fill in @ref bringup_module.refresh.
 *
 * @copyright Copyright (c) Centro de Inovacao EDGE
 */

#ifndef BRINGUP_MODULE_H_
#define BRINGUP_MODULE_H_

#include <stdbool.h>

#include <oled_menu.h>

/** One on-board hardware module, as presented in the menu. */
struct bringup_module {
	/** Home-screen row text. At most @ref OLED_MENU_COLUMNS characters. */
	const char *label;

	/**
	 * @brief Build this module's screen.
	 *
	 * @param home Screen its "< Back" row must return to.
	 *
	 * @retval 0 on success, negative errno otherwise.
	 */
	int (*create)(const struct oled_menu_screen *home);

	/** @return This module's screen, valid once create() succeeded. */
	const struct oled_menu_screen *(*screen)(void);

	/**
	 * @brief Re-read the hardware and update the widgets.
	 *
	 * Called periodically, only while this module's screen is showing.
	 * NULL for a module with nothing live to show.
	 */
	void (*refresh)(void);

	/**
	 * @brief Whether the hardware this module tests is actually there.
	 *
	 * Checked once, after create(). A module that reports false keeps its
	 * menu row and its screen, but the row is struck through. NULL for a
	 * module that is always present, such as the LEDs.
	 *
	 * @return False if the device did not probe.
	 */
	bool (*present)(void);
};

/* The modules, in the order they appear on the home screen. */
extern const struct bringup_module module_leds;
extern const struct bringup_module module_buttons;
extern const struct bringup_module module_strip;
extern const struct bringup_module module_buzzer;
extern const struct bringup_module module_analog;
extern const struct bringup_module module_sense;
extern const struct bringup_module module_ir;
extern const struct bringup_module module_temp;
extern const struct bringup_module module_accel;
extern const struct bringup_module module_display;
extern const struct bringup_module module_wifi;
extern const struct bringup_module module_about;

/**
 * @brief Build the home screen and every module screen behind it.
 *
 * @retval 0 on success.
 * @retval -ENODEV if LVGL has no encoder input device.
 * @retval -ENOMEM if a screen could not be allocated.
 */
int menu_init(void);

#endif /* BRINGUP_MODULE_H_ */
