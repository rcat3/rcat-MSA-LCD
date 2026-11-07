/*
 * WiFi for the HAL, shared by the ESP32 boards: joining a network for the
 * voice lab, and the hotspot for the phone remote. See hal.h.
 */

#include <string.h>

#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_wifi.h"

#include "hal.h"

static const char *TAG = "network";

// WiFi needs about this much internal RAM to start. Below it, starting would
// fail part way, and the driver can't give back what it took (each failed
// try costs about 9 KB), so it isn't tried.
#define WIFI_MIN_FREE_INTERNAL  (40 * 1024)

// Phone remote hotspot
#define HOTSPOT_CHANNEL     6
#define HOTSPOT_MAX_PHONES  4

static bool wifi_ready;
static volatile bool wifi_wanted;
static volatile bool wifi_connected;
static esp_ip4_addr_t wifi_ip;
static volatile bool hotspot_on;

static void wifi_event_cb(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_connected = false;
        if (wifi_wanted)
        {
            esp_wifi_connect();
        }
    }
    else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
    {
        wifi_ip = ((ip_event_got_ip_t *)data)->ip_info.ip;
        wifi_connected = true;
        ESP_LOGI(TAG, "WiFi connected, " IPSTR, IP2STR(&wifi_ip));
    }
    else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STACONNECTED)
    {
        ESP_LOGI(TAG, "Hotspot: a phone joined");
    }
    else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        ESP_LOGI(TAG, "Hotspot: a phone left");
    }
}

/* Start the WiFi driver, once, for joining a network or for the hotspot. Each
 * step is done once, so trying again after a failure carries on from where
 * it stopped. */
static bool wifi_setup(void)
{
    static bool netif_ready;
    static bool wifi_failed;

    if (wifi_ready)
    {
        return true;
    }
    if (wifi_failed)
    {
        ESP_LOGE(TAG, "WiFi failed to start earlier. Restart the board to try again.");
        return false;
    }
    size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    ESP_LOGI(TAG, "Free internal RAM before WiFi: %u bytes (largest block %u)", (unsigned)free_internal,
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    if (free_internal < WIFI_MIN_FREE_INTERNAL)
    {
        ESP_LOGE(TAG, "Not enough internal RAM for WiFi (needs about %u bytes)", (unsigned)WIFI_MIN_FREE_INTERNAL);
        return false;
    }
    if (!netif_ready)
    {
        esp_err_t err = esp_netif_init();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        {
            ESP_LOGE(TAG, "Network setup failed (%s)", esp_err_to_name(err));
            return false;
        }
        err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        {
            ESP_LOGE(TAG, "Event loop failed (%s)", esp_err_to_name(err));
            return false;
        }
        netif_ready = true;
    }
    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&init_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "WiFi didn't start (%s)", esp_err_to_name(err));
        wifi_failed = true;
        return false;
    }
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_cb, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_cb, NULL);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);     // nothing about the networks is kept in flash
    wifi_ready = true;
    return true;
}

/* Stop whichever of the two is running. */
static void wifi_stop(void)
{
    if (wifi_wanted)
    {
        wifi_wanted = false;
        wifi_connected = false;
        esp_wifi_disconnect();
    }
    hotspot_on = false;
    esp_wifi_stop();
}

