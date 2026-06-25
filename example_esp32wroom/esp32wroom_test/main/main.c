#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include "board.h"
#include "driver/gpio.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "esp32wroom_test";

static const char *reset_reason_to_string(esp_reset_reason_t reason)
{
    switch (reason) {
    case ESP_RST_POWERON:
        return "power-on";
    case ESP_RST_EXT:
        return "external";
    case ESP_RST_SW:
        return "software";
    case ESP_RST_PANIC:
        return "panic";
    case ESP_RST_INT_WDT:
        return "interrupt-watchdog";
    case ESP_RST_TASK_WDT:
        return "task-watchdog";
    case ESP_RST_WDT:
        return "other-watchdog";
    case ESP_RST_DEEPSLEEP:
        return "deep-sleep";
    case ESP_RST_BROWNOUT:
        return "brownout";
    case ESP_RST_SDIO:
        return "sdio";
    default:
        return "unknown";
    }
}

static void init_led(void)
{
    gpio_config_t led_cfg = {
        .pin_bit_mask = 1ULL << BOARD_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&led_cfg));
    ESP_ERROR_CHECK(gpio_set_level(BOARD_LED_GPIO, 0));
}

void app_main(void)
{
    init_led();

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    uint32_t flash_size = 0;
    esp_err_t flash_ret = esp_flash_get_size(NULL, &flash_size);

    ESP_LOGI(TAG, "ESP32-WROOM board serial test started");
    ESP_LOGI(TAG, "Board: my_board_esp32wroom");
    ESP_LOGI(TAG, "LED GPIO=%d, I2C SDA=%d, I2C SCL=%d",
             BOARD_LED_GPIO, BOARD_I2C_SDA_GPIO, BOARD_I2C_SCL_GPIO);
    ESP_LOGI(TAG, "UART1 TX=%d, RX=%d, CAN TX=%d, CAN RX=%d",
             BOARD_UART_TX_GPIO, BOARD_UART_RX_GPIO, BOARD_CAN_TX_GPIO, BOARD_CAN_RX_GPIO);
    ESP_LOGI(TAG, "SPI MOSI=%d, MISO=%d, SCK=%d, CS0=%d, CS1=%d",
             BOARD_SPI_MOSI_GPIO, BOARD_SPI_MISO_GPIO, BOARD_SPI_SCK_GPIO,
             BOARD_SPI_CS0_GPIO, BOARD_SPI_CS1_GPIO);
    ESP_LOGI(TAG, "Chip cores=%d, revision=%d, features=0x%08" PRIx32,
             chip_info.cores, chip_info.revision, chip_info.features);
    ESP_LOGI(TAG, "Reset reason: %s (%d)",
             reset_reason_to_string(esp_reset_reason()), esp_reset_reason());

    if (flash_ret == ESP_OK) {
        ESP_LOGI(TAG, "Flash size: %" PRIu32 " MB", flash_size / (1024U * 1024U));
    } else {
        ESP_LOGW(TAG, "Flash size read failed: %s", esp_err_to_name(flash_ret));
    }

    printf("\r\n==== esp32wroom_test is running ====\r\n");
    printf("Open serial monitor at 115200 baud to see heartbeat logs.\r\n\r\n");

    uint32_t counter = 0;
    bool led_on = false;

    while (1) {
        led_on = !led_on;
        ESP_ERROR_CHECK(gpio_set_level(BOARD_LED_GPIO, led_on ? 1 : 0));

        int64_t uptime_ms = esp_timer_get_time() / 1000;
        ESP_LOGI(TAG, "heartbeat=%" PRIu32 ", uptime=%" PRId64 " ms, LED=%s",
                 counter, uptime_ms, led_on ? "on" : "off");
        printf("serial printf: counter=%" PRIu32 ", uptime=%" PRId64 " ms\r\n",
               counter, uptime_ms);

        counter++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
