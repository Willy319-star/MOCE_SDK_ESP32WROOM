#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DRIVER_BUTTON_EVENT_NONE = 0,
    DRIVER_BUTTON_EVENT_PRESS,
    DRIVER_BUTTON_EVENT_RELEASE,
    DRIVER_BUTTON_EVENT_SHORT_PRESS,
    DRIVER_BUTTON_EVENT_LONG_PRESS,
} driver_button_event_t;

void driver_button_init(void);

/* 周期调用，用于按键状态更新与事件检测 */
void driver_button_process(void);

/* 获取并清除当前事件 */
driver_button_event_t driver_button_get_event(void);

/* 当前是否按下 */
bool driver_button_is_pressed(void);

#ifdef __cplusplus
}
#endif