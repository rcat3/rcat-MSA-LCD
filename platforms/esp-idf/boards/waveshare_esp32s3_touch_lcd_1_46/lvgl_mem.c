/*
 * LVGL's memory (LV_USE_STDLIB_MALLOC = LV_STDLIB_CUSTOM in lv_conf.h), from
 * PSRAM where it can.
 *
 * With the C library's malloc, everything smaller than
 * CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL goes in internal RAM, which is most of
 * LVGL's objects, labels and styles. Internal RAM is what the display's DMA
 * buffers, the task stacks, WiFi and Bluetooth need, and Bluetooth won't start
 * without about 40 KB of it. The draw buffers are allocated separately, in
 * internal RAM, so drawing doesn't slow down.
 */

#include "esp_heap_caps.h"
#include "lvgl.h"

#define PSRAM_FIRST     MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT

void lv_mem_init(void)
{
}

void lv_mem_deinit(void)
{
}

lv_mem_pool_t lv_mem_add_pool(void *mem, size_t bytes)
{
    return NULL;    // not supported, as with the C library's malloc
}

void lv_mem_remove_pool(lv_mem_pool_t pool)
{
}

void *lv_malloc_core(size_t size)
{
    return heap_caps_malloc_prefer(size, 2, PSRAM_FIRST);
}

void *lv_realloc_core(void *p, size_t new_size)
{
    return heap_caps_realloc_prefer(p, new_size, 2, PSRAM_FIRST);
}

void lv_free_core(void *p)
{
    heap_caps_free(p);
}

void lv_mem_monitor_core(lv_mem_monitor_t *mon_p)
{
}

lv_result_t lv_mem_test_core(void)
{
    return LV_RESULT_OK;
}
