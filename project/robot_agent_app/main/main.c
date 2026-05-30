#include <stdio.h>
#include <string.h>
#include <math.h>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board.h"
#include "driver_button.h"
#include "driver_encoder.h"
#include "driver_motor.h"
#include "driver_mpu6050.h"
#include "driver_oled.h"
#include "driver_tb6612.h"
#include "driver_tof2000c_vl53l0x.h"
#include "driver_tw_tts.h"
#include "service_device.h"
#include "service_pid.h"

static const char *TAG = "robot_agent_app";

typedef enum {
    APP_STATE_INIT = 0,
    APP_STATE_SELF_CHECK,
    APP_STATE_IDLE,
    APP_STATE_INTERACT,
    APP_STATE_MOVE,
    APP_STATE_AVOID,
    APP_STATE_PROTECT,
    APP_STATE_ERROR,
} app_state_t;

typedef struct {
    bool init_ok;
    bool tof_ok;
    bool mpu_ok;
    bool enc_ok;
    bool motor_ok;
    bool tts_ok;
    bool oled_ok;
    bool obstacle_near;
    bool user_near;
    bool pose_safe;
    bool lifted;
    bool moving;
    bool protecting;
    uint16_t distance_mm;
    float pitch_deg;
    float roll_deg;
    int32_t left_count;
    int32_t right_count;
    int16_t left_speed_cmd;
    int16_t right_speed_cmd;
    app_state_t state;
    app_state_t last_state;
    int64_t last_telemetry_ms;
    int64_t last_speech_ms;
    int64_t last_button_ms;
    service_pid_t pid_left;
    service_pid_t pid_right;
} app_ctx_t;

static app_ctx_t g_app;

static const char *state_to_string(app_state_t state)
{
    switch (state) {
    case APP_STATE_INIT: return "INIT";
    case APP_STATE_SELF_CHECK: return "SELF_CHECK";
    case APP_STATE_IDLE: return "IDLE";
    case APP_STATE_INTERACT: return "INTERACT";
    case APP_STATE_MOVE: return "MOVE";
    case APP_STATE_AVOID: return "AVOID";
    case APP_STATE_PROTECT: return "PROTECT";
    case APP_STATE_ERROR: return "ERROR";
    default: return "UNKNOWN";
    }
}

static void oled_draw_status(const app_ctx_t *ctx)
{
    char line1[32];
    char line2[32];
    char line3[32];
    char line4[32];
    char line5[32];

    snprintf(line1, sizeof(line1), "ST:%s", state_to_string(ctx->state));
    snprintf(line2, sizeof(line2), "TOF:%umm", (unsigned)ctx->distance_mm);
    snprintf(line3, sizeof(line3), "P:%.1f R:%.1f", (double)ctx->pitch_deg, (double)ctx->roll_deg);
    snprintf(line4, sizeof(line4), "L:%ld R:%ld", (long)ctx->left_count, (long)ctx->right_count);
    snprintf(line5, sizeof(line5), "M:%d/%d", (int)ctx->left_speed_cmd, (int)ctx->right_speed_cmd);

    driver_oled_clear();
    driver_oled_draw_string(0, 0, line1, true);
    driver_oled_draw_string(0, 12, line2, true);
    driver_oled_draw_string(0, 24, line3, true);
    driver_oled_draw_string(0, 36, line4, true);
    driver_oled_draw_string(0, 48, line5, true);
    (void)driver_oled_flush();
}

static void speak_once_on_state_change(app_ctx_t *ctx, app_state_t new_state)
{
    if (ctx->last_state == new_state) {
        return;
    }

    ctx->last_state = new_state;

    switch (new_state) {
    case APP_STATE_INIT:
        (void)driver_tw_tts_speak("系统初始化");
        break;
    case APP_STATE_SELF_CHECK:
        (void)driver_tw_tts_speak("开始自检");
        break;
    case APP_STATE_IDLE:
        (void)driver_tw_tts_speak("进入待机");
        break;
    case APP_STATE_INTERACT:
        (void)driver_tw_tts_speak("检测到用户靠近");
        break;
    case APP_STATE_MOVE:
        (void)driver_tw_tts_speak("进入移动状态");
        break;
    case APP_STATE_AVOID:
        (void)driver_tw_tts_speak("前方有障碍，开始避障");
        break;
    case APP_STATE_PROTECT:
        (void)driver_tw_tts_speak("进入保护状态");
        break;
    case APP_STATE_ERROR:
        (void)driver_tw_tts_speak("系统故障");
        break;
    default:
        break;
    }
}

