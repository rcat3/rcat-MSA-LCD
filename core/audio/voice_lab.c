/*
 * Streams the microphone to tools/voice_monitor.py over WiFi (see
 * voice_lab.h).
 *
 * Each 10 ms block of samples goes out as one UDP packet: a 16 byte header
 * (see packet_t) followed by the samples, little-endian. Packets are sent
 * from the microphone's task without waiting, and dropped if the network is
 * busy; the monitor fills the gaps with silence.
 *
 * One socket, on VOICE_LAB_PORT, both listens for hellos (on the UI loop)
 * and sends the stream (on the microphone's task). Sending from the port the
 * hello went to makes the stream a reply, which routers and firewalls between
 * network segments let through. ESP-IDF's lwIP allows one task to send while
 * another receives on the same socket.
 */
#include <stdio.h>
#include <string.h>

#include "app/remote.h"
#include "audio/mic.h"
#include "audio/voice_lab.h"
#include "hal.h"

#if RCAT_HAVE_NETWORK && RCAT_HAVE_FILESYSTEM

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#define WIFI_FILE           "wifi.txt"
#define HELLO               "RCAT-HELLO"
#define HELLO_TIMEOUT_MS    5000
#define MAX_BLOCK_SAMPLES   (MIC_SAMPLE_RATE / 100)

#define PACKET_MAGIC        0x31564352u     // "RCV1"
#define KIND_COUNT          2

typedef struct
{
    uint32_t magic;
    uint32_t seq;
    uint32_t sample_rate;
    uint16_t kind;
    uint16_t count;
    int16_t samples[MAX_BLOCK_SAMPLES];
} packet_t;

static char wifi_path[96];
static char ssid[33];
static bool started;
static bool listening;
static int sock = -1;

// Where to stream, written by the UI loop and read by the microphone's task:
// the address in the top 32 bits and the port in the bottom 16, 0 for nowhere.
static uint64_t destination;
static uint32_t last_hello_ms;

static packet_t packets[KIND_COUNT];    // only used by the microphone's task

// Counted by the microphone's task, reported by the UI loop.
static volatile uint32_t sent_count;
static volatile uint32_t failed_count;
static volatile int last_errno;
static uint32_t last_report_ms;
#define REPORT_MS 5000

void voice_lab_send(uint16_t kind, uint32_t sample_rate, const int16_t *samples, size_t count)
{
    uint64_t dest = __atomic_load_n(&destination, __ATOMIC_ACQUIRE);
    if (dest == 0 || kind >= KIND_COUNT || count > MAX_BLOCK_SAMPLES)
    {
        return;
    }

    packet_t *packet = &packets[kind];
    packet->magic = PACKET_MAGIC;
    packet->seq++;
    packet->sample_rate = sample_rate;
    packet->kind = kind;
    packet->count = (uint16_t)count;
    memcpy(packet->samples, samples, count * sizeof(int16_t));

    struct sockaddr_in to = { 0 };
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = (uint32_t)(dest >> 16);
    to.sin_port = (uint16_t)dest;
    if (sendto(sock, packet, offsetof(packet_t, samples) + count * sizeof(int16_t), MSG_DONTWAIT,
               (struct sockaddr *)&to, sizeof(to)) < 0)
    {
        failed_count++;
        last_errno = errno;
    }
    else
    {
        sent_count++;
    }
}

static void mic_cb(const int16_t *samples, size_t count, void *user_data)
{
    voice_lab_send(VOICE_LAB_KIND_MIC, MIC_SAMPLE_RATE, samples, count);
}

/* Read "key=value" lines from wifi.txt. */
static bool read_wifi_file(char *password, size_t password_len)
{
    FILE *f = fopen(wifi_path, "r");
    if (f == NULL)
    {
        return false;
    }
    ssid[0] = '\0';
    password[0] = '\0';
    char line[128];
    while (fgets(line, sizeof(line), f) != NULL)
    {
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "ssid=", 5) == 0)
        {
            snprintf(ssid, sizeof(ssid), "%.32s", line + 5);         // WiFi names are up to 32 characters
        }
        else if (strncmp(line, "password=", 9) == 0)
        {
            snprintf(password, password_len, "%.64s", line + 9);    // and passwords up to 64
        }
    }
    fclose(f);
    return ssid[0] != '\0';
}

