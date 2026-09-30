/*
 * HAL for the Waveshare ESP32-S3-Touch-LCD-1.46
 * https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.46
 *
 * 412x412 SPD2010 display on QSPI, SPD2010 touch on I2C, and a TCA9554 IO
 * expander (also on I2C) that drives the display and touch reset lines.
 * Pin assignments are from Waveshare's documentation and demo code.
 */

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "driver/ledc.h"
#include "driver/sdmmc_host.h"
#include "driver/spi_master.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_heap_caps.h"
#include "esp_io_expander_tca9554.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_io_interface.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_spd2010.h"
#include "esp_lcd_touch_spd2010.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "hal.h"
#include "lvgl.h"

static const char *TAG = "board";

#define LCD_H_RES           412
#define LCD_V_RES           412

// Display (QSPI)
#define LCD_HOST            SPI2_HOST
#define PIN_LCD_SCLK        40
#define PIN_LCD_D0          46
#define PIN_LCD_D1          45
#define PIN_LCD_D2          42
#define PIN_LCD_D3          41
#define PIN_LCD_CS          21
#define PIN_LCD_BL          5
// Waveshare's demo runs the panel at 80 MHz. Espressif's driver defaults to
// 20 MHz, so try that if the picture ever glitches.
#define LCD_PCLK_HZ         (80 * 1000 * 1000)
#define LCD_SPI_MODE        0

// I2C bus shared by the touch controller, IO expander, IMU and RTC
#define PIN_I2C_SDA         11
#define PIN_I2C_SCL         10
#define PIN_TOUCH_INT       4

// TCA9554 IO expander outputs (EXIO1..3 on the schematic)
#define EXIO_TOUCH_RST      IO_EXPANDER_PIN_NUM_0
#define EXIO_LCD_RST        IO_EXPANDER_PIN_NUM_1
#define EXIO_SD_CS          IO_EXPANDER_PIN_NUM_2

// Power. On battery the board only stays on while PWR_HOLD is driven high.
#define PIN_PWR_KEY         6       // power button, low while pressed
#define PIN_PWR_HOLD        7
#define POWER_OFF_HOLD_MS   2000
#define POWER_KEY_POLL_MS   100

// Battery voltage on GPIO8 through a 1/3 divider
#define BAT_ADC_CHANNEL     ADC_CHANNEL_7
#define BAT_DIVIDER         3.0f
#define BAT_CORRECTION      0.990476f   // from Waveshare's demo

// Microphone (I2S). It sends 24-bit samples in the top of 32-bit slots, on
// the right channel (from Waveshare's demo).
#define PIN_MIC_BCLK        15
#define PIN_MIC_WS          2
#define PIN_MIC_DIN         39
#define AUDIO_BLOCK_MS      10
#define AUDIO_TASK_STACK    4096
#define AUDIO_TASK_PRIORITY 5
#define AUDIO_TASK_CORE     1       // LVGL runs on core 0

// SD card, in 1-bit SD mode. Its D3 line is EXIO_SD_CS, which is held high.
#define PIN_SD_CLK          14
#define PIN_SD_CMD          17
#define PIN_SD_D0           16
#define SD_MOUNT_POINT      "/sdcard"

// Backlight PWM
#define BL_LEDC_TIMER       LEDC_TIMER_0
#define BL_LEDC_CHANNEL     LEDC_CHANNEL_0
#define BL_LEDC_RESOLUTION  LEDC_TIMER_13_BIT
#define BL_LEDC_MAX_DUTY    ((1 << 13) - 1)

// The touch controller refuses some reads (the Waveshare demo ignores these
// errors too). Poll it at the same rate as Waveshare's demo, and only treat
// the finger as lifted after several failed reads in a row.
#define TOUCH_READ_PERIOD_MS    30
#define TOUCH_MAX_READ_ERRORS   5

// LVGL draws this many lines at a time. The SPD2010 needs drawing areas
// aligned to 4 pixels, so keep this a multiple of 4.
#define DRAW_BUF_LINES      40
#define DRAW_BUF_BYTES      (LCD_H_RES * DRAW_BUF_LINES * 2)

// Settings are stored as a blob in NVS
#define NVS_NAMESPACE       "rcat"
#define NVS_SETTINGS_KEY    "settings"

static i2c_master_bus_handle_t i2c_bus;
static esp_io_expander_handle_t io_expander;
static esp_lcd_panel_handle_t panel;
static esp_lcd_touch_handle_t touch;
static adc_oneshot_unit_handle_t adc;
static adc_cali_handle_t adc_cali;

