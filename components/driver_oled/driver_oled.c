#include "driver_oled.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "bsp_i2c.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "driver_oled";

#define OLED_PAGES                  (DRIVER_OLED_HEIGHT / 8)
#define OLED_FB_SIZE                (DRIVER_OLED_WIDTH * OLED_PAGES)
#define OLED_CONTROL_CMD            0x00
#define OLED_CONTROL_DATA           0x40
#define OLED_DEFAULT_SPEED_HZ       400000
#define OLED_SH1106_COLUMN_OFFSET   2
#define OLED_DATA_CHUNK_SIZE        16

static i2c_master_dev_handle_t s_dev = NULL;
static driver_oled_config_t s_config = {0};
static bool s_inited = false;
static uint8_t s_framebuffer[OLED_FB_SIZE];

static const uint8_t s_font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x5f,0x00,0x00}, {0x00,0x07,0x00,0x07,0x00}, {0x14,0x7f,0x14,0x7f,0x14},
    {0x24,0x2a,0x7f,0x2a,0x12}, {0x23,0x13,0x08,0x64,0x62}, {0x36,0x49,0x55,0x22,0x50}, {0x00,0x05,0x03,0x00,0x00},
    {0x00,0x1c,0x22,0x41,0x00}, {0x00,0x41,0x22,0x1c,0x00}, {0x14,0x08,0x3e,0x08,0x14}, {0x08,0x08,0x3e,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00}, {0x08,0x08,0x08,0x08,0x08}, {0x00,0x60,0x60,0x00,0x00}, {0x20,0x10,0x08,0x04,0x02},
    {0x3e,0x51,0x49,0x45,0x3e}, {0x00,0x42,0x7f,0x40,0x00}, {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4b,0x31},
    {0x18,0x14,0x12,0x7f,0x10}, {0x27,0x45,0x45,0x45,0x39}, {0x3c,0x4a,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1e}, {0x00,0x36,0x36,0x00,0x00}, {0x00,0x56,0x36,0x00,0x00},
    {0x08,0x14,0x22,0x41,0x00}, {0x14,0x14,0x14,0x14,0x14}, {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x51,0x09,0x06},
    {0x32,0x49,0x79,0x41,0x3e}, {0x7e,0x11,0x11,0x11,0x7e}, {0x7f,0x49,0x49,0x49,0x36}, {0x3e,0x41,0x41,0x41,0x22},
    {0x7f,0x41,0x41,0x22,0x1c}, {0x7f,0x49,0x49,0x49,0x41}, {0x7f,0x09,0x09,0x09,0x01}, {0x3e,0x41,0x49,0x49,0x7a},
    {0x7f,0x08,0x08,0x08,0x7f}, {0x00,0x41,0x7f,0x41,0x00}, {0x20,0x40,0x41,0x3f,0x01}, {0x7f,0x08,0x14,0x22,0x41},
    {0x7f,0x40,0x40,0x40,0x40}, {0x7f,0x02,0x0c,0x02,0x7f}, {0x7f,0x04,0x08,0x10,0x7f}, {0x3e,0x41,0x41,0x41,0x3e},
    {0x7f,0x09,0x09,0x09,0x06}, {0x3e,0x41,0x51,0x21,0x5e}, {0x7f,0x09,0x19,0x29,0x46}, {0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7f,0x01,0x01}, {0x3f,0x40,0x40,0x40,0x3f}, {0x1f,0x20,0x40,0x20,0x1f}, {0x3f,0x40,0x38,0x40,0x3f},
    {0x63,0x14,0x08,0x14,0x63}, {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}, {0x00,0x7f,0x41,0x41,0x00},
    {0x02,0x04,0x08,0x10,0x20}, {0x00,0x41,0x41,0x7f,0x00}, {0x04,0x02,0x01,0x02,0x04}, {0x40,0x40,0x40,0x40,0x40},
    {0x00,0x01,0x02,0x04,0x00}, {0x20,0x54,0x54,0x54,0x78}, {0x7f,0x48,0x44,0x44,0x38}, {0x38,0x44,0x44,0x44,0x20},
    {0x38,0x44,0x44,0x48,0x7f}, {0x38,0x54,0x54,0x54,0x18}, {0x08,0x7e,0x09,0x01,0x02}, {0x0c,0x52,0x52,0x52,0x3e},
    {0x7f,0x08,0x04,0x04,0x78}, {0x00,0x44,0x7d,0x40,0x00}, {0x20,0x40,0x44,0x3d,0x00}, {0x7f,0x10,0x28,0x44,0x00},
    {0x00,0x41,0x7f,0x40,0x00}, {0x7c,0x04,0x18,0x04,0x78}, {0x7c,0x08,0x04,0x04,0x78}, {0x38,0x44,0x44,0x44,0x38},
    {0x7c,0x14,0x14,0x14,0x08}, {0x08,0x14,0x14,0x18,0x7c}, {0x7c,0x08,0x04,0x04,0x08}, {0x48,0x54,0x54,0x54,0x20},
    {0x04,0x3f,0x44,0x40,0x20}, {0x3c,0x40,0x40,0x20,0x7c}, {0x1c,0x20,0x40,0x20,0x1c}, {0x3c,0x40,0x30,0x40,0x3c},
    {0x44,0x28,0x10,0x28,0x44}, {0x0c,0x50,0x50,0x50,0x3c}, {0x44,0x64,0x54,0x4c,0x44}, {0x00,0x08,0x36,0x41,0x00},
    {0x00,0x00,0x7f,0x00,0x00}, {0x00,0x41,0x36,0x08,0x00}, {0x10,0x08,0x08,0x10,0x08}, {0x00,0x06,0x09,0x09,0x06},
};

