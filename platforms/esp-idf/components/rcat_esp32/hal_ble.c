/*
 * Bluetooth LE for the HAL (see "Bluetooth LE" in hal.h), on ESP-IDF's
 * NimBLE stack as a central with a few links.
 *
 * NimBLE runs on its own task and calls back from there. The UI loop only
 * reads the state and buffers kept here, under ble_lock, and starts NimBLE
 * procedures, which NimBLE allows from any task.
 */

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include "hal.h"

static const char *TAG = "ble";

#define BLE_MAX_DEVICES         16
#define BLE_RX_BUFFER           256
#define BLE_CONNECT_TIMEOUT_MS  8000
// The controller takes about 38 KB of internal RAM when it starts, and
// crashes rather than failing if it can't get it.
#define BLE_MIN_FREE_INTERNAL   (40 * 1024)

typedef struct
{
    // Shared with the UI loop, under ble_lock
    rcat_hal_ble_state_t state;
    uint8_t rx_buffer[BLE_RX_BUFFER];
    size_t rx_head;             // where the next byte goes
    size_t rx_count;

    // Set up before connecting, then only touched on the NimBLE task
    uint16_t conn_handle;
    char name[32];
    rcat_hal_ble_service_match_t service_match;
    uint16_t svc_start, svc_end;
    uint16_t tx_handle, rx_handle, rx_end, cccd_handle;
    bool tx_no_rsp;
} ble_link_t;

static portMUX_TYPE ble_lock = portMUX_INITIALIZER_UNLOCKED;

// Shared with the UI loop, under ble_lock
static ble_link_t links[RCAT_HAL_BLE_MAX_LINKS];
static rcat_hal_ble_device_t found[BLE_MAX_DEVICES];
static size_t found_count;

static bool started;
static volatile bool synced;
static volatile bool scan_wanted;
static uint8_t own_addr_type;

static int scan_event_cb(struct ble_gap_event *event, void *arg);
static int link_event_cb(struct ble_gap_event *event, void *arg);

static void set_state(ble_link_t *link, rcat_hal_ble_state_t s)
{
    taskENTER_CRITICAL(&ble_lock);
    link->state = s;
    taskEXIT_CRITICAL(&ble_lock);
}

rcat_hal_ble_state_t rcat_hal_ble_link_state(int link)
{
    if (link < 0 || link >= RCAT_HAL_BLE_MAX_LINKS)
    {
        return RCAT_HAL_BLE_IDLE;
    }
    taskENTER_CRITICAL(&ble_lock);
    rcat_hal_ble_state_t s = links[link].state;
    taskEXIT_CRITICAL(&ble_lock);
    return s;
}

/********************************************************************************
function:	Scanning
********************************************************************************/
static void scan_begin(void)
{
    if (!synced || !scan_wanted || ble_gap_disc_active() || ble_gap_conn_active())
    {
        return;     // a connection in progress restarts it when it's done
    }
    struct ble_gap_disc_params params = {
        .passive = 0,               // ask for scan responses, which often carry the name
        .filter_duplicates = 0,     // the name can come in a later report than the first
    };
    int rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &params, scan_event_cb, NULL);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Scan didn't start (%d)", rc);
    }
}

static void add_device(const struct ble_gap_disc_desc *disc)
{
    struct ble_hs_adv_fields fields;
    if (ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data) != 0)
    {
        return;
    }

    taskENTER_CRITICAL(&ble_lock);
    rcat_hal_ble_device_t *d = NULL;
    for (size_t i = 0; i < found_count; i++)
    {
        if (found[i].addr_type == disc->addr.type && memcmp(found[i].addr, disc->addr.val, 6) == 0)
        {
            d = &found[i];
            break;
        }
    }
    if (d == NULL && fields.name != NULL && fields.name_len > 0 && found_count < BLE_MAX_DEVICES)
    {
        d = &found[found_count++];
        memcpy(d->addr, disc->addr.val, 6);
        d->addr_type = disc->addr.type;
    }
    if (d != NULL)
    {
        d->rssi = disc->rssi;
        if (fields.name != NULL && fields.name_len > 0)
        {
            size_t len = fields.name_len < sizeof(d->name) - 1 ? fields.name_len : sizeof(d->name) - 1;
            memcpy(d->name, fields.name, len);
            d->name[len] = '\0';
        }
    }
    taskEXIT_CRITICAL(&ble_lock);
}

static int scan_event_cb(struct ble_gap_event *event, void *arg)
{
    if (event->type == BLE_GAP_EVENT_DISC)
    {
        add_device(&event->disc);
    }
    return 0;
}

