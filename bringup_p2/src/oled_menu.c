/**
 * @file oled_menu.c
 * @brief Shared encoder-driven menu layer for the ZBook OLED samples.
 *
 * @copyright Copyright (c) Centro de Inovacao EDGE
 */

#include "oled_menu.h"

#include <errno.h>
#include <stdarg.h>
#include <string.h>

#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

LOG_MODULE_REGISTER(oled_menu, LOG_LEVEL_INF);

#define BAR_WIDTH_PX  120
#define BAR_HEIGHT_PX 7

/** Longest row text this layer will render. One screen line plus a margin. */
#define ROW_TEXT_MAX 32

static lv_indev_t *encoder;

/* The screen on display, and the clock text to keep in its title bar. Only the
 * visible screen is written; oled_menu_show() re-applies the text so switching
 * screens does not blink the clock away.
 */
static const struct oled_menu_screen *active_screen;
static char clock_text[8];

/* Shared styles. Static so LVGL can keep referring to them for the whole run. */
static lv_style_t style_screen;
static lv_style_t style_title;
static lv_style_t style_row;
static lv_style_t style_row_focused;
static lv_style_t style_bar;
static lv_style_t style_bar_indicator;

static void styles_init(void)
{
	/* Screens and containers: no border, no padding, no gaps. On 64 rows of
	 * pixels there is nothing to spend on decoration.
	 */
	lv_style_init(&style_screen);
	lv_style_set_bg_opa(&style_screen, LV_OPA_COVER);
	lv_style_set_bg_color(&style_screen, lv_color_white());
	lv_style_set_text_color(&style_screen, lv_color_black());
	lv_style_set_border_width(&style_screen, 0);
	lv_style_set_outline_width(&style_screen, 0);
	lv_style_set_radius(&style_screen, 0);
	lv_style_set_pad_all(&style_screen, 0);
	lv_style_set_pad_gap(&style_screen, 0);

	/* The one permanently inverted strip, so the current screen is
	 * readable at a glance.
	 */
	lv_style_init(&style_title);
	lv_style_set_bg_opa(&style_title, LV_OPA_COVER);
	lv_style_set_bg_color(&style_title, lv_color_black());
	lv_style_set_text_color(&style_title, lv_color_white());
	lv_style_set_width(&style_title, lv_pct(100));
	lv_style_set_height(&style_title, OLED_MENU_TITLE_HEIGHT_PX);
	lv_style_set_pad_left(&style_title, 1);
	lv_style_set_pad_right(&style_title, 1);

	/* A row is a button with all decoration stripped: at 1 bpp the only
	 * focus indicator worth having is a full inversion, not an outline.
	 */
	lv_style_init(&style_row);
	lv_style_set_bg_opa(&style_row, LV_OPA_TRANSP);
	lv_style_set_text_color(&style_row, lv_color_black());
	lv_style_set_border_width(&style_row, 0);
	lv_style_set_outline_width(&style_row, 0);
	lv_style_set_shadow_width(&style_row, 0);
	lv_style_set_radius(&style_row, 0);
	lv_style_set_width(&style_row, lv_pct(100));
	lv_style_set_height(&style_row, OLED_MENU_ROW_HEIGHT_PX);
	lv_style_set_pad_all(&style_row, 0);
	lv_style_set_pad_left(&style_row, 2);

	lv_style_init(&style_row_focused);
	lv_style_set_bg_opa(&style_row_focused, LV_OPA_COVER);
	lv_style_set_bg_color(&style_row_focused, lv_color_black());
	lv_style_set_text_color(&style_row_focused, lv_color_white());

	lv_style_init(&style_bar);
	lv_style_set_bg_opa(&style_bar, LV_OPA_TRANSP);
	lv_style_set_border_width(&style_bar, 1);
	lv_style_set_border_color(&style_bar, lv_color_black());
	lv_style_set_radius(&style_bar, 0);
	lv_style_set_pad_all(&style_bar, 1);
	lv_style_set_width(&style_bar, BAR_WIDTH_PX);
	lv_style_set_height(&style_bar, BAR_HEIGHT_PX);

	lv_style_init(&style_bar_indicator);
	lv_style_set_bg_opa(&style_bar_indicator, LV_OPA_COVER);
	lv_style_set_bg_color(&style_bar_indicator, lv_color_black());
	lv_style_set_radius(&style_bar_indicator, 0);
}

int oled_menu_init(void)
{
	lv_indev_t *indev = NULL;

	/* Take whichever encoder input device the devicetree registered. Going
	 * by type rather than by node label keeps this layer independent of
	 * what each sample calls its overlay node.
	 */
	while ((indev = lv_indev_get_next(indev)) != NULL) {
		if (lv_indev_get_type(indev) == LV_INDEV_TYPE_ENCODER) {
			encoder = indev;
			break;
		}
	}

	if (encoder == NULL) {
		LOG_ERR("No LVGL encoder input device is registered");
		return -ENODEV;
	}

	styles_init();

	return 0;
}

int oled_menu_screen_create(struct oled_menu_screen *screen, const char *title,
			    enum oled_menu_layout layout)
{
	lv_obj_t *bar;

	screen->screen = lv_obj_create(NULL);
	if (screen->screen == NULL) {
		return -ENOMEM;
	}

	screen->group = lv_group_create();
	if (screen->group == NULL) {
		lv_obj_delete(screen->screen);
		screen->screen = NULL;
		return -ENOMEM;
	}

