/**
 * @file mod_analog.c
 * @brief The three analog inputs: potentiometer, LDR and microphone.
 *
 *        All three are plain ADC channels described in the board devicetree
 *        (channels 1, 4 and 0 respectively), so one screen covers them and
 *        shows raw counts next to a percentage of full scale.
 *
 * @copyright Copyright (c) Centro de Inovacao EDGE
 */

#include "module.h"

#include <errno.h>
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

#define CHANNEL_COUNT 3

static const struct device *const adc = DEVICE_DT_GET(DT_NODELABEL(adc));

static const struct adc_channel_cfg channels[CHANNEL_COUNT] = {
	ADC_CHANNEL_CFG_DT(DT_NODELABEL(potentiometer)),
	ADC_CHANNEL_CFG_DT(DT_NODELABEL(ldr_adc)),
	ADC_CHANNEL_CFG_DT(DT_NODELABEL(microphone_adc)),
};

static const char *const names[CHANNEL_COUNT] = {"POT", "LDR", "MIC"};

static struct oled_menu_screen screen;
static lv_obj_t *rows[CHANNEL_COUNT];
static bool ready[CHANNEL_COUNT];

static void refresh(void)
{
	for (size_t i = 0; i < CHANNEL_COUNT; i++) {
		int ret;
		uint16_t sample = 0;
		struct adc_sequence sequence = {
			.buffer = &sample,
			.buffer_size = sizeof(sample),
			.channels = BIT(channels[i].channel_id),
			.resolution = RESOLUTION_BITS,
		};

		if (!ready[i]) {
			lv_label_set_text_fmt(rows[i], "%-3s  not ready", names[i]);
			continue;
		}

		ret = adc_read(adc, &sequence);
		if (ret < 0) {
			lv_label_set_text_fmt(rows[i], "%-3s  err %d", names[i], ret);
			continue;
		}

		lv_label_set_text_fmt(rows[i], "%-3s %4u %3u%%", names[i], (unsigned int)sample,
				      (unsigned int)((sample * 100U) / FULL_SCALE));
	}
}

static int create(const struct oled_menu_screen *home)
{
	int ret = oled_menu_screen_create(&screen, "Pot/LDR/Mic", OLED_MENU_LAYOUT_LIST);

	if (ret < 0) {
		return ret;
	}

	for (size_t i = 0; i < CHANNEL_COUNT; i++) {
		if (device_is_ready(adc)) {
			ret = adc_channel_setup(adc, &channels[i]);
			if (ret < 0) {
				LOG_ERR("%s: channel %u setup failed (%d)", names[i],
					channels[i].channel_id, ret);
			} else {
				ready[i] = true;
			}
		} else {
			LOG_WRN("%s is not ready", adc->name);
		}

		rows[i] = oled_menu_label_add(&screen, names[i]);
	}

	oled_menu_back_row_add(&screen, home);

	return 0;
}

static bool present(void)
{
	return device_is_ready(adc);
}

static const struct oled_menu_screen *get_screen(void)
{
	return &screen;
}

const struct bringup_module module_analog = {
	.label = "Pot/LDR/Mic",
	.create = create,
	.screen = get_screen,
	.refresh = refresh,
	.present = present,
};
