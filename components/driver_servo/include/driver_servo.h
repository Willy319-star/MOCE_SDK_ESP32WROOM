#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DRIVER_SERVO_0 = 0,
    DRIVER_SERVO_1,
    DRIVER_SERVO_MAX,
} driver_servo_id_t;

esp_err_t driver_servo_init(void);
esp_err_t driver_servo_set_angle(driver_servo_id_t servo, uint16_t angle_deg);
esp_err_t driver_servo_set_pulse_us(driver_servo_id_t servo, uint16_t pulse_us);
uint16_t driver_servo_get_angle(driver_servo_id_t servo);
uint16_t driver_servo_get_pulse_us(driver_servo_id_t servo);

#ifdef __cplusplus
}
#endif
