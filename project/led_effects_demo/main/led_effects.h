#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LED_EFFECT_OFF = 0,
    LED_EFFECT_ON,
    LED_EFFECT_BLINK_1HZ,
    LED_EFFECT_BLINK_2HZ,
    LED_EFFECT_BLINK_5HZ,
    LED_EFFECT_BREATHE,
    LED_EFFECT_MAX
} led_effect_t;

void led_effects_init(void);
void led_effects_set(led_effect_t effect);
void led_effects_next(void);
void led_effects_process(void);

#ifdef __cplusplus
}
#endif
