#include "app.h"
#include "app/settings.h"
#include "audio/audio_level.h"
#include "hal.h"
#include "lvgl.h"
#include "ui/ui.h"

void app_run(void)
{
    if (rcat_hal_init() != 0)
    {
        return;
    }
    settings_init();
    rcat_hal_backlight_set(settings_get()->brightness);

    lv_init();
    rcat_hal_lvgl_register();
    rcat_hal_display_set_rotation(settings_get()->rotation);
    audio_level_start();    // does nothing on boards without a microphone
    ui_init();

    while (1)
    {
        lv_timer_handler();
        rcat_hal_delay_ms(5);
    }
}
