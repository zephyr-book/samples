/**
 * @file oled_menu.h
 * @brief A small encoder-driven menu layer for the ZBook's 128x64 monochrome
 *        OLED, shared by the samples under samples/oled/.
 *
 *        The look is fixed by the panel: an inverted 9 px title bar over rows
 *        of 10 px, sized against the UNSCII 8 font (which makes the screen an
 *        exact 16x8 character grid). Focus is drawn as a full row inversion,
 *        because a 1 px outline is easy to miss at 1 bpp.
 *
 *        Navigation is LVGL's own: every focusable row on a screen joins that
 *        screen's lv_group_t, and the encoder is pointed at the group of
 *        whichever screen is showing. Rotating moves the focus, pressing sends
 *        LV_EVENT_CLICKED to the focused row.
 *
 * @copyright Copyright (c) Centro de Inovacao EDGE
 */

#ifndef OLED_MENU_H_
#define OLED_MENU_H_

#include <stdbool.h>
#include <stdint.h>

#include <lvgl.h>

/** Height of the inverted title bar, in pixels. */
#define OLED_MENU_TITLE_HEIGHT_PX 9

/** Height of one focusable row, in pixels. */
#define OLED_MENU_ROW_HEIGHT_PX 10

/** Characters that fit across the screen in the UNSCII 8 font. */
#define OLED_MENU_COLUMNS 16

/** How the content area below the title bar arranges its children. */
enum oled_menu_layout {
	/** Rows packed from the top; scrolls when they overflow. */
	OLED_MENU_LAYOUT_LIST,
	/** Children centred; for a value plus a bar plus a back row. */
	OLED_MENU_LAYOUT_PANEL,
};

/** One screen, its focus group, and the area widgets are added to. */
struct oled_menu_screen {
	/** The LVGL screen object. */
	lv_obj_t *screen;
	/** Content area below the title bar; parent of everything added. */
	lv_obj_t *content;
	/** Title text, at the left of the title bar. */
	lv_obj_t *title;
	/** Clock text, right-aligned in the title bar. Empty until set. */
	lv_obj_t *clock;
	/** Focus group the encoder is pointed at while this screen shows. */
	lv_group_t *group;
};

/**
 * @brief Initialise the shared styles and find the encoder input device.
 *
 * Call once, from the thread that will pump lv_timer_handler(). Any LVGL
 * encoder input device registered by the devicetree is used; the node's label
 * does not matter.
 *
 * @retval 0 on success.
 * @retval -ENODEV if LVGL has no encoder input device.
 */
int oled_menu_init(void);

/**
 * @brief Build an empty screen with an inverted title bar.
 *
 * @param[out] screen Filled in on success.
 * @param      title  Title bar text, at most @ref OLED_MENU_COLUMNS characters.
 * @param      layout How the content area arranges its children.
 *
 * @retval 0 on success.
 * @retval -ENOMEM if LVGL could not allocate the screen or its group.
 */
int oled_menu_screen_create(struct oled_menu_screen *screen, const char *title,
			    enum oled_menu_layout layout);

/**
 * @brief Replace a screen's title bar text.
 *
 * @param screen Screen to retitle.
 * @param title  New text.
 */
void oled_menu_screen_set_title(struct oled_menu_screen *screen, const char *title);

/**
 * @brief Show a screen and point the encoder at its focus group.
 *
 * @param screen Screen to display.
 */
void oled_menu_show(const struct oled_menu_screen *screen);

/**
 * @brief Set the text shown at the right of the title bar.
 *
 * Only the visible screen is written; the text is remembered and re-applied by
 * oled_menu_show(), so it does not blink when screens change. Titles have to
 * leave room for it: the bar is @ref OLED_MENU_COLUMNS characters wide in
 * total.
 *
 * @param text Text to show, or "" to clear it.
 */
void oled_menu_clock_set(const char *text);

/**
 * @brief Whether a screen is the one currently on the display.
 *
 * Lets a periodic refresh update only the visible screen.
 *
 * @param screen Screen to test.
 *
 * @return True if @p screen is active.
 */
bool oled_menu_is_active(const struct oled_menu_screen *screen);

/**
 * @brief Add a focusable row to a screen.
 *
 * @param screen    Screen to add to.
 * @param text      Row text.
 * @param on_click  Called on LV_EVENT_CLICKED, i.e. an encoder press. May be
 *                  NULL for a row that only displays something.
 * @param user_data Passed to @p on_click as the event user data.
 *
 * @return The row widget, or NULL on allocation failure.
 */
lv_obj_t *oled_menu_row_add(struct oled_menu_screen *screen, const char *text,
			    lv_event_cb_t on_click, void *user_data);

/**
 * @brief Add a "< Back" row that shows @p target when pressed.
 *
 * @param screen Screen to add the row to.
 * @param target Screen to return to. Must outlive @p screen.
 *
 * @return The row widget, or NULL on allocation failure.
 */
lv_obj_t *oled_menu_back_row_add(struct oled_menu_screen *screen,
				 const struct oled_menu_screen *target);

/**
 * @brief Replace a row's text.
 *
 * @param row  Row returned by oled_menu_row_add().
 * @param text New text.
 */
void oled_menu_row_set_text(lv_obj_t *row, const char *text);

/**
 * @brief Replace a row's text from a printf-style format.
 *
 * @param row Row returned by oled_menu_row_add().
 * @param fmt Format string, then its arguments.
 */
void oled_menu_row_set_text_fmt(lv_obj_t *row, const char *fmt, ...);

/**
 * @brief Mark a row as referring to hardware that is not present.
 *
 * The row stays in the list and stays selectable -- opening it is how you see
 * why -- but its text is struck through so an absent module is obvious from
 * the menu itself.
 *
 * @param row    Row returned by oled_menu_row_add().
 * @param absent True to strike the text through, false to clear it.
 */
void oled_menu_row_set_absent(lv_obj_t *row, bool absent);

/**
 * @brief Add a plain, non-focusable label.
 *
 * Update it afterwards with lv_label_set_text() or lv_label_set_text_fmt().
 *
 * @param screen Screen to add to.
 * @param text   Initial text.
 *
 * @return The label, or NULL on allocation failure.
 */
lv_obj_t *oled_menu_label_add(struct oled_menu_screen *screen, const char *text);

/**
 * @brief Add a horizontal progress bar, 120 px wide.
 *
 * Update it afterwards with lv_bar_set_value().
 *
 * @param screen Screen to add to.
 * @param min    Value at empty.
 * @param max    Value at full.
 *
 * @return The bar, or NULL on allocation failure.
 */
lv_obj_t *oled_menu_bar_add(struct oled_menu_screen *screen, int32_t min, int32_t max);

#endif /* OLED_MENU_H_ */
