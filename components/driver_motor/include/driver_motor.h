#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DRIVER_MOTOR_LEFT = 0,
    DRIVER_MOTOR_RIGHT,
    DRIVER_MOTOR_MAX,
} driver_motor_id_t;

#define DRIVER_MOTOR_PPR_DEFAULT 1008

typedef struct {
    uint16_t ppr;
} driver_motor_config_t;

esp_err_t driver_motor_init(driver_motor_id_t motor, const driver_motor_config_t *config);
esp_err_t driver_motor_set_speed(driver_motor_id_t motor, int16_t speed);
esp_err_t driver_motor_set_pwm_raw(driver_motor_id_t motor, uint32_t duty);
esp_err_t driver_motor_get_encoder_count(driver_motor_id_t motor, int *count);
esp_err_t driver_motor_reset_encoder(driver_motor_id_t motor);
esp_err_t driver_motor_brake(driver_motor_id_t motor);
esp_err_t driver_motor_coast(driver_motor_id_t motor);
uint16_t driver_motor_get_ppr(driver_motor_id_t motor);

#ifdef __cplusplus
}
#endif