/********************************************************************************
function:	Finding the link: the service, its two characteristics, and the
			descriptor that turns notifications on
********************************************************************************/
static void link_failed(ble_link_t *link, const char *why, int rc)
{
    ESP_LOGW(TAG, "%s: %s (%d), disconnecting", link->name, why, rc);
    ble_gap_terminate(link->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

static void link_ready(ble_link_t *link)
{
    ESP_LOGI(TAG, "%s: link ready (write handle %u, notify handle %u). Free internal RAM: %u bytes",
             link->name, link->tx_handle, link->rx_handle,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    taskENTER_CRITICAL(&ble_lock);
    link->rx_count = 0;
    link->state = RCAT_HAL_BLE_CONNECTED;
    taskEXIT_CRITICAL(&ble_lock);
}

static int cccd_write_cb(uint16_t conn, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg)
{
    ble_link_t *link = arg;
    if (error->status != 0)
    {
        link_failed(link, "notifications didn't turn on", error->status);
        return 0;
    }
    link_ready(link);
    return 0;
}

static int dsc_cb(uint16_t conn, const struct ble_gatt_error *error, uint16_t chr_val_handle,
                  const struct ble_gatt_dsc *dsc, void *arg)
{
    ble_link_t *link = arg;
    if (error->status == 0)
    {
        if (ble_uuid_cmp(&dsc->uuid.u, BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16)) == 0)
        {
            link->cccd_handle = dsc->handle;
        }
        return 0;
    }
    if (error->status != BLE_HS_EDONE)
    {
        link_failed(link, "descriptor discovery failed", error->status);
        return 0;
    }

    if (link->cccd_handle == 0)
    {
        ESP_LOGW(TAG, "%s: no notification descriptor, so nothing will be received", link->name);
        link_ready(link);
        return 0;
    }
    static const uint8_t notify_on[2] = { 1, 0 };
    int rc = ble_gattc_write_flat(conn, link->cccd_handle, notify_on, sizeof(notify_on), cccd_write_cb, link);
    if (rc != 0)
    {
        link_failed(link, "couldn't turn notifications on", rc);
    }
    return 0;
}

static int chr_cb(uint16_t conn, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr, void *arg)
{
    ble_link_t *link = arg;
    if (error->status == 0)
    {
        // The characteristic after the notifying one ends its descriptors.
        if (link->rx_handle != 0 && link->rx_end == 0)
        {
            link->rx_end = chr->def_handle - 1;
        }
        if (link->tx_handle == 0 && (chr->properties & (BLE_GATT_CHR_PROP_WRITE | BLE_GATT_CHR_PROP_WRITE_NO_RSP)))
        {
            link->tx_handle = chr->val_handle;
            link->tx_no_rsp = (chr->properties & BLE_GATT_CHR_PROP_WRITE_NO_RSP) != 0;
        }
        if (link->rx_handle == 0 && (chr->properties & BLE_GATT_CHR_PROP_NOTIFY))
        {
            link->rx_handle = chr->val_handle;
        }
        return 0;
    }
    if (error->status != BLE_HS_EDONE)
    {
        link_failed(link, "characteristic discovery failed", error->status);
        return 0;
    }

    if (link->tx_handle == 0)
    {
        link_failed(link, "the service has nothing to write to", 0);
        return 0;
    }
    if (link->rx_handle == 0)
    {
        link_ready(link);
        return 0;
    }
    if (link->rx_end == 0)
    {
        link->rx_end = link->svc_end;
    }
    int rc = ble_gattc_disc_all_dscs(conn, link->rx_handle, link->rx_end, dsc_cb, link);
    if (rc != 0)
    {
        link_failed(link, "descriptor discovery didn't start", rc);
    }
    return 0;
}

static void uuid_to_bytes(const ble_uuid_t *uuid, uint8_t out[16])
{
    // The Bluetooth base UUID, 00000000-0000-1000-8000-00805f9b34fb
    static const uint8_t base[16] = { 0, 0, 0, 0, 0x00, 0x00, 0x10, 0x00,
                                      0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb };
    memcpy(out, base, 16);
    switch (uuid->type)
    {
    case BLE_UUID_TYPE_16:
    {
        uint16_t v = BLE_UUID16(uuid)->value;
        out[2] = v >> 8;
        out[3] = v & 0xFF;
        break;
    }
    case BLE_UUID_TYPE_32:
    {
        uint32_t v = BLE_UUID32(uuid)->value;
        out[0] = v >> 24;
        out[1] = (v >> 16) & 0xFF;
        out[2] = (v >> 8) & 0xFF;
        out[3] = v & 0xFF;
        break;
    }
    default:
        // NimBLE keeps 128-bit UUIDs least significant byte first.
        for (int i = 0; i < 16; i++)
        {
            out[i] = BLE_UUID128(uuid)->value[15 - i];
        }
        break;
    }
}

static int svc_cb(uint16_t conn, const struct ble_gatt_error *error, const struct ble_gatt_svc *service, void *arg)
{
    ble_link_t *link = arg;
    if (error->status == 0)
    {
        uint8_t uuid[16];
        uuid_to_bytes(&service->uuid.u, uuid);
        if (link->svc_start == 0 && link->service_match(uuid))
        {
            link->svc_start = service->start_handle;
            link->svc_end = service->end_handle;
        }
        return 0;
    }
    if (error->status != BLE_HS_EDONE)
    {
        link_failed(link, "service discovery failed", error->status);
        return 0;
    }

    if (link->svc_start == 0)
    {
        link_failed(link, "no matching service", 0);
        return 0;
    }
    int rc = ble_gattc_disc_all_chrs(conn, link->svc_start, link->svc_end, chr_cb, link);
    if (rc != 0)
    {
        link_failed(link, "characteristic discovery didn't start", rc);
    }
    return 0;
}

/********************************************************************************
function:	A link's events, on NimBLE's task
********************************************************************************/
static int link_event_cb(struct ble_gap_event *event, void *arg)
{
    ble_link_t *link = arg;
    switch (event->type)
    {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status != 0)
        {
            ESP_LOGW(TAG, "%s: connection failed (%d)", link->name, event->connect.status);
            set_state(link, RCAT_HAL_BLE_IDLE);
        }
        else
        {
            link->conn_handle = event->connect.conn_handle;
            ESP_LOGI(TAG, "%s: connected, finding the link", link->name);
            int rc = ble_gattc_disc_all_svcs(link->conn_handle, svc_cb, link);
            if (rc != 0)
            {
                link_failed(link, "service discovery didn't start", rc);
            }
        }
        scan_begin();   // carry on scanning, if it was
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "%s: disconnected (reason %d)", link->name, event->disconnect.reason);
        link->conn_handle = BLE_HS_CONN_HANDLE_NONE;
        set_state(link, RCAT_HAL_BLE_IDLE);
        break;

    case BLE_GAP_EVENT_NOTIFY_RX:
        if (event->notify_rx.attr_handle == link->rx_handle)
        {
            struct os_mbuf *om = event->notify_rx.om;
            uint8_t data[64];
            uint16_t len = OS_MBUF_PKTLEN(om);
            if (len > sizeof(data))
            {
                len = sizeof(data);
            }
            os_mbuf_copydata(om, 0, len, data);
            taskENTER_CRITICAL(&ble_lock);
            for (uint16_t i = 0; i < len; i++)
            {
                link->rx_buffer[link->rx_head] = data[i];
                link->rx_head = (link->rx_head + 1) % BLE_RX_BUFFER;
                if (link->rx_count < BLE_RX_BUFFER)
                {
                    link->rx_count++;   // otherwise the oldest byte has been overwritten
                }
            }
            taskEXIT_CRITICAL(&ble_lock);
        }
        break;

    default:
        break;
    }
    return 0;
}

