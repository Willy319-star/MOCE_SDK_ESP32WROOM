#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void bsp_led_init(void);
void bsp_led_set(int on);
void bsp_led_toggle(void);

/* 亮度百分比: 0 ~ 100 */
void bsp_led_set_brightness(uint8_t percent);

/* 原始 PWM 占空比 */
void bsp_led_set_duty(uint32_t duty);

/* 查询当前亮度百分比 */
uint8_t bsp_led_get_brightness(void);

#ifdef __cplusplus
}
#endif