static i2s_chan_handle_t mic_channel;
static rcat_hal_audio_block_cb_t audio_cb;
static void *audio_cb_user_data;
static size_t audio_block_samples;

static lv_display_t *display;
static uint8_t *draw_buf1;
static uint8_t *draw_buf2;
static uint8_t *rotate_buf;
static SemaphoreHandle_t clear_done;

static void power_init(void);
static void i2c_and_expander_init(void);
static void backlight_init(void);
static void panel_init(void);
static void panel_clear(void);
static void touch_init(void);
static void battery_init(void);
static void settings_storage_init(void);
static bool color_trans_done_cb(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx);
static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
static void rounder_event_cb(lv_event_t *e);
static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data);
static uint32_t tick_get_cb(void);

int rcat_hal_init(void)
{
    power_init();
    settings_storage_init();
    i2c_and_expander_init();
    backlight_init();
    panel_init();
    touch_init();
    battery_init();
    return 0;
}

/********************************************************************************
function:	Register the display and touch screen with LVGL
parameter:
********************************************************************************/
void rcat_hal_lvgl_register(void)
{
    lv_tick_set_cb(tick_get_cb);

    display = lv_display_create(LCD_H_RES, LCD_V_RES);
    lv_display_set_flush_cb(display, disp_flush_cb);
    lv_display_set_buffers(display, draw_buf1, draw_buf2, DRAW_BUF_BYTES, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_add_event_cb(display, rounder_event_cb, LV_EVENT_INVALIDATE_AREA, NULL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);
    lv_timer_set_period(lv_indev_get_read_timer(indev), TOUCH_READ_PERIOD_MS);
}

void rcat_hal_display_set_rotation(uint8_t quarter_turns)
{
    // The SPD2010 can't swap X and Y, so LVGL rotates in software (see
    // disp_flush_cb) and remaps touch to match. LVGL counts rotation
    // counter-clockwise, so 90 degrees clockwise is its ROTATION_270.
    static const lv_display_rotation_t rotations[4] = {
        LV_DISPLAY_ROTATION_0,
        LV_DISPLAY_ROTATION_270,
        LV_DISPLAY_ROTATION_180,
        LV_DISPLAY_ROTATION_90,
    };
    lv_display_set_rotation(display, rotations[quarter_turns & 3]);
}

void rcat_hal_backlight_set(uint8_t percent)
{
    if (percent > 100)
    {
        percent = 100;
    }
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL, percent * BL_LEDC_MAX_DUTY / 100);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL);
}

bool rcat_hal_battery_voltage(float *volts)
{
    int raw;
    int millivolts;

    if (adc_cali == NULL
        || adc_oneshot_read(adc, BAT_ADC_CHANNEL, &raw) != ESP_OK
        || adc_cali_raw_to_voltage(adc_cali, raw, &millivolts) != ESP_OK)
    {
        return false;
    }
    *volts = millivolts * BAT_DIVIDER / 1000.0f / BAT_CORRECTION;
    return true;
}

bool rcat_hal_settings_read(void *data, size_t len)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK)
    {
        return false;   // nothing saved yet
    }

    // Like erased flash, so a shorter saved blob leaves the rest at 0xFF.
    memset(data, 0xFF, len);
    size_t stored_len = len;
    esp_err_t err = nvs_get_blob(nvs, NVS_SETTINGS_KEY, data, &stored_len);
    nvs_close(nvs);
    return err == ESP_OK;
}

bool rcat_hal_settings_write(const void *data, size_t len)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK)
    {
        return false;
    }

    esp_err_t err = nvs_set_blob(nvs, NVS_SETTINGS_KEY, data, len);
    if (err == ESP_OK)
    {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err == ESP_OK;
}

/********************************************************************************
function:	Read the microphone in blocks and hand them to the callback
parameter:
********************************************************************************/
static void audio_task(void *arg)
{
    int32_t *raw = heap_caps_malloc(audio_block_samples * sizeof(int32_t), MALLOC_CAP_INTERNAL);
    int16_t *samples = heap_caps_malloc(audio_block_samples * sizeof(int16_t), MALLOC_CAP_INTERNAL);
    assert(raw && samples);

    while (1)
    {
        size_t bytes_read = 0;
        if (i2s_channel_read(mic_channel, raw, audio_block_samples * sizeof(int32_t), &bytes_read, portMAX_DELAY) != ESP_OK)
        {
            continue;
        }

        size_t count = bytes_read / sizeof(int32_t);
        for (size_t i = 0; i < count; i++)
        {
            samples[i] = (int16_t)(raw[i] >> 16);
        }
        audio_cb(samples, count, audio_cb_user_data);
    }
}

