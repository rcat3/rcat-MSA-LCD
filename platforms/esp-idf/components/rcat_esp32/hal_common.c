/*
 * HAL pieces that are the same on every ESP32 board
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"

#include "hal.h"
#include "rcat_esp32.h"

static const char *TAG = "storage";

#define SD_MOUNT_POINT      "/sdcard"

bool rcat_esp32_sd_mount(int clk, int cmd, int d0, const char **path)
{
    static bool mounted;

    if (!mounted)
    {
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
        slot.width = 1;
        slot.clk = clk;
        slot.cmd = cmd;
        slot.d0 = d0;
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
