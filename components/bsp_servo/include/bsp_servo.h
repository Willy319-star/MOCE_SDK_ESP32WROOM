#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BSP_SERVO_0 = 0,
    BSP_SERVO_1,
    BSP_SERVO_MAX,
} bsp_servo_id_t;

esp_err_t bsp_servo_init(void);
esp_err_t bsp_servo_set_angle(bsp_servo_id_t servo, uint16_t angle_deg);
esp_err_t bsp_servo_set_pulse_us(bsp_servo_id_t servo, uint16_t pulse_us);
uint16_t bsp_servo_get_angle(bsp_servo_id_t servo);
uint16_t bsp_servo_get_pulse_us(bsp_servo_id_t servo);

#ifdef __cplusplus
}
#endif
