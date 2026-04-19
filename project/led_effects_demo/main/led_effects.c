#include "led_effects.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "bsp_led.h"

#define LED_BREATH_STEP_PERCENT    2U
#define LED_BREATH_STEP_DELAY_MS   25U
#define LED_BREATH_EDGE_HOLD_MS    120U

static void led_effects_delay_ms(uint32_t delay_ms)
{
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

const char *led_effects_mode_name(led_effect_mode_t mode)
{
    switch (mode) {
    case LED_EFFECT_SOLID_ON:
        return "solid on";
    case LED_EFFECT_SOLID_OFF:
        return "solid off";
    case LED_EFFECT_BLINK:
        return "blink";
    case LED_EFFECT_BREATH:
        return "breath";
    default:
        return "unknown";
    }
}

void led_effects_apply_solid(int on, uint32_t hold_ms)
{
    bsp_led_set(on);
    led_effects_delay_ms(hold_ms);
}

void led_effects_blink(led_blink_freq_t freq_hz, uint32_t duration_ms)
{
    uint32_t frequency = (uint32_t)freq_hz;
    if (frequency == 0U) {
        frequency = (uint32_t)LED_BLINK_FREQ_1HZ;
    }

    uint32_t half_period_ms = 1000U / (frequency * 2U);
    uint32_t elapsed_ms = 0U;
    int led_on = 0;

    while (elapsed_ms < duration_ms) {
        led_on = !led_on;
        bsp_led_set(led_on);
        led_effects_delay_ms(half_period_ms);
        elapsed_ms += half_period_ms;
    }

    bsp_led_set(0);
}

void led_effects_breath(uint32_t duration_ms)
{
    uint32_t elapsed_ms = 0U;

    while (elapsed_ms < duration_ms) {
        for (uint8_t brightness = 0; brightness <= 100; brightness += LED_BREATH_STEP_PERCENT) {
            bsp_led_set_brightness(brightness);
            led_effects_delay_ms(LED_BREATH_STEP_DELAY_MS);
            elapsed_ms += LED_BREATH_STEP_DELAY_MS;
            if (elapsed_ms >= duration_ms || brightness == 100) {
                break;
            }
        }

        if (elapsed_ms >= duration_ms) {
            break;
        }

        led_effects_delay_ms(LED_BREATH_EDGE_HOLD_MS);
        elapsed_ms += LED_BREATH_EDGE_HOLD_MS;

        for (int brightness = 100; brightness >= 0; brightness -= LED_BREATH_STEP_PERCENT) {
            bsp_led_set_brightness((uint8_t)brightness);
            led_effects_delay_ms(LED_BREATH_STEP_DELAY_MS);
            elapsed_ms += LED_BREATH_STEP_DELAY_MS;
            if (elapsed_ms >= duration_ms || brightness == 0) {
                break;
            }
        }

        if (elapsed_ms >= duration_ms) {
            break;
        }

        led_effects_delay_ms(LED_BREATH_EDGE_HOLD_MS);
        elapsed_ms += LED_BREATH_EDGE_HOLD_MS;
    }
}