static void on_sync(void)
{
    ble_hs_util_ensure_addr(0);
    ble_hs_id_infer_auto(0, &own_addr_type);
    synced = true;
    ESP_LOGI(TAG, "Bluetooth ready");
    scan_begin();
}

static void on_reset(int reason)
{
    ESP_LOGW(TAG, "Bluetooth reset (%d)", reason);
    synced = false;
}

static void host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/* Start the stack the first time it's needed. */
static bool ble_start(void)
{
    if (started)
    {
        return true;
    }
    size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    ESP_LOGI(TAG, "Free internal RAM before Bluetooth: %u bytes", (unsigned)free_internal);
    if (free_internal < BLE_MIN_FREE_INTERNAL)
    {
        ESP_LOGE(TAG, "Not enough internal RAM for Bluetooth (it needs about %u bytes)",
                 (unsigned)BLE_MIN_FREE_INTERNAL);
        return false;
    }
    esp_err_t err = nimble_port_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Bluetooth didn't start (%s)", esp_err_to_name(err));
        return false;
    }
    for (int i = 0; i < RCAT_HAL_BLE_MAX_LINKS; i++)
    {
        links[i].conn_handle = BLE_HS_CONN_HANDLE_NONE;
    }
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = on_reset;
    nimble_port_freertos_init(host_task);
    started = true;
    ESP_LOGI(TAG, "Free internal RAM after Bluetooth: %u bytes",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    return true;
}

