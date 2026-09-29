#include "app.h"
#include "hal.h"
#include "lvgl.h"
#include "ui/ui.h"

void app_run(void)
{
    if (hal_init() != 0)
    {
        return;
    }
    hal_backlight_set(UI_INITIAL_BRIGHTNESS);

    lv_init();
    hal_lvgl_register();
    ui_init();

    while (1)
    {
        lv_timer_handler();
        hal_delay_ms(5);
    }
}
