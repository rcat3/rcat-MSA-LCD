/*****************************************************************************
* | File      	:   LCD_1in28_LVGL_test.c
* | Author      :   Waveshare team
* | Function    :   1.28inch LCD  test demo
* | Info        :
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

#include "LCD_test.h"
#include "LVGL_example.h"
#include "src/core/lv_obj.h"
#include "src/misc/lv_area.h"
#include "hardware/adc.h"

// LVGL
static lv_disp_draw_buf_t disp_buf;
static lv_color_t buf0[DISP_HOR_RES * DISP_VER_RES/2];
static lv_color_t buf1[DISP_HOR_RES * DISP_VER_RES/2];
static lv_disp_drv_t disp_drv;
static lv_disp_t *disp;

static lv_indev_drv_t indev_ts;
static lv_indev_drv_t indev_en;
static lv_group_t *group;

static lv_obj_t *label_imu;
static lv_obj_t *label_battery;

// Input Device
static int16_t encoder_diff;
static lv_indev_state_t encoder_act;

static uint16_t ts_x;
static uint16_t ts_y;
static lv_indev_state_t ts_act;

// Setup/config
static void brightness_slider_event_cb(lv_event_t * e);
static void rotation_roller_event_cb(lv_event_t * e);
static lv_obj_t * brightness_slider_label;
static lv_obj_t * rotation_slider_label;

// Timer
static struct repeating_timer lvgl_timer;
static struct repeating_timer battery_update_timer;

static void disp_flush_cb(lv_disp_drv_t * disp, const lv_area_t * area, lv_color_t * color_p);
static void touch_callback(uint gpio, uint32_t events);
static void ts_read_cb(lv_indev_drv_t * drv, lv_indev_data_t*data);
static void get_diff_data(void);
static void encoder_read_cb(lv_indev_drv_t * drv, lv_indev_data_t*data);
static void dma_handler(void);
static bool repeating_lvgl_timer_callback(struct repeating_timer *t);
static bool repeating_battery_timer_callback(struct repeating_timer *t);
static void add_pic_tile(lv_obj_t *tv, const lv_img_dsc_t *pic, uint8_t num, bool is_gif);
static void brightness_slider_event_cb(lv_event_t * e);

static uint16_t img_rotation = 0;

/********************************************************************************
function:	Initializes LVGL and enbable timers IRQ and DMA IRQ
parameter:
********************************************************************************/
void LVGL_Init(void)
{
    // /*1.Init Timer*/
    //add_repeating_timer_ms(500, repeating_imu_data_update_timer_callback, NULL, &imu_data_update_timer);
    //add_repeating_timer_ms(50, repeating_imu_diff_timer_callback,        NULL, &imu_diff_timer);
    add_repeating_timer_ms(1000, repeating_battery_timer_callback, NULL, &battery_update_timer);
    add_repeating_timer_ms(5,   repeating_lvgl_timer_callback,            NULL, &lvgl_timer);

    // /*2.Init LVGL core*/
    lv_init();

    // /*3.Init LVGL display*/
    lv_disp_draw_buf_init(&disp_buf, buf0, buf1, DISP_HOR_RES * DISP_VER_RES / 2);
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = disp_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.hor_res = DISP_HOR_RES;
    disp_drv.ver_res = DISP_VER_RES;
    disp= lv_disp_drv_register(&disp_drv);

#if INPUTDEV_TS
    // /*4.Init touch screen as input device*/
    lv_indev_drv_init(&indev_ts);
    indev_ts.type = LV_INDEV_TYPE_POINTER;
    indev_ts.read_cb = ts_read_cb;
    lv_indev_t * ts_indev = lv_indev_drv_register(&indev_ts);
    //Enable touch IRQ
    DEV_IRQ_SET(Touch_INT_PIN, GPIO_IRQ_EDGE_RISE, &touch_callback);
#endif

    // /*5.Init imu as input device*/
    lv_indev_drv_init(&indev_en);
    indev_en.type = LV_INDEV_TYPE_ENCODER;
    indev_en.read_cb = encoder_read_cb;
    lv_indev_t * encoder_indev = lv_indev_drv_register(&indev_en);
    group = lv_group_create();
    lv_indev_set_group(encoder_indev, group);

    // /6.Init DMA for transmit color data from memory to SPI
    dma_channel_set_irq0_enabled(dma_tx, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

}


/********************************************************************************
function:	Initializes the layout of LVGL widgets
parameter:
********************************************************************************/
void Widgets_Init(void)
{
    // /*Create tileview*/
    lv_obj_t *tv = lv_tileview_create(lv_scr_act());
    lv_obj_set_scrollbar_mode(tv,  LV_SCROLLBAR_MODE_OFF);
    lv_group_add_obj(group, tv);

    //extern const uint8_t num_imgs;
    const uint8_t num_imgs = 5;

    LV_IMG_DECLARE(home);
    LV_IMG_DECLARE(RCatLogo);
    LV_IMG_DECLARE(nonbinary);
    LV_IMG_DECLARE(hal9000);
    LV_IMG_DECLARE(evileye);
    LV_IMG_DECLARE(black);

    add_pic_tile(tv, &home, 0, false);
    add_pic_tile(tv, &RCatLogo, 1, false);
    add_pic_tile(tv, &nonbinary, 2, false);
    add_pic_tile(tv, &hal9000, 3, false);
    add_pic_tile(tv, &evileye, 4, true);
    add_pic_tile(tv, &black, 5, false);

    // ---------------------------
    // Brightness tile.
    lv_obj_t *brightness_tile = lv_tileview_add_tile(tv, 0, num_imgs+1, LV_DIR_TOP|LV_DIR_BOTTOM);

    // Brightness slider
    lv_obj_t *brightness_slider = lv_slider_create(brightness_tile);
    lv_obj_set_size(brightness_slider, 200, 20);
    lv_slider_set_value(brightness_slider, INITIAL_LCD_BRIGHTNESS, LV_ANIM_OFF);
    lv_slider_set_range(brightness_slider, 1, 100);
    lv_obj_center(brightness_slider);
    lv_obj_add_event_cb(brightness_slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    brightness_slider_label = lv_label_create(brightness_tile);
    char buf[8];
    lv_snprintf(buf, sizeof(buf), "%d%%", INITIAL_LCD_BRIGHTNESS);

    lv_label_set_text(brightness_slider_label, buf);
    lv_obj_align_to(brightness_slider_label, brightness_slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    lv_obj_t *brightness_label = lv_label_create(brightness_tile);
    lv_label_set_text(brightness_label, "Brightness");
    lv_obj_align_to(brightness_label, brightness_slider, LV_ALIGN_OUT_TOP_MID, 0, -10);

    // Battery voltage label
    label_battery = lv_label_create(brightness_tile);
    lv_obj_align(label_battery, LV_ALIGN_TOP_MID, 0, 35);

    // ---------------------------
    // Image rotation tile.
    lv_obj_t *rotation_tile = lv_tileview_add_tile(tv, 0, num_imgs+2, LV_DIR_TOP);

    // Image rotation roller.
    lv_obj_t *rotation_roller = lv_roller_create(rotation_tile);
    lv_roller_set_options(rotation_roller,
                          "0°\n90°\n180°\n270°", LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(rotation_roller, 3);
    lv_obj_center(rotation_roller);
    lv_obj_add_event_cb(rotation_roller, rotation_roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *rotation_label = lv_label_create(rotation_tile);
    lv_label_set_text(rotation_label, "Img Rotation");
    lv_obj_align_to(rotation_label, rotation_roller, LV_ALIGN_OUT_TOP_MID, 0, -10);
}

static void brightness_slider_event_cb(lv_event_t * e)
{
    lv_obj_t * brightness_slider = lv_event_get_target(e);
    char buf[8];
    lv_snprintf(buf, sizeof(buf), "%d%%", lv_slider_get_value(brightness_slider));
    lv_label_set_text(brightness_slider_label, buf);
    lv_obj_align_to(brightness_slider_label, brightness_slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    DEV_SET_PWM(lv_slider_get_value(brightness_slider));
}


static void rotation_roller_event_cb(lv_event_t * e)
{
    lv_obj_t * rotation_roller = lv_event_get_target(e);
    uint16_t selected = lv_roller_get_selected(rotation_roller);

    if (selected == 0)
    {
        img_rotation = 0;
    }
    else if (selected == 1)
    {
        img_rotation = 900;
    }
    else if (selected == 2)
    {
        img_rotation = 1800;
    }
    else
    {
        img_rotation = 2700;
    }

    Widgets_Init();
}


static void add_pic_tile(lv_obj_t *tv, const lv_img_dsc_t *pic, uint8_t num, bool is_gif)
{
        lv_dir_t tile_direction;
        lv_obj_t *this_img;

        // Figure out the scroll direction for a new tile.
        if (num == 0)
        {
            // The first tile can only swipe down.
            tile_direction = LV_DIR_BOTTOM;
        }
        else
        {
            // All other tiles (in the middle) can swipe up and down.
            tile_direction = LV_DIR_TOP|LV_DIR_BOTTOM;
        }

        // Add a tile...
        lv_obj_t *this_tile = lv_tileview_add_tile(tv, 0, num, tile_direction);

        // Add an image to the tile.
        if (is_gif)
        {
            this_img = lv_gif_create(this_tile);
            lv_gif_set_src(this_img, pic);
        }
        else
        {
            this_img = lv_img_create(this_tile);
            lv_img_set_src(this_img, pic);
        }


        lv_obj_align(this_img, LV_ALIGN_CENTER, 0, 0);

        if (!is_gif)
        {
            lv_img_set_angle(this_img, img_rotation);
        }

}

/********************************************************************************
function:	Refresh image by transferring the color data to the SPI bus by DMA
parameter:
********************************************************************************/
static void disp_flush_cb(lv_disp_drv_t * disp, const lv_area_t * area, lv_color_t * color_p)
{

    LCD_1IN28_SetWindows(area->x1, area->y1, area->x2 , area->y2);
    dma_channel_configure(dma_tx,
                          &c,
                          &spi_get_hw(LCD_SPI_PORT)->dr,
                          color_p, // read address
                          ((area->x2 + 1 - area-> x1)*(area->y2 + 1 - area -> y1))*2,
                          true);
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
static void ts_read_cb(lv_indev_drv_t * drv, lv_indev_data_t*data)
{
    data->point.x = ts_x;
    data->point.y = ts_y;
    data->state = ts_act;
    ts_act = LV_INDEV_STATE_RELEASED;
}


/********************************************************************************
function:	Update encoder input device status
parameter:
********************************************************************************/
static void encoder_read_cb(lv_indev_drv_t * drv, lv_indev_data_t*data)
{
    data->enc_diff = encoder_diff;
    data->state    = encoder_act;
    /* encoder_diff = 0; */
}

/********************************************************************************
function:   Indicate ready with the flushing when DMA complete transmission
parameter:
********************************************************************************/
static void dma_handler(void)
{
    if (dma_channel_get_irq0_status(dma_tx)) {
        dma_channel_acknowledge_irq0(dma_tx);
        lv_disp_flush_ready(&disp_drv);         /* Indicate you are ready with the flushing*/
    }
}


/********************************************************************************
function:   Report the elapsed time to LVGL each 5ms
parameter:
********************************************************************************/
static bool repeating_lvgl_timer_callback(struct repeating_timer *t)
{
    lv_tick_inc(5);
    return true;
}


/********************************************************************************
function:   Update the battery level
parameter:
********************************************************************************/
static bool repeating_battery_timer_callback(struct repeating_timer *t)
{
    char label_text[15];
    uint16_t result = adc_read();
    const float conversion_factor = 3.3f / (1 << 12) * 3;

    sprintf(label_text, "Battery: %.2fV", result * conversion_factor);
    lv_label_set_text(label_battery, label_text);
    return true;
}
