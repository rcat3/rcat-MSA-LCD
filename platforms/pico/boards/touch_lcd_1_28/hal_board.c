/*****************************************************************************
* | File        :   hal_board.c
* | Author      :   Waveshare team, Rubber Cat
* | Function    :   HAL for the Waveshare RP2040/RP2350-Touch-LCD-1.28
* | Info        :   Originally based on the Waveshare LVGL demo
*----------------
* |	This version:   V1.0
* | Date        :   2023-12-23
* | Info        :
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/

#include <string.h>

#include "hal.h"
#include "lvgl.h"

#include "hardware/flash.h"
#include "pico/flash.h"

#include "DEV_Config.h"
#include "LCD_1in28.h"
#include "CST816S.h"

#define DRAW_BUF_LINES 10

// Settings live in the last sector of flash, well past the end of the firmware.
// Flashing a new .uf2 doesn't touch it, so settings survive firmware updates.
#define SETTINGS_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

extern char __flash_binary_end;

// LVGL
static lv_display_t *display;
static uint8_t draw_buf[LCD_1IN28_WIDTH * DRAW_BUF_LINES * 2] __attribute__((aligned(4)));

// Touch screen state, updated from the touch interrupt
static uint16_t ts_x;
static uint16_t ts_y;
static lv_indev_state_t ts_act;

// Display rotation in clockwise quarter turns, used to map touch coordinates
static uint8_t rotation;

static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
static void touch_callback(uint gpio, uint32_t events);
static void ts_read_cb(lv_indev_t *indev, lv_indev_data_t *data);
static void dma_handler(void);
static uint32_t tick_get_cb(void);

int rcat_hal_init(void)
{
    if (DEV_Module_Init() != 0)
    {
        return -1;
    }
    LCD_1IN28_Init(HORIZONTAL);
    LCD_1IN28_Clear(BLACK);
    CST816S_init(CST816S_Point_Mode);
    return 0;
}

/********************************************************************************
function:	Register the display and touch screen with LVGL, then enable the
            DMA IRQ used for flushing
parameter:
********************************************************************************/
void rcat_hal_lvgl_register(void)
{
    lv_tick_set_cb(tick_get_cb);

    // Display
    display = lv_display_create(LCD_1IN28_WIDTH, LCD_1IN28_HEIGHT);
    lv_display_set_flush_cb(display, disp_flush_cb);
    lv_display_set_buffers(display, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Touch screen
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, ts_read_cb);
    DEV_IRQ_SET(Touch_INT_PIN, GPIO_IRQ_EDGE_RISE, &touch_callback);

    // DMA for transmitting color data from memory to SPI
    dma_channel_set_irq0_enabled(dma_tx, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
}

void rcat_hal_display_set_rotation(uint8_t quarter_turns)
{
    // The command goes over the same SPI bus as the DMA flush, so let any
    // transfer in progress finish first.
    dma_channel_wait_for_finish_blocking(dma_tx);
    while (spi_is_busy(LCD_SPI_PORT))
    {
    }

    // The panel rotates in hardware and the touch coordinates are mapped to
    // match in ts_read_cb(). LVGL itself doesn't need to know.
    rotation = quarter_turns & 3;
    LCD_1IN28_SetRotation(rotation);
    lv_obj_invalidate(lv_screen_active());
}

void rcat_hal_backlight_set(uint8_t percent)
{
    DEV_SET_PWM(percent);
}

bool rcat_hal_battery_voltage(float *volts)
{
    // The battery is measured through a 1/3 voltage divider.
    const float conversion_factor = 3.3f / (1 << 12) * 3;
    *volts = adc_read() * conversion_factor;
    return true;
}

bool rcat_hal_settings_read(void *data, size_t len)
{
    if (len > FLASH_PAGE_SIZE)
    {
        return false;
    }
    memcpy(data, (const void *)(XIP_BASE + SETTINGS_FLASH_OFFSET), len);
    return true;
}

typedef struct
{
    const void *data;
    size_t len;
} settings_write_t;

// Runs with interrupts disabled, since the flash can't be read (and so no
// code can run from it) while it's being erased or programmed.
static void settings_write_cb(void *param)
{
    const settings_write_t *w = param;
    static uint8_t page[FLASH_PAGE_SIZE];

    memset(page, 0xFF, sizeof(page));
    memcpy(page, w->data, w->len);
    flash_range_erase(SETTINGS_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(SETTINGS_FLASH_OFFSET, page, FLASH_PAGE_SIZE);
}

bool rcat_hal_settings_write(const void *data, size_t len)
{
    if (len > FLASH_PAGE_SIZE)
    {
        return false;
    }

    // Never erase part of the firmware if it ever grows into the last sector.
    if ((uintptr_t)&__flash_binary_end - XIP_BASE > SETTINGS_FLASH_OFFSET)
    {
        return false;
    }

    settings_write_t w = { data, len };
    return flash_safe_execute(settings_write_cb, &w, 100) == PICO_OK;
}

void rcat_hal_delay_ms(uint32_t ms)
{
    DEV_Delay_ms(ms);
}

/********************************************************************************
function:	Refresh image by transferring the color data to the SPI bus by DMA
parameter:
********************************************************************************/
static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    uint32_t pixels = lv_area_get_size(area);

    // LVGL renders little-endian RGB565 but the panel wants the high byte first.
    lv_draw_sw_rgb565_swap(px_map, pixels);

    LCD_1IN28_SetWindows(area->x1, area->y1, area->x2, area->y2);
    dma_channel_configure(dma_tx,
                          &c,
                          &spi_get_hw(LCD_SPI_PORT)->dr,
                          px_map, // read address
                          pixels * 2,
                          true);
}

/********************************************************************************
function:   Indicate ready with the flushing when DMA complete transmission
parameter:
********************************************************************************/
static void dma_handler(void)
{
    if (dma_channel_get_irq0_status(dma_tx))
    {
        dma_channel_acknowledge_irq0(dma_tx);
        lv_display_flush_ready(display);
    }
}

/********************************************************************************
function:   Touch interrupt handler
parameter:
********************************************************************************/
static void touch_callback(uint gpio, uint32_t events)
{
    if (gpio == Touch_INT_PIN)
    {
        CST816S_Get_Point();
        ts_x = Touch_CTS816.x_point;
        ts_y = Touch_CTS816.y_point;
        ts_act = LV_INDEV_STATE_PRESSED;
    }
}

/********************************************************************************
function:   Update touch screen input device status, rotating the touch point
            to match the display rotation
parameter:
********************************************************************************/
static void ts_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    const int32_t max = LCD_1IN28_WIDTH - 1;    // the panel is square

    switch (rotation)
    {
        case 1:
            data->point.x = ts_y;
            data->point.y = max - ts_x;
            break;
        case 2:
            data->point.x = max - ts_x;
            data->point.y = max - ts_y;
            break;
        case 3:
            data->point.x = max - ts_y;
            data->point.y = ts_x;
            break;
        default:
            data->point.x = ts_x;
            data->point.y = ts_y;
            break;
    }
    data->state = ts_act;
    ts_act = LV_INDEV_STATE_RELEASED;
}

/********************************************************************************
function:   Milliseconds since boot, for LVGL's timing
parameter:
********************************************************************************/
static uint32_t tick_get_cb(void)
{
    return to_ms_since_boot(get_absolute_time());
}