static void stop_motors(void)
{
    (void)driver_motor_brake(DRIVER_MOTOR_LEFT);
    (void)driver_motor_brake(DRIVER_MOTOR_RIGHT);
    (void)driver_tb6612_brake(DRIVER_TB6612_MOTOR_LEFT);
    (void)driver_tb6612_brake(DRIVER_TB6612_MOTOR_RIGHT);
    g_app.left_speed_cmd = 0;
    g_app.right_speed_cmd = 0;
}

static void update_pose_from_mpu(app_ctx_t *ctx, const driver_mpu6050_data_t *mpu)
{
    const float ax = mpu->accel_x_g;
    const float ay = mpu->accel_y_g;
    const float az = mpu->accel_z_g;

    ctx->pitch_deg = atan2f(ax, sqrtf(ay * ay + az * az)) * 57.29578f;
    ctx->roll_deg = atan2f(ay, sqrtf(ax * ax + az * az)) * 57.29578f;

    ctx->lifted = (fabsf(ax) < 0.2f && fabsf(ay) < 0.2f && fabsf(az) < 0.6f);
    ctx->pose_safe = (fabsf(ctx->pitch_deg) < 28.0f && fabsf(ctx->roll_deg) < 28.0f);
}

static esp_err_t init_all_peripherals(app_ctx_t *ctx)
{
    esp_err_t err;

    driver_button_init();
    service_device_init();

    err = driver_oled_init_profile(DRIVER_OLED_PROFILE_096_SSD1306);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OLED init failed: %s", esp_err_to_name(err));
        ctx->oled_ok = false;
    } else {
        ctx->oled_ok = true;
    }

    err = driver_tof2000c_vl53l0x_init_default();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "TOF init failed: %s", esp_err_to_name(err));
        ctx->tof_ok = false;
    } else {
        ctx->tof_ok = true;
        (void)driver_tof2000c_vl53l0x_start_continuous(50);
    }

    err = driver_mpu6050_init_default();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "MPU6050 init failed: %s", esp_err_to_name(err));
        ctx->mpu_ok = false;
    } else {
        ctx->mpu_ok = true;
    }

    err = driver_tw_tts_init_default();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "TTS init failed: %s", esp_err_to_name(err));
        ctx->tts_ok = false;
    } else {
        ctx->tts_ok = true;
        (void)driver_tw_tts_set_volume(8);
    }

    err = driver_encoder_init(DRIVER_ENCODER_LEFT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Left encoder init failed: %s", esp_err_to_name(err));
        ctx->enc_ok = false;
    }
    err = driver_encoder_init(DRIVER_ENCODER_RIGHT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Right encoder init failed: %s", esp_err_to_name(err));
        ctx->enc_ok = false;
    }
    if (ctx->enc_ok != false) {
        ctx->enc_ok = true;
    }

    err = driver_motor_init(DRIVER_MOTOR_LEFT, &(driver_motor_config_t){ .ppr = DRIVER_MOTOR_PPR_DEFAULT });
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Left motor init failed: %s", esp_err_to_name(err));
        ctx->motor_ok = false;
    }
    err = driver_motor_init(DRIVER_MOTOR_RIGHT, &(driver_motor_config_t){ .ppr = DRIVER_MOTOR_PPR_DEFAULT });
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Right motor init failed: %s", esp_err_to_name(err));
        ctx->motor_ok = false;
    }
    if (ctx->motor_ok != false) {
        ctx->motor_ok = true;
    }

    (void)driver_tb6612_init(DRIVER_TB6612_MOTOR_LEFT);
    (void)driver_tb6612_init(DRIVER_TB6612_MOTOR_RIGHT);

    ctx->init_ok = ctx->tof_ok && ctx->mpu_ok && ctx->enc_ok && ctx->motor_ok && ctx->tts_ok && ctx->oled_ok;

    ESP_LOGI(TAG, "Init result: tof=%d mpu=%d enc=%d motor=%d tts=%d oled=%d",
             ctx->tof_ok, ctx->mpu_ok, ctx->enc_ok, ctx->motor_ok, ctx->tts_ok, ctx->oled_ok);

    return ctx->init_ok ? ESP_OK : ESP_FAIL;
}

