/**
 * @file mod_analog.c
 * @brief The three analog inputs: potentiometer, LDR and microphone.
 *
 *        All three are plain ADC channels described in the board devicetree
 *        (channels 1, 4 and 0 respectively). Rather than crowding them onto
 *        one screen, the module row opens a submenu naming each input, and
 *        each input then gets a screen to itself: the reading in counts, as a
 *        percentage of full scale, and a bar.
 *
 * @copyright Copyright (c) Centro de Inovacao EDGE
 */

#include "module.h"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mod_analog, LOG_LEVEL_INF);

/* Matches zephyr,resolution on the channel nodes. */
#define RESOLUTION_BITS 12
#define FULL_SCALE      ((1 << RESOLUTION_BITS) - 1)

static const struct device *const adc = DEVICE_DT_GET(DT_NODELABEL(adc));

/** One analog input and the widgets showing it. */
struct analog_input {
	/** Submenu row text. Up to OLED_MENU_COLUMNS characters. */
	const char *label;
	/** Title bar text. Shorter, because the clock shares the bar. */
	const char *title;
	/** The ADC channel, straight from the devicetree. */
	const struct adc_channel_cfg channel;
	struct oled_menu_screen screen;
	lv_obj_t *value;
	lv_obj_t *bar;
	bool ready;
};

static struct analog_input inputs[] = {
	{
		.label = "Potentiometer",
		.title = "Pot",
		.channel = ADC_CHANNEL_CFG_DT(DT_NODELABEL(potentiometer)),
	},
	{
		.label = "LDR (light)",
		.title = "LDR",
		.channel = ADC_CHANNEL_CFG_DT(DT_NODELABEL(ldr_adc)),
	},
	{
		.label = "Microphone",
		.title = "Mic",
		.channel = ADC_CHANNEL_CFG_DT(DT_NODELABEL(microphone_adc)),
	},
};

static struct oled_menu_screen submenu;

static void on_open_input(lv_event_t *event)
{
	oled_menu_show(&((struct analog_input *)lv_event_get_user_data(event))->screen);
}

static void input_refresh(struct analog_input *input)
{
	int ret;
	uint16_t sample = 0;
	struct adc_sequence sequence = {
		.buffer = &sample,
		.buffer_size = sizeof(sample),
		.channels = BIT(input->channel.channel_id),
		.resolution = RESOLUTION_BITS,
	};

	if (!input->ready) {
		lv_label_set_text(input->value, "not ready");
		return;
	}

	ret = adc_read(adc, &sequence);
	if (ret < 0) {
		lv_label_set_text_fmt(input->value, "error %d", ret);
		return;
	}

	/* Counts as well as the percentage: a bring-up test wants the raw
	 * number, and the percentage is what the bar shows.
	 */
	lv_label_set_text_fmt(input->value, "%u counts\n%u.%u %%", (unsigned int)sample,
			      (unsigned int)((sample * 1000U) / FULL_SCALE) / 10U,
			      (unsigned int)((sample * 1000U) / FULL_SCALE) % 10U);
	lv_bar_set_value(input->bar, sample, LV_ANIM_OFF);
}

static void refresh(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(inputs); i++) {
		if (oled_menu_is_active(&inputs[i].screen)) {
			input_refresh(&inputs[i]);
			return;
		}
	}
}

static int create(const struct oled_menu_screen *home)
{
	int ret = oled_menu_screen_create(&submenu, "Analog in", OLED_MENU_LAYOUT_LIST);

	if (ret < 0) {
		return ret;
	}

	if (!device_is_ready(adc)) {
		LOG_WRN("%s is not ready", adc->name);
	}

	for (size_t i = 0; i < ARRAY_SIZE(inputs); i++) {
		struct analog_input *input = &inputs[i];

		if (device_is_ready(adc)) {
			ret = adc_channel_setup(adc, &input->channel);
			if (ret < 0) {
				LOG_ERR("%s: channel %u setup failed (%d)", input->label,
					input->channel.channel_id, ret);
			} else {
				input->ready = true;
			}
		}

		ret = oled_menu_screen_create(&input->screen, input->title, OLED_MENU_LAYOUT_PANEL);
		if (ret < 0) {
			return ret;
		}

		input->value = oled_menu_label_add(&input->screen, "--");
		lv_obj_set_style_pad_bottom(input->value, 3, LV_PART_MAIN);
		input->bar = oled_menu_bar_add(&input->screen, 0, FULL_SCALE);
		oled_menu_back_row_add(&input->screen, &submenu);

		oled_menu_row_add(&submenu, input->label, on_open_input, input);
	}

	oled_menu_back_row_add(&submenu, home);

	return 0;
}

static const struct oled_menu_screen *get_screen(void)
{
	return &submenu;
}

static bool is_showing(void)
{
	if (oled_menu_is_active(&submenu)) {
		return true;
	}

	for (size_t i = 0; i < ARRAY_SIZE(inputs); i++) {
		if (oled_menu_is_active(&inputs[i].screen)) {
			return true;
		}
	}

	return false;
}

static bool present(void)
{
	return device_is_ready(adc);
}

const struct bringup_module module_analog = {
	.label = "Analog in",
	.create = create,
	.screen = get_screen,
	.is_showing = is_showing,
	.refresh = refresh,
	.present = present,
};
