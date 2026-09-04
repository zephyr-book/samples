/**
 * @file menu.c
 * @brief The home screen: one row per on-board module, and the timer that
 *        refreshes whichever module screen is showing.
 *
 * @copyright Copyright (c) Centro de Inovacao EDGE
 */

#include "clock_sync.h"
#include "module.h"

#include <errno.h>
#include <stddef.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(menu, LOG_LEVEL_INF);

/** Fast enough to feel live on a button press, slow enough not to flicker. */
#define REFRESH_PERIOD_MS 100

/** The title bar only shows minutes, so once a second is plenty. */
#define CLOCK_PERIOD_MS 1000

/* Every module is listed unconditionally, including hardware that may not be
 * fitted: a row struck through is more useful than a row that is not there.
 */
static const struct bringup_module *const modules[] = {
	&module_leds,   &module_buttons, &module_strip, &module_buzzer,
	&module_analog, &module_sense,   &module_ir,    &module_temp,
	&module_accel,  &module_display, &module_wifi,  &module_about,
};

static struct oled_menu_screen home;

static void on_open_module(lv_event_t *event)
{
	const struct bringup_module *module = lv_event_get_user_data(event);

	oled_menu_show(module->screen());
}

static void clock_timer_cb(lv_timer_t *timer)
{
	char text[8];

	ARG_UNUSED(timer);

	/* Empty until SNTP has set the clock, which leaves the title bar to
	 * the title alone rather than showing a placeholder time.
	 */
	(void)clock_sync_format(text, sizeof(text));
	oled_menu_clock_set(text);
}

static void refresh_timer_cb(lv_timer_t *timer)
{
	ARG_UNUSED(timer);

	for (size_t i = 0; i < ARRAY_SIZE(modules); i++) {
		if (modules[i]->refresh == NULL) {
			continue;
		}

		if (oled_menu_is_active(modules[i]->screen())) {
			modules[i]->refresh();
			return;
		}
	}
}

int menu_init(void)
{
	int ret;

	ret = oled_menu_init();
	if (ret < 0) {
		return ret;
	}

	ret = oled_menu_screen_create(&home, "ZBook P2", OLED_MENU_LAYOUT_LIST);
	if (ret < 0) {
		return ret;
	}

	for (size_t i = 0; i < ARRAY_SIZE(modules); i++) {
		lv_obj_t *row;

		ret = modules[i]->create(&home);
		if (ret < 0) {
			LOG_ERR("%s: screen setup failed (%d)", modules[i]->label, ret);
			return ret;
		}

		row = oled_menu_row_add(&home, modules[i]->label, on_open_module,
					(void *)modules[i]);

		/* Hardware that did not probe keeps its row and its screen, but
		 * the row is struck through: on a board with parts left
		 * unpopulated that is the most useful thing the menu can say.
		 */
		if (modules[i]->present != NULL && !modules[i]->present()) {
			oled_menu_row_set_absent(row, true);
			LOG_INF("%s: not present", modules[i]->label);
		}
	}

	lv_timer_create(refresh_timer_cb, REFRESH_PERIOD_MS, NULL);
	lv_timer_create(clock_timer_cb, CLOCK_PERIOD_MS, NULL);

	oled_menu_show(&home);

	return 0;
}
