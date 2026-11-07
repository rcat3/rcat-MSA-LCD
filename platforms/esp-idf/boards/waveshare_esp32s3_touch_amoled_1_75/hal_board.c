/*
 * HAL for the Waveshare ESP32-S3-Touch-AMOLED-1.75
 * https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.75
 *
 * 466x466 CO5300 AMOLED on QSPI, CST9217 touch, an AXP2101 power manager, an
 * ES8311 DAC for the speaker (through an NS4150B amplifier) and an ES7210 ADC
 * for the two microphones, a QMI8658 IMU and a TCA9554 IO expander, all but
 * the display on one I2C bus. Pin assignments are from Waveshare's schematic
 * and their board support package.
 *
 * WiFi, the hotspot, Bluetooth LE, settings and the SD card are shared with
 * the other ESP32 boards, in components/rcat_esp32.
 */

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "driver/spi_master.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_heap_caps.h"
#include "esp_io_expander_tca9554.h"
#include "esp_lcd_co5300.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch_cst9217.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "hal.h"
#include "lvgl.h"
#include "rcat_esp32.h"

static const char *TAG = "board";

#define LCD_H_RES           466
#define LCD_V_RES           466

// Display (QSPI). The panel's memory starts 6 columns in from the glass.
#define LCD_HOST            SPI2_HOST
#define PIN_LCD_SCLK        38
#define PIN_LCD_D0          4
#define PIN_LCD_D1          5
#define PIN_LCD_D2          6
#define PIN_LCD_D3          7
#define PIN_LCD_CS          12
#define PIN_LCD_RST         39
#define LCD_X_GAP           6
#define LCD_PCLK_HZ         (40 * 1000 * 1000)

// I2C bus shared by everything but the display
#define PIN_I2C_SDA         15
#define PIN_I2C_SCL         14
#define PIN_TOUCH_INT       11
#define PIN_TOUCH_RST       40

// TCA9554 IO expander inputs
#define EXIO_PWR_KEY        IO_EXPANDER_PIN_NUM_4   // SYS_OUT: high while the power button is held

// AXP2101 power manager. It turns the board on with the power button by
// itself, and turns it off if the button's held for several seconds; the
// firmware turns it off sooner, after POWER_OFF_HOLD_MS, like the 1.46" board.
#define AXP_ADDR            0x34
#define AXP_REG_STATUS1     0x00
#define AXP_STATUS1_BATTERY 0x08    // a battery is connected
#define AXP_REG_COMMON      0x10
#define AXP_COMMON_PWROFF   0x01
#define AXP_REG_ADC_ENABLE  0x30
#define AXP_ADC_VBAT        0x01
#define AXP_REG_VBAT_H      0x34    // 14 bits in mV, high 6 bits first
#define AXP_REG_LDO_ONOFF0  0x90
#define AXP_LDO_ALDO1       0x01
#define AXP_REG_ALDO1_V     0x92
#define AXP_ALDO1_3V3       ((3300 - 500) / 100)    // 0.5 V + 0.1 V a step
#define AXP_I2C_HZ          400000
#define AXP_TIMEOUT_MS      50
#define POWER_OFF_HOLD_MS   2000
#define POWER_KEY_POLL_MS   100

// Audio. The DAC and the ADC share one I2S port, so its clocks run at one
// rate for both: the microphone's. Sound for the speaker is resampled to it.
#define I2S_PORT            I2S_NUM_0
#define PIN_I2S_MCLK        42
#define PIN_I2S_BCLK        9
#define PIN_I2S_WS          45
#define PIN_I2S_DOUT        8       // to the ES8311
#define PIN_I2S_DIN         10      // from the ES7210
#define PIN_PA_EN           46      // the NS4150B amplifier, on while high
#define AUDIO_RATE          32000   // MIC_SAMPLE_RATE in core/audio/mic.h
#define AUDIO_BLOCK_MS      5       // short, to keep the voice effect's delay down
#define AUDIO_TASK_STACK    8192    // the voice effects and the voice lab run on it too
#define AUDIO_TASK_PRIORITY 5
#define AUDIO_TASK_CORE     1       // LVGL runs on core 0
#define MIC_GAIN_DB         30.0f   // the ES7210's microphone gain, 0 to 37.5 dB
#define SPK_VOLUME          80      // the ES8311's volume, 0 to 100
#define SPK_BLOCK_FRAMES    256
#define SPK_MAX_RATIO       6       // up to this many frames out for each frame in (5.3 kHz)
#define SPK_DMA_DESC_NUM    16
#define SPK_DMA_FRAME_NUM   128
#define SPK_IDLE_MS         500     // silence before the amplifier turns off, to save power
#define SPK_IDLE_POLL_MS    20
#define SPK_TASK_STACK      (24 * 1024)     // MP3 decoding, as on the 1.46" board
#define SPK_TASK_PRIORITY   6
#define SPK_TASK_CORE       1