bool rcat_hal_audio_in_start(uint32_t sample_rate, rcat_hal_audio_block_cb_t cb, void *user_data)
{
    if (mic_channel != NULL)
    {
        return false;   // already running
    }

    audio_cb = cb;
    audio_cb_user_data = user_data;
    audio_block_samples = sample_rate * AUDIO_BLOCK_MS / 1000;

    i2s_chan_config_t chan_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_config.dma_frame_num = audio_block_samples;
    if (i2s_new_channel(&chan_config, NULL, &mic_channel) != ESP_OK)
    {
        ESP_LOGE(TAG, "No I2S channel for the microphone");
        mic_channel = NULL;
        return false;
    }

    i2s_std_config_t std_config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = PIN_MIC_BCLK,
            .ws = PIN_MIC_WS,
            .dout = I2S_GPIO_UNUSED,
            .din = PIN_MIC_DIN,
        },
    };
    std_config.slot_cfg.slot_mask = I2S_STD_SLOT_RIGHT;
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(mic_channel, &std_config));
    ESP_ERROR_CHECK(i2s_channel_enable(mic_channel));

    xTaskCreatePinnedToCore(audio_task, "audio", AUDIO_TASK_STACK, NULL, AUDIO_TASK_PRIORITY, NULL, AUDIO_TASK_CORE);
    return true;
}

bool rcat_hal_storage_mount(const char **path)
{
    static bool mounted;

    if (!mounted)
    {
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
        slot.width = 1;
        slot.clk = PIN_SD_CLK;
        slot.cmd = PIN_SD_CMD;
        slot.d0 = PIN_SD_D0;
        slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

        // Never format the card, even if it can't be read. It may have files
        // someone wants to keep.
        const esp_vfs_fat_sdmmc_mount_config_t mount_config = {
            .format_if_mount_failed = false,
            .max_files = 4,
            .allocation_unit_size = 16 * 1024,
        };
        sdmmc_card_t *card;
        esp_err_t err = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot, &mount_config, &card);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "No SD card (%s)", esp_err_to_name(err));
            return false;
        }
        ESP_LOGI(TAG, "SD card mounted, %llu MB", ((uint64_t)card->csd.capacity) * card->csd.sector_size / (1024 * 1024));
        mounted = true;
    }

    *path = SD_MOUNT_POINT;
    return true;
}

void rcat_hal_delay_ms(uint32_t ms)
{
    TickType_t ticks = pdMS_TO_TICKS(ms);
    vTaskDelay(ticks > 0 ? ticks : 1);
}

/********************************************************************************
function:	Power button handling. Holding the button for 2 seconds turns the
            board off when it's running on battery.
parameter:
********************************************************************************/
static void power_key_timer_cb(void *arg)
{
    static bool released_since_boot;
    static uint32_t held_ms;

    if (gpio_get_level(PIN_PWR_KEY) != 0)
    {
        released_since_boot = true;
        held_ms = 0;
        return;
    }

    // The button is usually still held from switching the board on, so
    // only count presses that start after it has been let go.
    if (!released_since_boot)
    {
        return;
    }

    held_ms += POWER_KEY_POLL_MS;
    if (held_ms >= POWER_OFF_HOLD_MS)
    {
        ESP_LOGI(TAG, "Power button held, turning off");
        rcat_hal_backlight_set(0);
        gpio_set_level(PIN_PWR_HOLD, 0);
    }
}

static void power_init(void)
{
    // Latch the power on first thing, before the power button is released.
    gpio_config_t hold = {
        .pin_bit_mask = 1ULL << PIN_PWR_HOLD,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&hold);
    gpio_set_level(PIN_PWR_HOLD, 1);

    gpio_config_t key = {
        .pin_bit_mask = 1ULL << PIN_PWR_KEY,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&key);

    const esp_timer_create_args_t timer_args = {
        .callback = power_key_timer_cb,
        .name = "power_key",
    };
    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, POWER_KEY_POLL_MS * 1000));
}

static void settings_storage_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

