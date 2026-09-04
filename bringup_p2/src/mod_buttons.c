/**
 * @file mod_buttons.c
 * @brief The four user buttons and the rotary encoder.
 *
 *        All five signals already reach LVGL as Zephyr input events, so this
 *        screen subscribes to the same stream rather than reading the GPIOs:
 *        what it shows is exactly what the input subsystem is reporting. The
 *        encoder's own button is LVGL's select, so pressing it here also
 *        activates the focused row -- that is the test.
 *
 * @copyright Copyright (c) Centro de Inovacao EDGE
 */

#include "module.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mod_buttons, LOG_LEVEL_INF);

#define BUTTON_COUNT 4

static const uint16_t button_codes[BUTTON_COUNT] = {
	INPUT_BTN_0,
	INPUT_BTN_1,
	INPUT_BTN_2,
	INPUT_BTN_3,
};

static struct oled_menu_screen screen;
static lv_obj_t *button_label;
static lv_obj_t *encoder_label;

/* Written from the input callback, read by the LVGL refresh. Both run in
 * thread context and each value is a single word, so no lock is needed.
 */
static volatile bool button_state[BUTTON_COUNT];
static volatile bool encoder_pressed;
static volatile int32_t encoder_position;
static volatile uint32_t encoder_edges;

static void on_input_event(struct input_event *event, void *user_data)
{
	ARG_UNUSED(user_data);

	if (event->type == INPUT_EV_REL && event->code == INPUT_REL_WHEEL) {
		encoder_position += event->value;
		encoder_edges++;
		return;
	}

	if (event->type != INPUT_EV_KEY) {
		return;
	}

	if (event->code == INPUT_KEY_ENTER) {
		encoder_pressed = (event->value != 0);
		return;
	}

	for (size_t i = 0; i < BUTTON_COUNT; i++) {
		if (event->code == button_codes[i]) {
			button_state[i] = (event->value != 0);
			return;
		}
	}
}

/* NULL device: every input device in the system, which is what lets one
 * callback cover the buttons (gpio-keys) and the encoder (gpio-qdec) at once.
 */
INPUT_CALLBACK_DEFINE(NULL, on_input_event, NULL);

static void refresh(void)
{
	char marks[BUTTON_COUNT + 1];

	for (size_t i = 0; i < BUTTON_COUNT; i++) {
		marks[i] = button_state[i] ? '*' : '-';
	}
	marks[BUTTON_COUNT] = '\0';

	lv_label_set_text_fmt(button_label, "BTN 0123\n    %s", marks);
	lv_label_set_text_fmt(encoder_label, "ENC %+ld  n=%u\nPRESS  %c", (long)encoder_position,
			      (unsigned int)encoder_edges, encoder_pressed ? '*' : '-');
}

static void on_reset(lv_event_t *event)
{
	ARG_UNUSED(event);

	encoder_position = 0;
	encoder_edges = 0;
}

static int create(const struct oled_menu_screen *home)
{
	int ret = oled_menu_screen_create(&screen, "Buttons/Enc", OLED_MENU_LAYOUT_LIST);

	if (ret < 0) {
		return ret;
	}

	button_label = oled_menu_label_add(&screen, "BTN 0123\n    ----");
	encoder_label = oled_menu_label_add(&screen, "ENC +0  n=0\nPRESS  -");

	oled_menu_row_add(&screen, "Reset count", on_reset, NULL);
	oled_menu_back_row_add(&screen, home);

	return 0;
}

static const struct oled_menu_screen *get_screen(void)
{
	return &screen;
}

const struct bringup_module module_buttons = {
	.label = "Buttons/Enc",
	.create = create,
	.screen = get_screen,
	.refresh = refresh,
};
