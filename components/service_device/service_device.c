#include "service_device.h"
#include "bsp_led.h"
#include "bsp_button.h"

void service_device_init(void)
{
    bsp_led_init();
    bsp_button_init();
}