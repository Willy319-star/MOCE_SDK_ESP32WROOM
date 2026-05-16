#include "driver_motor.h"

#include <stdbool.h>

#include "driver_tb6612.h"
#include "driver_encoder.h"
#include "esp_log.h"

static const char *TAG = "driver_motor";

static uint16_t s_ppr[DRIVER_MOTOR_MAX] = {
    DRIVER_MOTOR_PPR_DEFAULT,
    DRIVER_MOTOR_PPR_DEFAULT,
};

static bool s_inited[DRIVER_MOTOR_MAX] = {false, false};

static driver_tb6612_motor_t motor_to_tb6612(driver_motor_id_t motor)
{
    return (driver_tb6612_motor_t)motor;
}

static driver_encoder_id_t motor_to_encoder(driver_motor_id_t motor)
{
    return (driver_encoder_id_t)motor;
}

static bool motor_id_is_valid(driver_motor_id_t motor)
{
    return motor >= DRIVER_MOTOR_LEFT && motor < DRIVER_MOTOR_MAX;
}

esp_err_t driver_motor_init(driver_motor_id_t motor, const driver_motor_config_t *config)
{
    if (!motor_id_is_valid(motor)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (config != NULL) {
        s_ppr[motor] = config->ppr;
    }

    esp_err_t err = driver_tb6612_init(motor_to_tb6612(motor));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "tb6612 init failed for motor %d", motor);
        return err;
    }

    err = driver_encoder_init(motor_to_encoder(motor));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "encoder init failed for motor %d", motor);
        return err;
    }

    s_inited[motor] = true;
    ESP_LOGI(TAG, "motor %d initialized (PPR=%u)", motor, s_ppr[motor]);
    return ESP_OK;
}

esp_err_t driver_motor_set_speed(driver_motor_id_t motor, int16_t speed)
{
    if (!motor_id_is_valid(motor) || !s_inited[motor]) {
        return ESP_ERR_INVALID_ARG;
    }

    return driver_tb6612_set_speed(motor_to_tb6612(motor), speed);
}

esp_err_t driver_motor_set_pwm_raw(driver_motor_id_t motor, uint32_t duty)
{
    if (!motor_id_is_valid(motor) || !s_inited[motor]) {
        return ESP_ERR_INVALID_ARG;
    }

    return driver_tb6612_set_pwm_raw(motor_to_tb6612(motor), duty);
}

esp_err_t driver_motor_get_encoder_count(driver_motor_id_t motor, int *count)
{
    if (!motor_id_is_valid(motor) || !s_inited[motor]) {
        return ESP_ERR_INVALID_ARG;
    }

    return driver_encoder_get_count(motor_to_encoder(motor), count);
}

esp_err_t driver_motor_reset_encoder(driver_motor_id_t motor)
{
    if (!motor_id_is_valid(motor) || !s_inited[motor]) {
        return ESP_ERR_INVALID_ARG;
    }

    return driver_encoder_reset(motor_to_encoder(motor));
}

esp_err_t driver_motor_brake(driver_motor_id_t motor)
{
    if (!motor_id_is_valid(motor) || !s_inited[motor]) {
        return ESP_ERR_INVALID_ARG;
    }

    return driver_tb6612_brake(motor_to_tb6612(motor));
}

esp_err_t driver_motor_coast(driver_motor_id_t motor)
{
    if (!motor_id_is_valid(motor) || !s_inited[motor]) {
        return ESP_ERR_INVALID_ARG;
    }

    return driver_tb6612_coast(motor_to_tb6612(motor));
}

uint16_t driver_motor_get_ppr(driver_motor_id_t motor)
{
    if (!motor_id_is_valid(motor)) {
        return 0;
    }

    return s_ppr[motor];
}
