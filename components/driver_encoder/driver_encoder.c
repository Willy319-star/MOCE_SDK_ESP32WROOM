#include "driver_encoder.h"

#include <stdbool.h>
#include <stddef.h>

#include "board.h"
#include "driver/pulse_cnt.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "driver_encoder";

static const int s_encoder_a_gpios[DRIVER_ENCODER_MAX] = {
    BOARD_ENCODER_LEFT_A_GPIO,
    BOARD_ENCODER_RIGHT_A_GPIO,
};

static const int s_encoder_b_gpios[DRIVER_ENCODER_MAX] = {
    BOARD_ENCODER_LEFT_B_GPIO,
    BOARD_ENCODER_RIGHT_B_GPIO,
};

static pcnt_unit_handle_t s_pcnt_units[DRIVER_ENCODER_MAX] = {NULL, NULL};
static pcnt_channel_handle_t s_pcnt_channels[DRIVER_ENCODER_MAX] = {NULL, NULL};
static bool s_inited[DRIVER_ENCODER_MAX] = {false, false};

#define ENCODER_PCNT_LOW_LIMIT  (-20000)
#define ENCODER_PCNT_HIGH_LIMIT  20000

static bool encoder_id_is_valid(driver_encoder_id_t encoder)
{
    return encoder >= DRIVER_ENCODER_LEFT && encoder < DRIVER_ENCODER_MAX;
}

esp_err_t driver_encoder_init(driver_encoder_id_t encoder)
{
    if (!encoder_id_is_valid(encoder)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_inited[encoder]) {
        return ESP_OK;
    }

    pcnt_unit_config_t unit_config = {
        .low_limit = ENCODER_PCNT_LOW_LIMIT,
        .high_limit = ENCODER_PCNT_HIGH_LIMIT,
        .intr_priority = 0,
    };

    esp_err_t err = pcnt_new_unit(&unit_config, &s_pcnt_units[encoder]);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "pcnt new unit failed for encoder %d: %s", encoder, esp_err_to_name(err));
        return err;
    }

    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,
    };
    err = pcnt_unit_set_glitch_filter(s_pcnt_units[encoder], &filter_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "pcnt set glitch filter failed: %s", esp_err_to_name(err));
        return err;
    }

    pcnt_chan_config_t chan_config = {
        .edge_gpio_num = s_encoder_a_gpios[encoder],
        .level_gpio_num = s_encoder_b_gpios[encoder],
    };

    err = pcnt_new_channel(s_pcnt_units[encoder], &chan_config, &s_pcnt_channels[encoder]);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "pcnt new channel failed: %s", esp_err_to_name(err));
        return err;
    }

    err = pcnt_channel_set_edge_action(s_pcnt_channels[encoder],
                                       PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                       PCNT_CHANNEL_EDGE_ACTION_DECREASE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "pcnt set edge action failed: %s", esp_err_to_name(err));
        return err;
    }

    err = pcnt_channel_set_level_action(s_pcnt_channels[encoder],
                                        PCNT_CHANNEL_LEVEL_ACTION_INVERSE,
                                        PCNT_CHANNEL_LEVEL_ACTION_KEEP);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "pcnt set level action failed: %s", esp_err_to_name(err));
        return err;
    }

    err = pcnt_unit_enable(s_pcnt_units[encoder]);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "pcnt enable failed: %s", esp_err_to_name(err));
        return err;
    }

    err = pcnt_unit_start(s_pcnt_units[encoder]);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "pcnt start failed: %s", esp_err_to_name(err));
        return err;
    }

    s_inited[encoder] = true;
    ESP_LOGI(TAG, "encoder %d initialized (A: GPIO %d, B: GPIO %d)",
             encoder, s_encoder_a_gpios[encoder], s_encoder_b_gpios[encoder]);
    return ESP_OK;
}

esp_err_t driver_encoder_get_count(driver_encoder_id_t encoder, int *count)
{
    if (!encoder_id_is_valid(encoder) || count == NULL || !s_inited[encoder]) {
        return ESP_ERR_INVALID_ARG;
    }

    return pcnt_unit_get_count(s_pcnt_units[encoder], count);
}

esp_err_t driver_encoder_reset(driver_encoder_id_t encoder)
{
    if (!encoder_id_is_valid(encoder) || !s_inited[encoder]) {
        return ESP_ERR_INVALID_ARG;
    }

    return pcnt_unit_clear_count(s_pcnt_units[encoder]);
}