// SD card, in 1-bit SD mode. Its D3 line is held high.
#define PIN_SD_CLK          2
#define PIN_SD_CMD          1
#define PIN_SD_D0           3
#define PIN_SD_D3           41

// The touch controller is polled at the same rate as on the 1.46" board, and
// the finger only counts as lifted after several failed reads in a row.
#define TOUCH_READ_PERIOD_MS    30
#define TOUCH_MAX_READ_ERRORS   5

// LVGL draws this many lines at a time. The CO5300 needs drawing areas that
// start on an even pixel and end on an odd one, so keep this even.
#define DRAW_BUF_LINES      32
#define DRAW_BUF_BYTES      (LCD_H_RES * DRAW_BUF_LINES * 2)

static i2c_master_bus_handle_t i2c_bus;
static i2c_master_dev_handle_t axp;
static esp_io_expander_handle_t io_expander;
static esp_lcd_panel_handle_t panel;
static esp_lcd_touch_handle_t touch;

static SemaphoreHandle_t audio_lock;
static i2s_chan_handle_t i2s_tx;
static i2s_chan_handle_t i2s_rx;
static esp_codec_dev_handle_t spk_dev;
static esp_codec_dev_handle_t mic_dev;
static bool mic_running;
static rcat_hal_audio_block_cb_t audio_cb;
static void *audio_cb_user_data;
static bool spk_running;
static rcat_hal_audio_fill_cb_t spk_cb;
static void *spk_cb_user_data;

static lv_display_t *display;
static uint8_t rotation_turns;      // clockwise quarter turns
static uint8_t *draw_buf1;
static uint8_t *draw_buf2;
static uint8_t *rotate_buf;
static SemaphoreHandle_t clear_done;

static void i2c_init(void);
static void power_init(void);
static void panel_init(void);
static void panel_clear(void);
static void touch_init(void);
static bool color_trans_done_cb(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx);
static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
static void rounder_event_cb(lv_event_t *e);
static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data);
static uint32_t tick_get_cb(void);

int rcat_hal_init(void)
{
    rcat_esp32_settings_init();
    i2c_init();
    power_init();
    panel_init();
    touch_init();
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
    // LVGL rotates in software (see disp_flush_cb) and remaps touch to match,
    // as on the 1.46" board. LVGL counts rotation counter-clockwise, so 90
    // degrees clockwise is its ROTATION_270.
    static const lv_display_rotation_t rotations[4] = {
        LV_DISPLAY_ROTATION_0,
        LV_DISPLAY_ROTATION_270,
        LV_DISPLAY_ROTATION_180,
        LV_DISPLAY_ROTATION_90,
    };
    lv_display_set_rotation(display, rotations[quarter_turns & 3]);
    rotation_turns = quarter_turns & 3;
}

void rcat_hal_backlight_set(uint8_t percent)
{
    // An AMOLED has no backlight. The panel dims itself instead.
    if (percent > 100)
    {
        percent = 100;
    }
    esp_lcd_panel_co5300_set_brightness(panel, percent);
}

/********************************************************************************
function:	AXP2101 registers
parameter:
********************************************************************************/
static bool axp_read(uint8_t reg, uint8_t *data, size_t len)
{
    return axp != NULL && i2c_master_transmit_receive(axp, &reg, 1, data, len, AXP_TIMEOUT_MS) == ESP_OK;
}

static bool axp_write(uint8_t reg, uint8_t value)
{
    const uint8_t data[2] = { reg, value };
    return axp != NULL && i2c_master_transmit(axp, data, sizeof(data), AXP_TIMEOUT_MS) == ESP_OK;
}