static void i2c_and_expander_init(void)
{
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));

    ESP_ERROR_CHECK(esp_io_expander_new_i2c_tca9554(i2c_bus, ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000, &io_expander));
    esp_io_expander_set_dir(io_expander, EXIO_TOUCH_RST | EXIO_LCD_RST | EXIO_SD_CS, IO_EXPANDER_OUTPUT);
    esp_io_expander_set_level(io_expander, EXIO_TOUCH_RST | EXIO_LCD_RST | EXIO_SD_CS, 1);

    // Reset the display and touch controllers.
    esp_io_expander_set_level(io_expander, EXIO_LCD_RST | EXIO_TOUCH_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_io_expander_set_level(io_expander, EXIO_LCD_RST | EXIO_TOUCH_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
}

static void backlight_init(void)
{
    const ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = BL_LEDC_RESOLUTION,
        .timer_num = BL_LEDC_TIMER,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    // Start dark. The app sets the saved brightness once the screen is cleared.
    const ledc_channel_config_t channel = {
        .gpio_num = PIN_LCD_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = BL_LEDC_CHANNEL,
        .timer_sel = BL_LEDC_TIMER,
        .duty = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));
}

static void panel_init(void)
{
    const spi_bus_config_t bus_config = SPD2010_PANEL_BUS_QSPI_CONFIG(PIN_LCD_SCLK,
                                                                      PIN_LCD_D0, PIN_LCD_D1,
                                                                      PIN_LCD_D2, PIN_LCD_D3,
                                                                      DRAW_BUF_BYTES);
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus_config, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_config = SPD2010_PANEL_IO_QSPI_CONFIG(PIN_LCD_CS, color_trans_done_cb, NULL);
    io_config.pclk_hz = LCD_PCLK_HZ;
    io_config.spi_mode = LCD_SPI_MODE;
    esp_lcd_panel_io_handle_t io;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io));

    const spd2010_vendor_config_t vendor_config = {
        .flags = {
            .use_qspi_interface = 1,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1,               // reset is on the IO expander
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = (void *)&vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_spd2010(io, &panel_config, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));

    // LVGL's draw buffers, plus one for rotated output. They have to be in
    // internal RAM so the SPI DMA can read them.
    draw_buf1 = heap_caps_malloc(DRAW_BUF_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    draw_buf2 = heap_caps_malloc(DRAW_BUF_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    rotate_buf = heap_caps_malloc(DRAW_BUF_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    assert(draw_buf1 && draw_buf2 && rotate_buf);

    panel_clear();
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
}

/********************************************************************************
function:	Fill the panel with black so the backlight doesn't show whatever
            was left in its memory
parameter:
********************************************************************************/
static void panel_clear(void)
{
    clear_done = xSemaphoreCreateBinary();
    memset(draw_buf1, 0, DRAW_BUF_BYTES);

    for (int y = 0; y < LCD_V_RES; y += DRAW_BUF_LINES)
    {
        int y_end = LV_MIN(y + DRAW_BUF_LINES, LCD_V_RES);
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, y, LCD_H_RES, y_end, draw_buf1));
        xSemaphoreTake(clear_done, portMAX_DELAY);
    }
}

/********************************************************************************
function:	The SPD2010 touch driver writes the register address first, then
            reads with a zero-length command. That worked with ESP-IDF's old
            I2C driver, but the new one (which the rest of the board uses)
            rejects a zero-length write. A command of -1 means "just read",
            so pass that instead.
parameter:
********************************************************************************/
static esp_err_t (*touch_io_rx_param)(esp_lcd_panel_io_t *io, int lcd_cmd, void *param, size_t param_size);

static esp_err_t touch_io_rx_param_plain_read(esp_lcd_panel_io_t *io, int lcd_cmd, void *param, size_t param_size)
{
    return touch_io_rx_param(io, (lcd_cmd == 0) ? -1 : lcd_cmd, param, param_size);
}

static void touch_init(void)
{
    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_SPD2010_CONFIG();
    esp_lcd_panel_io_handle_t io;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &io));
    touch_io_rx_param = io->rx_param;
    io->rx_param = touch_io_rx_param_plain_read;

    const esp_lcd_touch_config_t touch_config = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = -1,                 // reset is on the IO expander
        .int_gpio_num = PIN_TOUCH_INT,
    };
    if (esp_lcd_touch_new_i2c_spd2010(io, &touch_config, &touch) != ESP_OK)
    {
        ESP_LOGE(TAG, "Touch controller not found");
        touch = NULL;
        return;
    }

    // Failed reads are expected now and then (see TOUCH_MAX_READ_ERRORS), so
    // keep them out of the log once the controller is up.
    esp_log_level_set("SPD2010", ESP_LOG_NONE);
    esp_log_level_set("lcd_panel.io.i2c", ESP_LOG_NONE);
}