/********************************************************************************
function:	The HAL functions, called from the UI loop
********************************************************************************/
bool rcat_hal_ble_scan_start(void)
{
    if (!ble_start())
    {
        return false;
    }
    taskENTER_CRITICAL(&ble_lock);
    found_count = 0;
    taskEXIT_CRITICAL(&ble_lock);

    scan_wanted = true;
    scan_begin();       // or on_sync does, once the stack is ready
    return true;
}

void rcat_hal_ble_scan_stop(void)
{
    scan_wanted = false;
    if (started && ble_gap_disc_active())
    {
        ble_gap_disc_cancel();
    }
}

size_t rcat_hal_ble_scan_results(rcat_hal_ble_device_t *devices, size_t max)
{
    taskENTER_CRITICAL(&ble_lock);
    size_t n = found_count < max ? found_count : max;
    memcpy(devices, found, n * sizeof(found[0]));
    taskEXIT_CRITICAL(&ble_lock);
    return n;
}

int rcat_hal_ble_connect(const rcat_hal_ble_device_t *device, rcat_hal_ble_service_match_t match)
{
    if (!started || !synced || match == NULL || ble_gap_conn_active())
    {
        return -1;
    }
    int free_link = -1;
    for (int i = 0; i < RCAT_HAL_BLE_MAX_LINKS; i++)
    {
        rcat_hal_ble_state_t s = rcat_hal_ble_link_state(i);
        if (s == RCAT_HAL_BLE_CONNECTING)
        {
            return -1;      // one at a time
        }
        if (s == RCAT_HAL_BLE_IDLE && links[i].conn_handle == BLE_HS_CONN_HANDLE_NONE && free_link < 0)
        {
            free_link = i;
        }
    }
    if (free_link < 0)
    {
        return -1;
    }

    // NimBLE can't scan and connect at once. The scan picks up again once
    // the connection is made, or fails.
    if (ble_gap_disc_active())
    {
        ble_gap_disc_cancel();
    }

    ble_link_t *link = &links[free_link];
    link->service_match = match;
    link->svc_start = link->svc_end = 0;
    link->tx_handle = link->rx_handle = link->rx_end = link->cccd_handle = 0;
    memcpy(link->name, device->name, sizeof(link->name));
    link->name[sizeof(link->name) - 1] = '\0';
    set_state(link, RCAT_HAL_BLE_CONNECTING);

    ble_addr_t addr = { .type = device->addr_type };
    memcpy(addr.val, device->addr, 6);
    int rc = ble_gap_connect(own_addr_type, &addr, BLE_CONNECT_TIMEOUT_MS, NULL, link_event_cb, link);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Couldn't connect to %s (%d)", device->name, rc);
        set_state(link, RCAT_HAL_BLE_IDLE);
        scan_begin();
        return -1;
    }
    ESP_LOGI(TAG, "Connecting to %s on link %d", device->name, free_link);
    return free_link;
}

void rcat_hal_ble_disconnect(int link)
{
    if (!started || link < 0 || link >= RCAT_HAL_BLE_MAX_LINKS)
    {
        return;
    }
    ble_link_t *l = &links[link];
    if (l->conn_handle != BLE_HS_CONN_HANDLE_NONE)
    {
        ble_gap_terminate(l->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
    else if (rcat_hal_ble_link_state(link) == RCAT_HAL_BLE_CONNECTING && ble_gap_conn_active())
    {
        ble_gap_conn_cancel();      // reported as a failed connection, which goes back to idle
    }
}

bool rcat_hal_ble_send(int link, const void *data, size_t len)
{
    if (rcat_hal_ble_link_state(link) != RCAT_HAL_BLE_CONNECTED)
    {
        return false;
    }
    ble_link_t *l = &links[link];
    int rc = l->tx_no_rsp ? ble_gattc_write_no_rsp_flat(l->conn_handle, l->tx_handle, data, len)
                          : ble_gattc_write_flat(l->conn_handle, l->tx_handle, data, len, NULL, NULL);
    return rc == 0;
}

size_t rcat_hal_ble_receive(int link, void *data, size_t max)
{
    if (link < 0 || link >= RCAT_HAL_BLE_MAX_LINKS)
    {
        return 0;
    }
    ble_link_t *l = &links[link];
    uint8_t *out = data;
    taskENTER_CRITICAL(&ble_lock);
    size_t n = l->rx_count < max ? l->rx_count : max;
    size_t tail = (l->rx_head + BLE_RX_BUFFER - l->rx_count) % BLE_RX_BUFFER;
    for (size_t i = 0; i < n; i++)
    {
        out[i] = l->rx_buffer[(tail + i) % BLE_RX_BUFFER];
    }
    l->rx_count -= n;
    taskEXIT_CRITICAL(&ble_lock);
    return n;
}