static bool axp_set_bits(uint8_t reg, uint8_t bits)
{
    uint8_t value;
    return axp_read(reg, &value, 1) && axp_write(reg, value | bits);
}

bool rcat_hal_battery_voltage(float *volts)
{
    uint8_t status;
    uint8_t raw[2];
    if (!axp_read(AXP_REG_STATUS1, &status, 1) || !(status & AXP_STATUS1_BATTERY)
        || !axp_read(AXP_REG_VBAT_H, raw, sizeof(raw)))
    {
        return false;
    }
    *volts = (((raw[0] & 0x3F) << 8) | raw[1]) / 1000.0f;
    return true;
}

bool rcat_hal_accel_read(float *x, float *y, float *z)
{
    float a[3];
    if (!rcat_esp32_qmi8658_read(i2c_bus, a))
    {
        return false;
    }

    // The IMU's axes in the panel's own frame (x right, y down, z out of the
    // glass, with the USB port at the bottom). Like the 1.46" board's, it's on
    // the back of the board. This is the 1.46" board's mapping, which hasn't
    // been checked on this board yet: if the level reads the wrong way, this
    // is the place to fix it.
    float px = a[1];
    float py = a[0];
    *z = -a[2];

    // Then into the frame of the picture, which is turned clockwise on the panel
    for (uint8_t i = 0; i < rotation_turns; i++)
    {
        float t = px;
        px = py;
        py = -t;
    }
    *x = px;
    *y = py;
    return true;
}

/********************************************************************************
function:	Set up the I2S port and both codecs, the first time either the
            microphone or the speaker is started. Both directions run at
            AUDIO_RATE, 16-bit stereo.
parameter:
********************************************************************************/
static bool audio_init(void)
{
    if (spk_dev != NULL)
    {
        return true;
    }

    i2s_chan_config_t chan_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_PORT, I2S_ROLE_MASTER);
    chan_config.dma_desc_num = SPK_DMA_DESC_NUM;
    chan_config.dma_frame_num = SPK_DMA_FRAME_NUM;
    chan_config.auto_clear = true;      // play silence, not old sound, if the task falls behind
    if (i2s_new_channel(&chan_config, &i2s_tx, &i2s_rx) != ESP_OK)
    {
        ESP_LOGE(TAG, "No I2S channels for the audio");
        return false;
    }
    i2s_std_config_t std_config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = PIN_I2S_MCLK,
            .bclk = PIN_I2S_BCLK,
            .ws = PIN_I2S_WS,
            .dout = PIN_I2S_DOUT,
            .din = PIN_I2S_DIN,
        },
    };
    std_config.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(i2s_tx, &std_config));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(i2s_rx, &std_config));

    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_PORT,
        .rx_handle = i2s_rx,
        .tx_handle = i2s_tx,
    };
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);
    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();

    // The speaker's DAC. The amplifier is switched here, not by the codec
    // driver, so it's only on while there's sound.
    audio_codec_i2c_cfg_t dac_i2c = {
        .port = I2C_NUM_0,
        .addr = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = i2c_bus,
    };
    es8311_codec_cfg_t dac_cfg = {
        .ctrl_if = audio_codec_new_i2c_ctrl(&dac_i2c),
        .gpio_if = gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .pa_pin = -1,
        .use_mclk = true,
        .hw_gain = {
            .pa_voltage = 3.3,
            .codec_dac_voltage = 3.3,
        },
    };
    const esp_codec_dev_cfg_t spk_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = es8311_codec_new(&dac_cfg),
        .data_if = data_if,
    };

    // The microphones' ADC: MIC1 and MIC2 are the two microphones, left and
    // right. MIC3 is the speaker's sound, for echo cancelling, which isn't
    // used.
    audio_codec_i2c_cfg_t adc_i2c = {
        .port = I2C_NUM_0,
        .addr = ES7210_CODEC_DEFAULT_ADDR,
        .bus_handle = i2c_bus,
    };
    es7210_codec_cfg_t adc_cfg = {
        .ctrl_if = audio_codec_new_i2c_ctrl(&adc_i2c),
        .mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC2,
    };
    const esp_codec_dev_cfg_t mic_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .codec_if = es7210_codec_new(&adc_cfg),
        .data_if = data_if,
    };

    spk_dev = esp_codec_dev_new((esp_codec_dev_cfg_t *)&spk_cfg);
    mic_dev = esp_codec_dev_new((esp_codec_dev_cfg_t *)&mic_cfg);
    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel = 2,
        .sample_rate = AUDIO_RATE,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    };
    if (spk_dev == NULL || mic_dev == NULL
        || esp_codec_dev_open(spk_dev, &fs) != ESP_CODEC_DEV_OK
        || esp_codec_dev_open(mic_dev, &fs) != ESP_CODEC_DEV_OK)
    {
        ESP_LOGE(TAG, "The audio codecs didn't start");
        spk_dev = NULL;
        mic_dev = NULL;
        return false;
    }
    esp_codec_dev_set_out_vol(spk_dev, SPK_VOLUME);
    esp_codec_dev_set_in_gain(mic_dev, MIC_GAIN_DB);

    const gpio_config_t pa_config = {
        .pin_bit_mask = 1ULL << PIN_PA_EN,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&pa_config);
    gpio_set_level(PIN_PA_EN, 0);
    ESP_LOGI(TAG, "Audio ready at %d Hz", AUDIO_RATE);
    return true;
}

