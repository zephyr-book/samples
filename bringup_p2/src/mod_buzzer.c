/**
 * @file mod_buzzer.c
 * @brief The piezo buzzer on PWM slice 8 (GPIO32, through Q7).
 *
 *        Slice 8 runs with divider 255, so the PWM clock is clk_sys/255 and
 *        the 16-bit counter reaches down to about 9 Hz -- the tones below are
 *        all comfortably inside that range.
 *
 * @copyright Copyright (c) Centro de Inovacao EDGE
 */

#include "module.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/pwm.h>
#include <stdbool.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mod_buzzer, LOG_LEVEL_INF);

static const struct pwm_dt_spec buzzer = PWM_DT_SPEC_GET(DT_ALIAS(pwm_buzzer));

static struct oled_menu_screen screen;
static lv_obj_t *status;

static const uint32_t tones_hz[] = {440, 880, 1760, 3520};
static const char *const tone_names[] = {"440 Hz", "880 Hz", "1760 Hz", "3520 Hz"};

static void tone_set(uint32_t hz)
{
	int ret;
	uint32_t period_ns;

	if (!pwm_is_ready_dt(&buzzer)) {
		lv_label_set_text(status, "not ready");
		return;
	}

	if (hz == 0) {
		/* Zero pulse rather than zero period: a zero period is not a
		 * valid PWM setting.
		 */
		ret = pwm_set_dt(&buzzer, buzzer.period, 0);
		if (ret < 0) {
			LOG_ERR("stop failed (%d)", ret);
			lv_label_set_text_fmt(status, "error %d", ret);
			return;
		}

		lv_label_set_text(status, "silent");
		return;
	}

	period_ns = NSEC_PER_SEC / hz;

	/* Square wave: the piezo is loudest at 50% duty. */
	ret = pwm_set_dt(&buzzer, period_ns, period_ns / 2U);
	if (ret < 0) {
		LOG_ERR("%u Hz failed (%d)", hz, ret);
		lv_label_set_text_fmt(status, "error %d", ret);
		return;
	}

	lv_label_set_text_fmt(status, "%u Hz", hz);
}

static void on_tone(lv_event_t *event)
{
	tone_set(tones_hz[(size_t)(uintptr_t)lv_event_get_user_data(event)]);
}

static void on_silence(lv_event_t *event)
{
	ARG_UNUSED(event);

	tone_set(0);
}

/** Short rising sweep: one press, and a working buzzer is unmistakable. */
static void on_sweep(lv_event_t *event)
{
	ARG_UNUSED(event);

	for (size_t i = 0; i < ARRAY_SIZE(tones_hz); i++) {
		tone_set(tones_hz[i]);
		k_msleep(150);
	}

	tone_set(0);
	lv_label_set_text(status, "sweep done");
}

static int create(const struct oled_menu_screen *home)
{
	int ret = oled_menu_screen_create(&screen, "Buzzer", OLED_MENU_LAYOUT_LIST);

	if (ret < 0) {
		return ret;
	}

	if (!pwm_is_ready_dt(&buzzer)) {
		LOG_WRN("PWM for the buzzer is not ready");
	}

	status = oled_menu_label_add(&screen, pwm_is_ready_dt(&buzzer) ? "silent" : "not ready");

	for (size_t i = 0; i < ARRAY_SIZE(tones_hz); i++) {
		oled_menu_row_add(&screen, tone_names[i], on_tone, (void *)(uintptr_t)i);
	}

	oled_menu_row_add(&screen, "Sweep", on_sweep, NULL);
	oled_menu_row_add(&screen, "Silence", on_silence, NULL);
	oled_menu_back_row_add(&screen, home);

	return 0;
}

static bool present(void)
{
	return pwm_is_ready_dt(&buzzer);
}

static const struct oled_menu_screen *get_screen(void)
{
	return &screen;
}

const struct bringup_module module_buzzer = {
	.label = "Buzzer",
	.create = create,
	.screen = get_screen,
	.refresh = NULL,
	.present = present,
};
