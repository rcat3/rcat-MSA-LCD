/*
 * The settings block (see hal.h), kept as a blob in NVS, for the ESP32 boards
 */

#include <string.h>

#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "hal.h"
#include "rcat_esp32.h"

#define NVS_NAMESPACE       "rcat"
#define NVS_SETTINGS_KEY    "settings"

void rcat_esp32_settings_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
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
