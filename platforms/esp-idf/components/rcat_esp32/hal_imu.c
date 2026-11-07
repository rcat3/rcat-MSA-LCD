/*
 * The QMI8658 IMU's accelerometer, which several of the ESP32 boards have on
 * their I2C bus
 */

#include "driver/i2c_master.h"
#include "esp_log.h"

#include "rcat_esp32.h"

static const char *TAG = "imu";

// Only the accelerometer is used.
#define IMU_ADDR            0x6B
#define IMU_REG_WHO_AM_I    0x00
#define IMU_WHO_AM_I        0x05
#define IMU_REG_CTRL1       0x02
#define IMU_REG_CTRL2       0x03
#define IMU_REG_CTRL7       0x08
#define IMU_REG_AX_L        0x35
#define IMU_CTRL1_AUTO_INC  0x40    // reads step through the registers; data is little-endian
#define IMU_CTRL2_2G_120HZ  0x06    // +/-2 g, about 120 readings a second
#define IMU_CTRL7_ACCEL_ON  0x01
#define IMU_G_PER_COUNT     (2.0f / 32768.0f)
#define IMU_I2C_HZ          400000
#define IMU_TIMEOUT_MS      50

static i2c_master_dev_handle_t imu;
static bool imu_tried;

/********************************************************************************
function:	Start the IMU's accelerometer the first time it's needed. Returns
            false if the IMU doesn't answer.
parameter:
********************************************************************************/
static bool imu_start(i2c_master_bus_handle_t bus)
{
    if (imu_tried)
    {
        return imu != NULL;
    }
    imu_tried = true;

    const i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = IMU_ADDR,
        .scl_speed_hz = IMU_I2C_HZ,
    };
    if (i2c_master_bus_add_device(bus, &dev_config, &imu) != ESP_OK)
    {
        imu = NULL;
        return false;
    }

    uint8_t reg = IMU_REG_WHO_AM_I;
    uint8_t id = 0;
    const uint8_t setup[][2] = {
        { IMU_REG_CTRL1, IMU_CTRL1_AUTO_INC },
        { IMU_REG_CTRL2, IMU_CTRL2_2G_120HZ },
        { IMU_REG_CTRL7, IMU_CTRL7_ACCEL_ON },
    };
    bool ok = i2c_master_transmit_receive(imu, &reg, 1, &id, 1, IMU_TIMEOUT_MS) == ESP_OK && id == IMU_WHO_AM_I;
    for (size_t i = 0; ok && i < sizeof(setup) / sizeof(setup[0]); i++)
    {
        ok = i2c_master_transmit(imu, setup[i], 2, IMU_TIMEOUT_MS) == ESP_OK;
    }
    if (!ok)
    {
        ESP_LOGW(TAG, "No IMU (id 0x%02x)", id);
        i2c_master_bus_rm_device(imu);
        imu = NULL;
    }
    return ok;
}

bool rcat_esp32_qmi8658_read(i2c_master_bus_handle_t bus, float a[3])
{
    if (!imu_start(bus))
    {
        return false;
    }

    uint8_t reg = IMU_REG_AX_L;
    uint8_t raw[6];
    if (i2c_master_transmit_receive(imu, &reg, 1, raw, sizeof(raw), IMU_TIMEOUT_MS) != ESP_OK)
    {
        return false;
    }
    for (int i = 0; i < 3; i++)
    {
        a[i] = (int16_t)(raw[2 * i] | (raw[2 * i + 1] << 8)) * IMU_G_PER_COUNT;
    }
    return true;
}