static bool audio_start(void)
{
    xSemaphoreTake(audio_lock, portMAX_DELAY);
    bool ok = audio_init();
    xSemaphoreGive(audio_lock);
    return ok;
}

/********************************************************************************
function:	Read the microphones in blocks, mix them to mono and hand them to
            the callback
parameter:
********************************************************************************/
static void audio_task(void *arg)
{
    size_t frames = AUDIO_RATE * AUDIO_BLOCK_MS / 1000;
    int16_t *raw = heap_caps_malloc(frames * 2 * sizeof(int16_t), MALLOC_CAP_INTERNAL);
    int16_t *samples = heap_caps_malloc(frames * sizeof(int16_t), MALLOC_CAP_INTERNAL);
    assert(raw && samples);

    while (1)
    {
        if (esp_codec_dev_read(mic_dev, raw, frames * 2 * sizeof(int16_t)) != ESP_CODEC_DEV_OK)
        {
            rcat_hal_delay_ms(AUDIO_BLOCK_MS);
            continue;
        }
        for (size_t i = 0; i < frames; i++)
        {
            samples[i] = (int16_t)(((int32_t)raw[2 * i] + raw[2 * i + 1]) / 2);
        }
        audio_cb(samples, frames, audio_cb_user_data);
    }
}

bool rcat_hal_audio_in_start(uint32_t sample_rate, rcat_hal_audio_block_cb_t cb, void *user_data)
{
    if (mic_running)
    {
        return false;   // already running
    }
    if (sample_rate != AUDIO_RATE)
    {
        ESP_LOGE(TAG, "The microphone only runs at %d Hz", AUDIO_RATE);
        return false;
    }
    if (!audio_start())
    {
        return false;
    }

    audio_cb = cb;
    audio_cb_user_data = user_data;
    if (xTaskCreatePinnedToCore(audio_task, "audio", AUDIO_TASK_STACK, NULL, AUDIO_TASK_PRIORITY, NULL,
                                AUDIO_TASK_CORE) != pdPASS)
    {
        ESP_LOGE(TAG, "Not enough memory for the microphone task");
        return false;
    }
    mic_running = true;
    return true;
}

/********************************************************************************
function:	Resample mono sound from in_rate to AUDIO_RATE, by straight lines
            between the samples, carrying on smoothly from the last block.
            Returns how many stereo frames were written to out.
parameter:
********************************************************************************/
static size_t resample(const int16_t *in, size_t count, uint32_t in_rate, int16_t *out, size_t max_frames)
{
    static float phase;         // how far past the last input sample the next output is, 0 to 1
    static int16_t last;

    float step = (float)in_rate / AUDIO_RATE;
    size_t n = 0;
    for (size_t i = 0; i < count; i++)
    {
        int16_t next = in[i];
        while (phase < 1.0f && n < max_frames)
        {
            int16_t s = (int16_t)(last + (next - last) * phase);
            out[2 * n] = out[2 * n + 1] = s;
            n++;
            phase += step;
        }
        phase -= 1.0f;
        last = next;
    }
    return n;
}