static void battery_init(void)
{
    const adc_oneshot_unit_init_cfg_t unit_config = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_config, &adc));

    const adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc, BAT_ADC_CHANNEL, &channel_config));

    const adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .chan = BAT_ADC_CHANNEL,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali) != ESP_OK)
    {
        ESP_LOGW(TAG, "No ADC calibration, battery voltage won't be shown");
        adc_cali = NULL;
    }
}

/********************************************************************************
function:	Called from the SPI interrupt when a block of pixels has been sent
parameter:
********************************************************************************/
static bool color_trans_done_cb(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    if (display != NULL)
    {
        lv_display_flush_ready(display);
        return false;
    }

    // Still clearing the screen in panel_clear()
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(clear_done, &woken);
    return woken == pdTRUE;
}

/********************************************************************************
function:	Send a rendered area to the panel, rotating it first if needed
parameter:
********************************************************************************/
static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    lv_display_rotation_t rotation = lv_display_get_rotation(disp);
    lv_area_t phys_area = *area;
    uint8_t *data = px_map;

    if (rotation != LV_DISPLAY_ROTATION_0)
    {
        int32_t w = lv_area_get_width(area);
        int32_t h = lv_area_get_height(area);
        int32_t rotated_w = (rotation == LV_DISPLAY_ROTATION_180) ? w : h;
        uint32_t src_stride = lv_draw_buf_width_to_stride(w, LV_COLOR_FORMAT_RGB565);
        uint32_t dest_stride = lv_draw_buf_width_to_stride(rotated_w, LV_COLOR_FORMAT_RGB565);

        lv_draw_sw_rotate(px_map, rotate_buf, w, h, src_stride, dest_stride, rotation, LV_COLOR_FORMAT_RGB565);
        lv_display_rotate_area(disp, &phys_area);
        data = rotate_buf;
    }

    // LVGL renders little-endian RGB565 but the panel wants the high byte first.
    lv_draw_sw_rgb565_swap(data, lv_area_get_size(&phys_area));

    // The end coordinates are exclusive. Completion is reported by color_trans_done_cb.
    esp_lcd_panel_draw_bitmap(panel, phys_area.x1, phys_area.y1, phys_area.x2 + 1, phys_area.y2 + 1, data);
}

/********************************************************************************
function:	The SPD2010 needs the X start and end of every drawing area aligned
            to 4 pixels. With a 90/270 degree rotation, the panel's X comes
            from LVGL's Y, so align that instead.
parameter:
********************************************************************************/
static void rounder_event_cb(lv_event_t *e)
{
    lv_area_t *area = lv_event_get_param(e);
    lv_display_rotation_t rotation = lv_display_get_rotation(lv_event_get_target(e));

    if (rotation == LV_DISPLAY_ROTATION_90 || rotation == LV_DISPLAY_ROTATION_270)
    {
        area->y1 &= ~3;
        area->y2 |= 3;
    }
    else
    {
        area->x1 &= ~3;
        area->x2 |= 3;
    }
}

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    static lv_point_t last_point;
    static lv_indev_state_t last_state = LV_INDEV_STATE_RELEASED;
    static uint8_t read_errors;

    esp_lcd_touch_point_data_t point;
    uint8_t count = 0;

    if (touch == NULL)
    {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    if (esp_lcd_touch_read_data(touch) != ESP_OK)
    {
        // Report the last state again rather than a lift in the middle of a drag.
        if (++read_errors >= TOUCH_MAX_READ_ERRORS)
        {
            last_state = LV_INDEV_STATE_RELEASED;
        }
    }
    else
    {
        read_errors = 0;
        esp_lcd_touch_get_data(touch, &point, &count, 1);
        if (count > 0)
        {
            last_point.x = point.x;
            last_point.y = point.y;
            last_state = LV_INDEV_STATE_PRESSED;
        }
        else
        {
            last_state = LV_INDEV_STATE_RELEASED;
        }
    }

    data->point = last_point;
    data->state = last_state;
}

static uint32_t tick_get_cb(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}