	lv_obj_remove_style_all(screen->screen);
	lv_obj_add_style(screen->screen, &style_screen, LV_PART_MAIN);
	lv_obj_remove_flag(screen->screen, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_flex_flow(screen->screen, LV_FLEX_FLOW_COLUMN);

	/* The bar is a flex row rather than a bare label so the clock can sit
	 * hard right while the title keeps the space that is left. Text colour
	 * and font are inherited by both labels from the bar's style.
	 */
	bar = lv_obj_create(screen->screen);
	lv_obj_remove_style_all(bar);
	lv_obj_add_style(bar, &style_title, LV_PART_MAIN);
	lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

	screen->title = lv_label_create(bar);
	lv_obj_remove_style_all(screen->title);
	lv_obj_set_flex_grow(screen->title, 1);
	lv_label_set_text(screen->title, title);

	screen->clock = lv_label_create(bar);
	lv_obj_remove_style_all(screen->clock);
	lv_label_set_text(screen->clock, clock_text);

	screen->content = lv_obj_create(screen->screen);
	lv_obj_remove_style_all(screen->content);
	lv_obj_add_style(screen->content, &style_screen, LV_PART_MAIN);
	lv_obj_set_width(screen->content, lv_pct(100));
	lv_obj_set_flex_grow(screen->content, 1);
	lv_obj_set_flex_flow(screen->content, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_scrollbar_mode(screen->content, LV_SCROLLBAR_MODE_OFF);

	if (layout == OLED_MENU_LAYOUT_LIST) {
		lv_obj_set_flex_align(screen->content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
				      LV_FLEX_ALIGN_START);
		/* More rows than fit is normal for a menu; LVGL scrolls the
		 * focused one into view by itself.
		 */
		lv_obj_add_flag(screen->content, LV_OBJ_FLAG_SCROLLABLE);
	} else {
		lv_obj_set_flex_align(screen->content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
				      LV_FLEX_ALIGN_CENTER);
		lv_obj_remove_flag(screen->content, LV_OBJ_FLAG_SCROLLABLE);
	}

	return 0;
}

void oled_menu_screen_set_title(struct oled_menu_screen *screen, const char *title)
{
	lv_label_set_text(screen->title, title);
}

void oled_menu_show(const struct oled_menu_screen *screen)
{
	active_screen = screen;
	lv_label_set_text(screen->clock, clock_text);
	lv_indev_set_group(encoder, screen->group);
	lv_screen_load(screen->screen);
}

void oled_menu_clock_set(const char *text)
{
	if (strcmp(text, clock_text) == 0) {
		return;
	}

	strncpy(clock_text, text, sizeof(clock_text) - 1);
	clock_text[sizeof(clock_text) - 1] = '\0';

	if (active_screen != NULL) {
		lv_label_set_text(active_screen->clock, clock_text);
	}
}

bool oled_menu_is_active(const struct oled_menu_screen *screen)
{
	return lv_screen_active() == screen->screen;
}

lv_obj_t *oled_menu_row_add(struct oled_menu_screen *screen, const char *text,
			    lv_event_cb_t on_click, void *user_data)
{
	lv_obj_t *label;
	lv_obj_t *row = lv_button_create(screen->content);

	if (row == NULL) {
		return NULL;
	}

	lv_obj_remove_style_all(row);
	lv_obj_add_style(row, &style_row, LV_PART_MAIN);
	lv_obj_add_style(row, &style_row_focused, LV_PART_MAIN | LV_STATE_FOCUSED);

	label = lv_label_create(row);
	lv_obj_remove_style_all(label);
	lv_obj_center(label);
	lv_label_set_text(label, text);

	if (on_click != NULL) {
		lv_obj_add_event_cb(row, on_click, LV_EVENT_CLICKED, user_data);
	}

	lv_group_add_obj(screen->group, row);

	return row;
}

static void on_back(lv_event_t *event)
{
	oled_menu_show(lv_event_get_user_data(event));
}

lv_obj_t *oled_menu_back_row_add(struct oled_menu_screen *screen,
				 const struct oled_menu_screen *target)
{
	/* on_back only reads the target, but the event user data is void *. */
	return oled_menu_row_add(screen, "< Back", on_back, (void *)target);
}

void oled_menu_row_set_absent(lv_obj_t *row, bool absent)
{
	lv_obj_t *label = lv_obj_get_child(row, 0);

	lv_obj_set_style_text_decor(
		label, absent ? LV_TEXT_DECOR_STRIKETHROUGH : LV_TEXT_DECOR_NONE, LV_PART_MAIN);
}

void oled_menu_row_set_text(lv_obj_t *row, const char *text)
{
	lv_label_set_text(lv_obj_get_child(row, 0), text);
}

void oled_menu_row_set_text_fmt(lv_obj_t *row, const char *fmt, ...)
{
	char text[ROW_TEXT_MAX];
	va_list args;

	va_start(args, fmt);
	vsnprintk(text, sizeof(text), fmt, args);
	va_end(args);

	oled_menu_row_set_text(row, text);
}

lv_obj_t *oled_menu_label_add(struct oled_menu_screen *screen, const char *text)
{
	lv_obj_t *label = lv_label_create(screen->content);

	if (label == NULL) {
		return NULL;
	}

	lv_obj_remove_style_all(label);
	lv_obj_set_style_text_color(label, lv_color_black(), LV_PART_MAIN);
	lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
	lv_label_set_text(label, text);

	return label;
}

lv_obj_t *oled_menu_bar_add(struct oled_menu_screen *screen, int32_t min, int32_t max)
{
	lv_obj_t *bar = lv_bar_create(screen->content);

	if (bar == NULL) {
		return NULL;
	}

	lv_obj_remove_style_all(bar);
	lv_obj_add_style(bar, &style_bar, LV_PART_MAIN);
	lv_obj_add_style(bar, &style_bar_indicator, LV_PART_INDICATOR);
	lv_bar_set_range(bar, min, max);
	lv_bar_set_value(bar, min, LV_ANIM_OFF);

	return bar;
}