static bool open_socket(void)
{
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        return false;
    }
    struct sockaddr_in addr = { 0 };
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(VOICE_LAB_PORT);
    return bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0;
}

static void close_socket(void)
{
    if (sock >= 0)
    {
        close(sock);
    }
    sock = -1;
    listening = false;
}

void voice_lab_init(const char *storage_path)
{
    if (storage_path != NULL)
    {
        snprintf(wifi_path, sizeof(wifi_path), "%s/" WIFI_FILE, storage_path);
    }
}

bool voice_lab_available(void)
{
    return wifi_path[0] != '\0' && mic_start();
}

const char *voice_lab_start(void)
{
    char password[65];
    if (!read_wifi_file(password, sizeof(password)))
    {
        return "Put wifi.txt on the SD card (see the README)";
    }
    remote_stop();      // its hotspot uses the WiFi too
    if (!rcat_hal_wifi_connect(ssid, password))
    {
        return "WiFi didn't start";
    }
    static bool listener_added;
    if (!listener_added)
    {
        listener_added = mic_add_listener(mic_cb, NULL);
    }
    started = true;
    return NULL;
}

bool voice_lab_running(void)
{
    return started;
}

void voice_lab_stop(void)
{
    __atomic_store_n(&destination, 0, __ATOMIC_RELEASE);
    close_socket();
    rcat_hal_wifi_disconnect();
    started = false;
}

void voice_lab_poll(uint32_t now_ms)
{
    char ip[16];
    if (!started || !rcat_hal_wifi_address(ip, sizeof(ip)))
    {
        return;
    }
    if (!listening)
    {
        // The socket can only be opened once the network is up.
        listening = open_socket();
        if (!listening)
        {
            close_socket();
            return;
        }
    }

    char buf[32];
    struct sockaddr_in from;
    socklen_t from_len = sizeof(from);
    int len;
    while ((len = recvfrom(sock, buf, sizeof(buf), MSG_DONTWAIT, (struct sockaddr *)&from, &from_len)) > 0)
    {
        if (len >= (int)strlen(HELLO) && memcmp(buf, HELLO, strlen(HELLO)) == 0)
        {
            uint64_t dest = ((uint64_t)from.sin_addr.s_addr << 16) | from.sin_port;
            if (dest != destination)
            {
                printf("voice lab: streaming to %s port %u\n", inet_ntoa(from.sin_addr), ntohs(from.sin_port));
            }
            __atomic_store_n(&destination, dest, __ATOMIC_RELEASE);
            last_hello_ms = now_ms;
        }
        from_len = sizeof(from);
    }

    if (destination != 0 && now_ms - last_hello_ms > HELLO_TIMEOUT_MS)
    {
        printf("voice lab: no hello for a while, stopped streaming\n");
        __atomic_store_n(&destination, 0, __ATOMIC_RELEASE);
    }

    if (now_ms - last_report_ms >= REPORT_MS && destination != 0)
    {
        printf("voice lab: sent %lu packets, %lu failed (last error %d)\n",
               (unsigned long)sent_count, (unsigned long)failed_count, last_errno);
        last_report_ms = now_ms;
    }
}

void voice_lab_status(char *text, size_t len)
{
    char ip[16];
    uint64_t dest = destination;
    if (!started)
    {
        snprintf(text, len, "Off");
    }
    else if (!rcat_hal_wifi_address(ip, sizeof(ip)))
    {
        snprintf(text, len, "Connecting to\n%s...", ssid);
    }
    else if (dest == 0)
    {
        snprintf(text, len, "%s\nWaiting for the monitor", ip);
    }
    else
    {
        struct in_addr pc = { .s_addr = (uint32_t)(dest >> 16) };
        snprintf(text, len, "%s\nStreaming to %s", ip, inet_ntoa(pc));
    }
}

#else   // no network or filesystem

void voice_lab_init(const char *storage_path) {}
bool voice_lab_available(void) { return false; }
bool voice_lab_running(void) { return false; }
const char *voice_lab_start(void) { return "No WiFi"; }
void voice_lab_stop(void) {}
void voice_lab_poll(uint32_t now_ms) {}
void voice_lab_status(char *text, size_t len) { snprintf(text, len, "Off"); }
void voice_lab_send(uint16_t kind, uint32_t sample_rate, const int16_t *samples, size_t count) {}

#endif