bool rcat_hal_wifi_connect(const char *ssid, const char *password)
{
    static esp_netif_t *sta_netif;

    if (!wifi_setup())
    {
        return false;
    }
    wifi_stop();
    if (sta_netif == NULL)
    {
        sta_netif = esp_netif_create_default_wifi_sta();
    }
    esp_wifi_set_mode(WIFI_MODE_STA);

    wifi_config_t config = { 0 };
    // These fields don't need a terminating zero when they're full.
    memcpy(config.sta.ssid, ssid, strnlen(ssid, sizeof(config.sta.ssid)));
    memcpy(config.sta.password, password, strnlen(password, sizeof(config.sta.password)));
    esp_wifi_set_config(WIFI_IF_STA, &config);

    wifi_wanted = true;
    esp_wifi_start();
    // Power saving delays packets by up to a beacon interval, so leave it off
    // while the voice lab streams.
    esp_wifi_set_ps(WIFI_PS_NONE);
    ESP_LOGI(TAG, "Connecting to WiFi network %s. Free internal RAM: %u bytes", ssid,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    return true;
}

void rcat_hal_wifi_disconnect(void)
{
    if (!wifi_ready || !wifi_wanted)
    {
        return;
    }
    wifi_stop();
    ESP_LOGI(TAG, "WiFi off");
}

bool rcat_hal_wifi_address(char *ip, size_t len)
{
    if (!wifi_connected)
    {
        return false;
    }
    esp_ip4_addr_t addr = wifi_ip;
    snprintf(ip, len, IPSTR, IP2STR(&addr));
    return true;
}

bool rcat_hal_hotspot_start(const char *ssid, const char *password)
{
    static esp_netif_t *ap_netif;

    if (!wifi_setup())
    {
        return false;
    }
    wifi_stop();
    if (ap_netif == NULL)
    {
        ap_netif = esp_netif_create_default_wifi_ap();
        if (ap_netif == NULL)
        {
            ESP_LOGE(TAG, "Hotspot: no network interface");
            return false;
        }
        // Have the phones ask the board for DNS, so it can send every name to
        // itself and the phone shows its page on joining.
        esp_netif_ip_info_t ip_info;
        esp_netif_get_ip_info(ap_netif, &ip_info);
        esp_netif_dns_info_t dns = { 0 };
        dns.ip.u_addr.ip4.addr = ip_info.ip.addr;
        dns.ip.type = ESP_IPADDR_TYPE_V4;
        uint8_t offer_dns = 0x02;   // OFFER_DNS
        esp_netif_dhcps_stop(ap_netif);
        esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &offer_dns,
                               sizeof(offer_dns));
        esp_netif_set_dns_info(ap_netif, ESP_NETIF_DNS_MAIN, &dns);
        // And tell newer phones straight away where the sign-in page is
        // (DHCP option 114). The option keeps the pointer, so it's static.
        static const char portal_uri[] = "http://" RCAT_HAL_HOTSPOT_ADDRESS "/";
        esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_CAPTIVEPORTAL_URI, (void *)portal_uri,
                               strlen(portal_uri));
        esp_netif_dhcps_start(ap_netif);
    }
    esp_wifi_set_mode(WIFI_MODE_AP);

    wifi_config_t config = { 0 };
    size_t ssid_len = strnlen(ssid, sizeof(config.ap.ssid));
    memcpy(config.ap.ssid, ssid, ssid_len);
    config.ap.ssid_len = (uint8_t)ssid_len;
    memcpy(config.ap.password, password, strnlen(password, sizeof(config.ap.password)));
    config.ap.channel = HOTSPOT_CHANNEL;
    config.ap.max_connection = HOTSPOT_MAX_PHONES;
    config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    config.ap.pmf_cfg.capable = true;
    esp_err_t err = esp_wifi_set_config(WIFI_IF_AP, &config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Hotspot: bad settings (%s)", esp_err_to_name(err));
        return false;
    }
    err = esp_wifi_start();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Hotspot didn't start (%s)", esp_err_to_name(err));
        return false;
    }
    hotspot_on = true;
    ESP_LOGI(TAG, "Hotspot %s on. Free internal RAM: %u bytes (largest block %u)", ssid,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    return true;
}

void rcat_hal_hotspot_stop(void)
{
    if (!wifi_ready || !hotspot_on)
    {
        return;
    }
    wifi_stop();
    ESP_LOGI(TAG, "Hotspot off");
}

int rcat_hal_hotspot_clients(void)
{
    wifi_sta_list_t list;
    if (!hotspot_on || esp_wifi_ap_get_sta_list(&list) != ESP_OK)
    {
        return 0;
    }
    return list.num;
}

uint32_t rcat_hal_random(void)
{
    // Truly random while the radio's on, which it is for the hotspot
    return esp_random();
}
