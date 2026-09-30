#include "app.h"
#include "app/settings.h"
#include "hal.h"
#include "lvgl.h"
#include "ui/ui.h"

void app_run(void)
{
    if (hal_init() != 0)
    {
        return;
    }
    settings_init();
    hal_backlight_set(settings_get()->brightness);

    lv_init();
    hal_lvgl_register();
    hal_display_set_rotation(settings_get()->rotation);
    ui_init();

    while (1)
    {
        lv_timer_handler();
        hal_delay_ms(5);
    }
}