static void self_check(app_ctx_t *ctx)
{
    uint8_t who_am_i = 0;
    uint8_t model_id = 0;
    esp_err_t err;

    err = driver_mpu6050_read_who_am_i(&who_am_i);
    if (err != ESP_OK || who_am_i != DRIVER_MPU6050_WHO_AM_I_VALUE) {
        ESP_LOGE(TAG, "MPU6050 WHO_AM_I mismatch: err=%s val=0x%02x", esp_err_to_name(err), who_am_i);
        ctx->mpu_ok = false;
    }

    err = driver_tof2000c_vl53l0x_read_model_id(&model_id);
    if (err != ESP_OK || model_id != DRIVER_TOF2000C_VL53L0X_MODEL_ID) {
        ESP_LOGE(TAG, "TOF model mismatch: err=%s val=0x%02x", esp_err_to_name(err), model_id);
        ctx->tof_ok = false;
    }

    if (ctx->motor_ok) {
        stop_motors();
        (void)driver_motor_reset_encoder(DRIVER_MOTOR_LEFT);
        (void)driver_motor_reset_encoder(DRIVER_MOTOR_RIGHT);
    }

    ctx->state = (ctx->tof_ok && ctx->mpu_ok && ctx->enc_ok && ctx->motor_ok && ctx->tts_ok && ctx->oled_ok)
                   ? APP_STATE_IDLE
                   : APP_STATE_ERROR;
}

static void read_sensors(app_ctx_t *ctx)
{
    driver_tof2000c_vl53l0x_result_t tof = {0};
    driver_mpu6050_data_t mpu = {0};
    int left = 0;
    int right = 0;

    if (driver_tof2000c_vl53l0x_read_continuous(&tof) == ESP_OK && !tof.timeout) {
        ctx->distance_mm = tof.distance_mm;
    }

    if (driver_mpu6050_read(&mpu) == ESP_OK) {
        update_pose_from_mpu(ctx, &mpu);
    }

    if (driver_motor_get_encoder_count(DRIVER_MOTOR_LEFT, &left) == ESP_OK) {
        ctx->left_count = left;
    }
    if (driver_motor_get_encoder_count(DRIVER_MOTOR_RIGHT, &right) == ESP_OK) {
        ctx->right_count = right;
    }

    ctx->obstacle_near = (ctx->distance_mm > 0U && ctx->distance_mm < 180U);
    ctx->user_near = (ctx->distance_mm > 0U && ctx->distance_mm < 500U);
    ctx->protecting = (!ctx->pose_safe || ctx->lifted || ctx->obstacle_near == false);
}

static void control_motion(app_ctx_t *ctx)
{
    const int16_t cruise_speed = 320;
    const int16_t slow_speed = 180;

    if (ctx->state == APP_STATE_MOVE) {
        int16_t target = ctx->obstacle_near ? slow_speed : cruise_speed;
        ctx->left_speed_cmd = target;
        ctx->right_speed_cmd = target;
        (void)driver_motor_set_speed(DRIVER_MOTOR_LEFT, target);
        (void)driver_motor_set_speed(DRIVER_MOTOR_RIGHT, target);
        (void)driver_tb6612_set_speed(DRIVER_TB6612_MOTOR_LEFT, target);
        (void)driver_tb6612_set_speed(DRIVER_TB6612_MOTOR_RIGHT, target);
    } else if (ctx->state == APP_STATE_AVOID) {
        ctx->left_speed_cmd = 120;
        ctx->right_speed_cmd = -120;
        (void)driver_motor_set_speed(DRIVER_MOTOR_LEFT, 120);
        (void)driver_motor_set_speed(DRIVER_MOTOR_RIGHT, -120);
        (void)driver_tb6612_set_speed(DRIVER_TB6612_MOTOR_LEFT, 120);
        (void)driver_tb6612_set_speed(DRIVER_TB6612_MOTOR_RIGHT, -120);
    } else {
        stop_motors();
    }
}

