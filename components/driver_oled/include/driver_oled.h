#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DRIVER_OLED_DEFAULT_I2C_ADDR 0x3c
#define DRIVER_OLED_WIDTH            128
#define DRIVER_OLED_HEIGHT           64

typedef enum {
    DRIVER_OLED_CONTROLLER_SSD1306 = 0,
    DRIVER_OLED_CONTROLLER_SH1106,
    DRIVER_OLED_CONTROLLER_SSD1309,
} driver_oled_controller_t;

typedef enum {
    DRIVER_OLED_PROFILE_096_SSD1306 = 0,
    DRIVER_OLED_PROFILE_13_SH1106,
    DRIVER_OLED_PROFILE_242_SSD1309,
} driver_oled_profile_t;

typedef struct {
    driver_oled_controller_t controller;
    uint8_t i2c_address;
    uint32_t scl_speed_hz;
    bool flip_x;
    bool flip_y;
    bool invert;
    uint8_t contrast;
} driver_oled_config_t;

esp_err_t driver_oled_init(const driver_oled_config_t *config);
esp_err_t driver_oled_init_profile(driver_oled_profile_t profile);
esp_err_t driver_oled_deinit(void);
bool driver_oled_is_initialized(void);

esp_err_t driver_oled_set_power(bool on);
esp_err_t driver_oled_set_contrast(uint8_t contrast);
esp_err_t driver_oled_set_invert(bool invert);

void driver_oled_clear(void);
void driver_oled_draw_pixel(int x, int y, bool on);
void driver_oled_draw_hline(int x, int y, int width, bool on);
void driver_oled_draw_vline(int x, int y, int height, bool on);
void driver_oled_draw_rect(int x, int y, int width, int height, bool on);
void driver_oled_fill_rect(int x, int y, int width, int height, bool on);
void driver_oled_draw_char(int x, int y, char ch, bool on);
void driver_oled_draw_string(int x, int y, const char *text, bool on);

esp_err_t driver_oled_flush(void);
esp_err_t driver_oled_clear_screen(void);

uint8_t *driver_oled_get_framebuffer(void);
size_t driver_oled_get_framebuffer_size(void);

#ifdef __cplusplus
}
#endif