/********************************************************************************
function:	Ask for sound and play it. The amplifier turns off after a moment
            of silence, and back on when there's more to play. The I2S port
            keeps running for the microphones, sending silence.
parameter:
********************************************************************************/
static void speaker_task(void *arg)
{
    int16_t *frames = heap_caps_calloc(SPK_BLOCK_FRAMES * 2, sizeof(int16_t), MALLOC_CAP_INTERNAL);
    int16_t *mono = heap_caps_calloc(SPK_BLOCK_FRAMES, sizeof(int16_t), MALLOC_CAP_INTERNAL);
    size_t out_max = SPK_BLOCK_FRAMES * SPK_MAX_RATIO;
    int16_t *out = heap_caps_calloc(out_max * 2, sizeof(int16_t), MALLOC_CAP_INTERNAL);
    assert(frames && mono && out);

    uint32_t rate = AUDIO_RATE;
    bool on = false;
    uint32_t idle_ms = 0;

    while (1)
    {
        uint32_t block_rate = rate;
        size_t count = spk_cb(frames, SPK_BLOCK_FRAMES, &block_rate, spk_cb_user_data);
        if (block_rate > 0)
        {
            rate = block_rate;
        }

        if (count == 0)
        {
            rcat_hal_delay_ms(SPK_IDLE_POLL_MS);
            if (on)
            {
                idle_ms += SPK_IDLE_POLL_MS;
                if (idle_ms >= SPK_IDLE_MS)
                {
                    gpio_set_level(PIN_PA_EN, 0);
                    on = false;
                    ESP_LOGI(TAG, "Speaker off");
                }
            }
            continue;
        }

        idle_ms = 0;
        if (!on)
        {
            gpio_set_level(PIN_PA_EN, 1);
            on = true;
            ESP_LOGI(TAG, "Speaker on, sound at %lu Hz", (unsigned long)rate);
        }

        // There's only one speaker, so mix both channels into it.
        for (size_t i = 0; i < count; i++)
        {
            mono[i] = (int16_t)(((int32_t)frames[2 * i] + frames[2 * i + 1]) / 2);
        }
        size_t n = resample(mono, count, rate, out, out_max);
        if (esp_codec_dev_write(spk_dev, out, n * 2 * sizeof(int16_t)) != ESP_CODEC_DEV_OK)
        {
            ESP_LOGW(TAG, "Speaker write failed");
        }
    }
}

bool rcat_hal_audio_out_start(rcat_hal_audio_fill_cb_t cb, void *user_data)
{
    if (spk_running)
    {
        return false;   // already running
    }
    if (!audio_start())
    {
        return false;
    }

    spk_cb = cb;
    spk_cb_user_data = user_data;
    if (xTaskCreatePinnedToCore(speaker_task, "speaker", SPK_TASK_STACK, NULL, SPK_TASK_PRIORITY, NULL,
                                SPK_TASK_CORE) != pdPASS)
    {
        ESP_LOGE(TAG, "Not enough memory for the speaker task");
        return false;
    }
    spk_running = true;
    ESP_LOGI(TAG, "Speaker ready");
    return true;
}

bool rcat_hal_storage_mount(const char **path)
{
    return rcat_esp32_sd_mount(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0, path);
}

/********************************************************************************
function:	Power button handling. Holding the button for 2 seconds turns the
            board off.
parameter:
********************************************************************************/
static void power_key_timer_cb(void *arg)
{
    static bool released_since_boot;
    static uint32_t held_ms;

    uint32_t levels = 0;
    if (esp_io_expander_get_level(io_expander, EXIO_PWR_KEY, &levels) != ESP_OK)
    {
        return;
    }
    if (!(levels & EXIO_PWR_KEY))
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
        axp_set_bits(AXP_REG_COMMON, AXP_COMMON_PWROFF);
    }
}

static void i2c_init(void)
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

    // Everything on it is an input, which is how it starts.
    ESP_ERROR_CHECK(esp_io_expander_new_i2c_tca9554(i2c_bus, ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000, &io_expander));
}

