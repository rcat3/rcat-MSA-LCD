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

#include "hal.h"
#include "lvgl.h"

#include "DEV_Config.h"
#include "LCD_1in28.h"
#include "CST816S.h"

#define DRAW_BUF_LINES 10
#define LVGL_TICK_MS   5

// LVGL
static lv_disp_draw_buf_t disp_buf;
static lv_color_t buf0[LCD_1IN28_WIDTH * DRAW_BUF_LINES];
static lv_disp_drv_t disp_drv;
static lv_indev_drv_t indev_ts;

// Touch screen state, updated from the touch interrupt
static uint16_t ts_x;
static uint16_t ts_y;
static lv_indev_state_t ts_act;

static struct repeating_timer lvgl_timer;

static void disp_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
static void touch_callback(uint gpio, uint32_t events);
static void ts_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data);
static void dma_handler(void);
static bool repeating_lvgl_timer_callback(struct repeating_timer *t);

int hal_init(void)
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
            tick timer and the DMA IRQ used for flushing
parameter:
********************************************************************************/
void hal_lvgl_register(void)
{
    add_repeating_timer_ms(LVGL_TICK_MS, repeating_lvgl_timer_callback, NULL, &lvgl_timer);

    // Display
    lv_disp_draw_buf_init(&disp_buf, buf0, NULL, LCD_1IN28_WIDTH * DRAW_BUF_LINES);
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = disp_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.hor_res = LCD_1IN28_WIDTH;
    disp_drv.ver_res = LCD_1IN28_HEIGHT;
    lv_disp_drv_register(&disp_drv);

    // Touch screen
    lv_indev_drv_init(&indev_ts);
    indev_ts.type = LV_INDEV_TYPE_POINTER;
    indev_ts.read_cb = ts_read_cb;
    lv_indev_drv_register(&indev_ts);
    DEV_IRQ_SET(Touch_INT_PIN, GPIO_IRQ_EDGE_RISE, &touch_callback);

    // DMA for transmitting color data from memory to SPI
    dma_channel_set_irq0_enabled(dma_tx, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
}

void hal_backlight_set(uint8_t percent)
{
    DEV_SET_PWM(percent);
}

bool hal_battery_voltage(float *volts)
{
    // The battery is measured through a 1/3 voltage divider.
    const float conversion_factor = 3.3f / (1 << 12) * 3;
    *volts = adc_read() * conversion_factor;
    return true;
}

void hal_delay_ms(uint32_t ms)
{
    DEV_Delay_ms(ms);
}

/********************************************************************************
function:	Refresh image by transferring the color data to the SPI bus by DMA
parameter:
********************************************************************************/
static void disp_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    LCD_1IN28_SetWindows(area->x1, area->y1, area->x2, area->y2);
    dma_channel_configure(dma_tx,
                          &c,
                          &spi_get_hw(LCD_SPI_PORT)->dr,
                          color_p, // read address
                          ((area->x2 + 1 - area->x1) * (area->y2 + 1 - area->y1)) * 2,
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
        lv_disp_flush_ready(&disp_drv);
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
function:   Update touch screen input device status
parameter:
********************************************************************************/
static void ts_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    data->point.x = ts_x;
    data->point.y = ts_y;
    data->state = ts_act;
    ts_act = LV_INDEV_STATE_RELEASED;
}

/********************************************************************************
function:   Report the elapsed time to LVGL
parameter:
********************************************************************************/
static bool repeating_lvgl_timer_callback(struct repeating_timer *t)
{
    lv_tick_inc(LVGL_TICK_MS);
    return true;
}