static void handle_button(app_ctx_t *ctx)
{
    driver_button_process();
    driver_button_event_t event = driver_button_get_event();

    if (event == DRIVER_BUTTON_EVENT_SHORT_PRESS) {
        if (ctx->state == APP_STATE_IDLE) {
            ctx->state = APP_STATE_MOVE;
        } else if (ctx->state == APP_STATE_MOVE) {
            ctx->state = APP_STATE_IDLE;
        }
    } else if (event == DRIVER_BUTTON_EVENT_LONG_PRESS) {
        ctx->state = APP_STATE_PROTECT;
    }
}

static void state_machine_step(app_ctx_t *ctx)
{
    switch (ctx->state) {
    case APP_STATE_INIT:
        if (init_all_peripherals(ctx) == ESP_OK) {
            ctx->state = APP_STATE_SELF_CHECK;
        } else {
            ctx->state = APP_STATE_ERROR;
        }
        break;

    case APP_STATE_SELF_CHECK:
        self_check(ctx);
        break;

    case APP_STATE_IDLE:
        if (!ctx->pose_safe || ctx->lifted) {
            ctx->state = APP_STATE_PROTECT;
        } else if (ctx->obstacle_near) {
            ctx->state = APP_STATE_AVOID;
        } else if (ctx->user_near) {
            ctx->state = APP_STATE_INTERACT;
        }
        break;

    case APP_STATE_INTERACT:
        if (!ctx->pose_safe || ctx->lifted) {
            ctx->state = APP_STATE_PROTECT;
        } else if (ctx->obstacle_near) {
            ctx->state = APP_STATE_AVOID;
        }
        break;

    case APP_STATE_MOVE:
        if (!ctx->pose_safe || ctx->lifted) {
            ctx->state = APP_STATE_PROTECT;
        } else if (ctx->obstacle_near) {
            ctx->state = APP_STATE_AVOID;
        }
        break;

    case APP_STATE_AVOID:
        if (!ctx->pose_safe || ctx->lifted) {
            ctx->state = APP_STATE_PROTECT;
        } else if (!ctx->obstacle_near) {
            ctx->state = APP_STATE_MOVE;
        }
        break;

    case APP_STATE_PROTECT:
        stop_motors();
        if (ctx->pose_safe && !ctx->lifted && !ctx->obstacle_near) {
            ctx->state = APP_STATE_IDLE;
        }
        break;

    case APP_STATE_ERROR:
    default:
        stop_motors();
        break;
    }
}

static void app_task(void *arg)
{
    (void)arg;

    memset(&g_app, 0, sizeof(g_app));
    g_app.state = APP_STATE_INIT;
    g_app.last_state = APP_STATE_ERROR;
    g_app.enc_ok = true;
    g_app.motor_ok = true;
    g_app.tts_ok = true;
    g_app.oled_ok = true;
    g_app.pose_safe = true;

    service_pid_config_t pid_cfg = {
        .kp = 0.8f,
        .ki = 0.0f,
        .kd = 0.0f,
        .output_min = -500.0f,
        .output_max = 500.0f,
        .integral_max = 200.0f,
    };
    (void)service_pid_init(&g_app.pid_left, &pid_cfg);
    (void)service_pid_init(&g_app.pid_right, &pid_cfg);

    ESP_LOGI(TAG, "robot_agent_app started");

    while (1) {
        handle_button(&g_app);
        read_sensors(&g_app);
        state_machine_step(&g_app);
        control_motion(&g_app);
        speak_once_on_state_change(&g_app, g_app.state);

        if ((xTaskGetTickCount() * portTICK_PERIOD_MS) - g_app.last_telemetry_ms >= 200) {
            g_app.last_telemetry_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
            ESP_LOGI(TAG, "state=%s dist=%umm pitch=%.1f roll=%.1f L=%ld R=%ld obstacle=%d user=%d lift=%d",
                     state_to_string(g_app.state),
                     (unsigned)g_app.distance_mm,
                     (double)g_app.pitch_deg,
                     (double)g_app.roll_deg,
                     (long)g_app.left_count,
                     (long)g_app.right_count,
                     g_app.obstacle_near,
                     g_app.user_near,
                     g_app.lifted);
            oled_draw_status(&g_app);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void)
{
    xTaskCreate(app_task, "robot_agent_app", 6144, NULL, 5, NULL);
}