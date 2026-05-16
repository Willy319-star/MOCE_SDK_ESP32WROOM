#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DRIVER_ENCODER_LEFT = 0,
    DRIVER_ENCODER_RIGHT,
    DRIVER_ENCODER_MAX,
} driver_encoder_id_t;

esp_err_t driver_encoder_init(driver_encoder_id_t encoder);
esp_err_t driver_encoder_get_count(driver_encoder_id_t encoder, int *count);
esp_err_t driver_encoder_reset(driver_encoder_id_t encoder);

#ifdef __cplusplus
}
#endif
