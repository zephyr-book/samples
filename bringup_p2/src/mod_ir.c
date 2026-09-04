/**
 * @file mod_ir.c
 * @brief The IR emitter (GPIO6, through Q8) and receiver (GPIO47).
 *
 *        Both are driven as plain GPIOs, the same way the P1 bring-up did it:
 *        switch the emitter on and watch the receiver line. If the fitted
 *        receiver is a demodulating type it will ignore steady light, so a
 *        modulated burst is offered as well -- bit-banged, since GPIO6 is not
 *        routed to a PWM slice on this revision.
 *
 * @copyright Copyright (c) Centro de Inovacao EDGE
 */

#include "module.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mod_ir, LOG_LEVEL_INF);

/* 38 kHz is what consumer IR receivers demodulate: 26 us per cycle. */
#define CARRIER_HALF_PERIOD_US 13
#define BURST_MS               200
#define BURST_CYCLES           ((BURST_MS * 1000U) / (2U * CARRIER_HALF_PERIOD_US))

static const struct gpio_dt_spec emitter = GPIO_DT_SPEC_GET(DT_NODELABEL(ir_emitter), gpios);
static const struct gpio_dt_spec receiver = GPIO_DT_SPEC_GET(DT_NODELABEL(ir_receiver), gpios);

static struct oled_menu_screen screen;
static lv_obj_t *emitter_row;
static lv_obj_t *receiver_label;
static bool emitter_on;

/* Set while a burst runs, so the live refresh reports what the receiver saw
 * during it rather than after it.
 */
static bool burst_seen;

static void emitter_row_update(void)
{
	if (!gpio_is_ready_dt(&emitter)) {
		oled_menu_row_set_text(emitter_row, "Emitter  n/a");
		return;
	}

	oled_menu_row_set_text_fmt(emitter_row, "Emitter  [%c]", emitter_on ? '*' : ' ');
}

static void on_toggle_emitter(lv_event_t *event)
{
	int ret;

	ARG_UNUSED(event);

	if (!gpio_is_ready_dt(&emitter)) {
		return;
	}

	ret = gpio_pin_set_dt(&emitter, emitter_on ? 0 : 1);
	if (ret < 0) {
		LOG_ERR("emitter set failed (%d)", ret);
		return;
	}

	emitter_on = !emitter_on;
	emitter_row_update();
}

/**
 * @brief Bit-bang a 38 kHz burst and record whether the receiver reacted.
 *
 * Runs with interrupts left enabled: a demodulating receiver integrates over
 * many cycles, so the occasional jittered edge does not matter.
 */
static void on_burst(lv_event_t *event)
{
	ARG_UNUSED(event);

	if (!gpio_is_ready_dt(&emitter) || !gpio_is_ready_dt(&receiver)) {
		lv_label_set_text(receiver_label, "RX/TX n/a");
		return;
	}

	burst_seen = false;

	for (uint32_t i = 0; i < BURST_CYCLES; i++) {
		gpio_pin_set_dt(&emitter, 1);
		k_busy_wait(CARRIER_HALF_PERIOD_US);
		gpio_pin_set_dt(&emitter, 0);
		k_busy_wait(CARRIER_HALF_PERIOD_US);

		if (gpio_pin_get_dt(&receiver) > 0) {
			burst_seen = true;
		}
	}

	emitter_on = false;
	emitter_row_update();

	lv_label_set_text(receiver_label, burst_seen ? "burst: SEEN" : "burst: no");
}

static void refresh(void)
{
	int value;

	if (!gpio_is_ready_dt(&receiver)) {
		lv_label_set_text(receiver_label, "RX not ready");
		return;
	}

	value = gpio_pin_get_dt(&receiver);
	if (value < 0) {
		lv_label_set_text_fmt(receiver_label, "RX err %d", value);
		return;
	}

	lv_label_set_text_fmt(receiver_label, "RX %s", value ? "ACTIVE" : "idle");
}

static int create(const struct oled_menu_screen *home)
{
	int ret = oled_menu_screen_create(&screen, "Infrared", OLED_MENU_LAYOUT_LIST);

	if (ret < 0) {
		return ret;
	}

	if (gpio_is_ready_dt(&emitter)) {
		ret = gpio_pin_configure_dt(&emitter, GPIO_OUTPUT_INACTIVE);
		if (ret < 0) {
			LOG_ERR("emitter configure failed (%d)", ret);
		}
	} else {
		LOG_WRN("IR emitter is not ready");
	}

	if (gpio_is_ready_dt(&receiver)) {
		ret = gpio_pin_configure_dt(&receiver, GPIO_INPUT);
		if (ret < 0) {
			LOG_ERR("receiver configure failed (%d)", ret);
		}
	} else {
		LOG_WRN("IR receiver is not ready");
	}

	receiver_label = oled_menu_label_add(&screen, "RX idle");
	emitter_row = oled_menu_row_add(&screen, "", on_toggle_emitter, NULL);
	emitter_row_update();
	oled_menu_row_add(&screen, "38kHz burst", on_burst, NULL);
	oled_menu_back_row_add(&screen, home);

	return 0;
}

static const struct oled_menu_screen *get_screen(void)
{
	return &screen;
}

const struct bringup_module module_ir = {
	.label = "Infrared",
	.create = create,
	.screen = get_screen,
	.refresh = refresh,
};
