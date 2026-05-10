#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void driver_led_init(void);
void driver_led_set(int on);
void driver_led_toggle(void);

/* 亮度百分比: 0 ~ 100 */
void driver_led_set_brightness(uint8_t percent);

/* 原始 PWM 占空比 */
void driver_led_set_duty(uint32_t duty);

/* 查询当前亮度百分比 */
uint8_t driver_led_get_brightness(void);

#ifdef __cplusplus
}
#endif