static esp_err_t oled_write_bytes(uint8_t control, const uint8_t *data, size_t len)
{
    if (data == NULL || len == 0 || len > OLED_DATA_CHUNK_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buffer[1 + OLED_DATA_CHUNK_SIZE];
    buffer[0] = control;
    memcpy(&buffer[1], data, len);
    return bsp_i2c_write(s_dev, buffer, len + 1, -1);
}

static esp_err_t oled_write_cmd(uint8_t cmd)
{
    return oled_write_bytes(OLED_CONTROL_CMD, &cmd, 1);
}

static esp_err_t oled_write_cmds(const uint8_t *cmds, size_t len)
{
    esp_err_t err = ESP_OK;

    for (size_t i = 0; i < len; i++) {
        err = oled_write_cmd(cmds[i]);
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}

static driver_oled_config_t oled_profile_config(driver_oled_profile_t profile)
{
    driver_oled_config_t config = {
        .controller = DRIVER_OLED_CONTROLLER_SSD1306,
        .i2c_address = DRIVER_OLED_DEFAULT_I2C_ADDR,
        .scl_speed_hz = OLED_DEFAULT_SPEED_HZ,
        .flip_x = false,
        .flip_y = false,
        .invert = false,
        .contrast = 0x7f,
    };

    if (profile == DRIVER_OLED_PROFILE_13_SH1106) {
        config.controller = DRIVER_OLED_CONTROLLER_SH1106;
        config.contrast = 0x80;
    }

    return config;
}

static esp_err_t oled_init_controller(void)
{
    const uint8_t common_init[] = {
        0xae,
        0xd5, 0x80,
        0xa8, 0x3f,
        0xd3, 0x00,
        0x40,
        0x8d, 0x14,
        0xda, 0x12,
        0x81, s_config.contrast,
        0xd9, 0xf1,
        0xdb, 0x40,
        0xa4,
        0xa6,
        0xaf,
    };
    const uint8_t ssd1306_mode[] = {
        0x20, 0x02,
        0x21, 0x00, 0x7f,
        0x22, 0x00, 0x07,
    };

    ESP_RETURN_ON_ERROR(oled_write_cmds(common_init, sizeof(common_init)), TAG, "common init failed");

    if (s_config.controller == DRIVER_OLED_CONTROLLER_SSD1306) {
        ESP_RETURN_ON_ERROR(oled_write_cmds(ssd1306_mode, sizeof(ssd1306_mode)), TAG, "ssd1306 mode failed");
    }

    ESP_RETURN_ON_ERROR(oled_write_cmd(s_config.flip_x ? 0xa0 : 0xa1), TAG, "set segment remap failed");
    ESP_RETURN_ON_ERROR(oled_write_cmd(s_config.flip_y ? 0xc0 : 0xc8), TAG, "set com scan failed");
    return driver_oled_set_invert(s_config.invert);
}

esp_err_t driver_oled_init(const driver_oled_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_inited) {
        ESP_RETURN_ON_ERROR(driver_oled_deinit(), TAG, "deinit old oled failed");
    }

    s_config = *config;
    if (s_config.i2c_address == 0) {
        s_config.i2c_address = DRIVER_OLED_DEFAULT_I2C_ADDR;
    }
    if (s_config.scl_speed_hz == 0) {
        s_config.scl_speed_hz = OLED_DEFAULT_SPEED_HZ;
    }

    ESP_RETURN_ON_ERROR(bsp_i2c_add_device_7bit(s_config.i2c_address, s_config.scl_speed_hz, &s_dev), TAG, "add oled device failed");
    esp_err_t err = oled_init_controller();
    if (err != ESP_OK) {
        bsp_i2c_remove_device(s_dev);
        s_dev = NULL;
        return err;
    }

    s_inited = true;
    driver_oled_clear();
    ESP_RETURN_ON_ERROR(driver_oled_flush(), TAG, "clear oled failed");

    ESP_LOGI(TAG, "OLED initialized: %s, addr 0x%02x",
             s_config.controller == DRIVER_OLED_CONTROLLER_SH1106 ? "SH1106 1.3" : "SSD1306 0.96",
             s_config.i2c_address);
    return ESP_OK;
}

esp_err_t driver_oled_init_profile(driver_oled_profile_t profile)
{
    if (profile != DRIVER_OLED_PROFILE_096_SSD1306 && profile != DRIVER_OLED_PROFILE_13_SH1106) {
        return ESP_ERR_INVALID_ARG;
    }

    driver_oled_config_t config = oled_profile_config(profile);
    return driver_oled_init(&config);
}

esp_err_t driver_oled_deinit(void)
{
    if (!s_inited) {
        return ESP_OK;
    }

    esp_err_t err = driver_oled_set_power(false);
    if (err != ESP_OK) {
        return err;
    }

    err = bsp_i2c_remove_device(s_dev);
    if (err != ESP_OK) {
        return err;
    }

    s_dev = NULL;
    s_inited = false;
    memset(s_framebuffer, 0, sizeof(s_framebuffer));
    return ESP_OK;
}

bool driver_oled_is_initialized(void)
{
    return s_inited;
}

esp_err_t driver_oled_set_power(bool on)
{
    if (s_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return oled_write_cmd(on ? 0xaf : 0xae);
}

esp_err_t driver_oled_set_contrast(uint8_t contrast)
{
    if (s_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    s_config.contrast = contrast;
    ESP_RETURN_ON_ERROR(oled_write_cmd(0x81), TAG, "contrast command failed");
    return oled_write_cmd(contrast);
}

esp_err_t driver_oled_set_invert(bool invert)
{
    if (s_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    s_config.invert = invert;
    return oled_write_cmd(invert ? 0xa7 : 0xa6);
}

void driver_oled_clear(void)
{
    memset(s_framebuffer, 0, sizeof(s_framebuffer));
}

void driver_oled_draw_pixel(int x, int y, bool on)
{
    if (x < 0 || x >= DRIVER_OLED_WIDTH || y < 0 || y >= DRIVER_OLED_HEIGHT) {
        return;
    }

    uint8_t mask = (uint8_t)(1U << (y & 0x07));
    uint8_t *pixel = &s_framebuffer[(y / 8) * DRIVER_OLED_WIDTH + x];
    if (on) {
        *pixel |= mask;
    } else {
        *pixel &= (uint8_t)~mask;
    }
}

void driver_oled_draw_hline(int x, int y, int width, bool on)
{
    for (int i = 0; i < width; i++) {
        driver_oled_draw_pixel(x + i, y, on);
    }
}

void driver_oled_draw_vline(int x, int y, int height, bool on)
{
    for (int i = 0; i < height; i++) {
        driver_oled_draw_pixel(x, y + i, on);
    }
}

void driver_oled_draw_rect(int x, int y, int width, int height, bool on)
{
    if (width <= 0 || height <= 0) {
        return;
    }

    driver_oled_draw_hline(x, y, width, on);
    driver_oled_draw_hline(x, y + height - 1, width, on);
    driver_oled_draw_vline(x, y, height, on);
    driver_oled_draw_vline(x + width - 1, y, height, on);
}

void driver_oled_fill_rect(int x, int y, int width, int height, bool on)
{
    for (int row = 0; row < height; row++) {
        driver_oled_draw_hline(x, y + row, width, on);
    }
}

void driver_oled_draw_char(int x, int y, char ch, bool on)
{
    if (ch < 0x20 || ch > 0x7f) {
        ch = '?';
    }

    const uint8_t *glyph = s_font5x7[(uint8_t)ch - 0x20];
    for (int col = 0; col < 5; col++) {
        uint8_t bits = glyph[col];
        for (int row = 0; row < 7; row++) {
            if ((bits & (1U << row)) != 0) {
                driver_oled_draw_pixel(x + col, y + row, on);
            }
        }
    }
}

void driver_oled_draw_string(int x, int y, const char *text, bool on)
{
    if (text == NULL) {
        return;
    }

    int cursor_x = x;
    while (*text != '\0') {
        if (*text == '\n') {
            cursor_x = x;
            y += 8;
        } else {
            driver_oled_draw_char(cursor_x, y, *text, on);
            cursor_x += 6;
        }
        text++;
    }
}

esp_err_t driver_oled_flush(void)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    for (uint8_t page = 0; page < OLED_PAGES; page++) {
        uint8_t column_offset = s_config.controller == DRIVER_OLED_CONTROLLER_SH1106 ? OLED_SH1106_COLUMN_OFFSET : 0;
        uint8_t page_cmds[] = {
            (uint8_t)(0xb0 | page),
            (uint8_t)(0x00 | (column_offset & 0x0f)),
            (uint8_t)(0x10 | (column_offset >> 4)),
        };

        ESP_RETURN_ON_ERROR(oled_write_cmds(page_cmds, sizeof(page_cmds)), TAG, "set page failed");

        const uint8_t *page_data = &s_framebuffer[page * DRIVER_OLED_WIDTH];
        for (size_t col = 0; col < DRIVER_OLED_WIDTH; col += OLED_DATA_CHUNK_SIZE) {
            ESP_RETURN_ON_ERROR(oled_write_bytes(OLED_CONTROL_DATA, &page_data[col], OLED_DATA_CHUNK_SIZE), TAG, "write data failed");
        }
    }
    return ESP_OK;
}

esp_err_t driver_oled_clear_screen(void)
{
    driver_oled_clear();
    return driver_oled_flush();
}

uint8_t *driver_oled_get_framebuffer(void)
{
    return s_framebuffer;
}

size_t driver_oled_get_framebuffer_size(void)
{
    return sizeof(s_framebuffer);
}