static void power_init(void)
{
    audio_lock = xSemaphoreCreateMutex();

    const i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AXP_ADDR,
        .scl_speed_hz = AXP_I2C_HZ,
    };
    if (i2c_master_bus_add_device(i2c_bus, &dev_config, &axp) != ESP_OK)
    {
        axp = NULL;
    }

    // DCDC1 is the board's 3.3 V and ALDO1 the codecs' analog 3.3 V. The
    // AXP2101 starts both, but make sure of ALDO1, and of the battery
    // voltage measurement. The charge current is left as it is (300 mA).
    bool ok = axp_write(AXP_REG_ALDO1_V, AXP_ALDO1_3V3)
              && axp_set_bits(AXP_REG_LDO_ONOFF0, AXP_LDO_ALDO1)
              && axp_set_bits(AXP_REG_ADC_ENABLE, AXP_ADC_VBAT);
    if (!ok)
    {
        ESP_LOGW(TAG, "The power manager didn't answer");
    }

    // The SD card's D3 line has to stay high in 1-bit SD mode.
    const gpio_config_t d3 = {
        .pin_bit_mask = 1ULL << PIN_SD_D3,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&d3);
    gpio_set_level(PIN_SD_D3, 1);

    const esp_timer_create_args_t timer_args = {
        .callback = power_key_timer_cb,
        .name = "power_key",
    };
    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, POWER_KEY_POLL_MS * 1000));
}

// Waveshare's initialization for this panel, but starting dark: the app sets
// the saved brightness once the screen is cleared.
static const co5300_lcd_init_cmd_t lcd_init_cmds[] = {
    {0xFE, (uint8_t[]){0x20}, 1, 0},
    {0x19, (uint8_t[]){0x10}, 1, 0},
    {0x1C, (uint8_t[]){0xA0}, 1, 0},
    {0xFE, (uint8_t[]){0x00}, 1, 0},
    {0xC4, (uint8_t[]){0x80}, 1, 0},
    {0x3A, (uint8_t[]){0x55}, 1, 0},
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x53, (uint8_t[]){0x20}, 1, 0},
    {0x51, (uint8_t[]){0x00}, 1, 0},
    {0x63, (uint8_t[]){0xFF}, 1, 0},
    {0x2A, (uint8_t[]){0x00, 0x06, 0x01, 0xD7}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xD1}, 4, 600},
    {0x11, NULL, 0, 600},
    {0x29, NULL, 0, 0},
};

static void panel_init(void)
{
    const spi_bus_config_t bus_config = CO5300_PANEL_BUS_QSPI_CONFIG(PIN_LCD_SCLK,
                                                                     PIN_LCD_D0, PIN_LCD_D1,
                                                                     PIN_LCD_D2, PIN_LCD_D3,
                                                                     DRAW_BUF_BYTES);
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus_config, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_config = CO5300_PANEL_IO_QSPI_CONFIG(PIN_LCD_CS, color_trans_done_cb, NULL);
    io_config.pclk_hz = LCD_PCLK_HZ;
    esp_lcd_panel_io_handle_t io;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io));

    const co5300_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags = {
            .use_qspi_interface = 1,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = (void *)&vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_co5300(io, &panel_config, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel, LCD_X_GAP, 0));

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
function:	Fill the panel with black, so turning the brightness up doesn't
            show whatever was left in its memory
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

static void touch_init(void)
{
    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_CST9217_CONFIG();
    io_config.scl_speed_hz = 400000;
    esp_lcd_panel_io_handle_t io;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &io));

    // Mirrored both ways from the panel, as in Waveshare's board support
    const esp_lcd_touch_config_t touch_config = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = PIN_TOUCH_RST,
        .int_gpio_num = PIN_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .mirror_x = 1,
            .mirror_y = 1,
        },
    };
    if (esp_lcd_touch_new_i2c_cst9217(io, &touch_config, &touch) != ESP_OK)
    {
        ESP_LOGE(TAG, "Touch controller not found");
        touch = NULL;
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
function:	The CO5300 needs every drawing area to start on an even pixel and
            end on an odd one, both ways. Rounding both ways covers any
            rotation.
parameter:
********************************************************************************/
static void rounder_event_cb(lv_event_t *e)
{
    lv_area_t *area = lv_event_get_param(e);
    area->x1 &= ~1;
    area->y1 &= ~1;
    area->x2 |= 1;
    area->y2 |= 1;
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
