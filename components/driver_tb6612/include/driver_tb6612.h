#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DRIVER_TB6612_MOTOR_LEFT = 0,
    DRIVER_TB6612_MOTOR_RIGHT,
    DRIVER_TB6612_MOTOR_MAX,
} driver_tb6612_motor_t;

esp_err_t driver_tb6612_init(driver_tb6612_motor_t motor);
esp_err_t driver_tb6612_set_speed(driver_tb6612_motor_t motor, int16_t speed);
esp_err_t driver_tb6612_set_pwm_raw(driver_tb6612_motor_t motor, uint32_t duty);
esp_err_t driver_tb6612_brake(driver_tb6612_motor_t motor);
esp_err_t driver_tb6612_coast(driver_tb6612_motor_t motor);

#ifdef __cplusplus
}
#endif
