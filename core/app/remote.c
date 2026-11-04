/*
 * Phone remote (see remote.h).
 *
 * Two small servers, both on non-blocking BSD sockets and run from
 * remote_poll():
 *
 *   DNS, on port 53, answers every name with the board's own address. Phones
 *   check for a working internet connection when they join a network, by
 *   fetching a known page; when that comes back as something else, they
 *   take it to be a sign-in page and show it. That's how the remote's page
 *   opens by itself.
 *
 *   HTTP, on port 80, serves the page (remote_page.c) and a small JSON API
 *   that it calls:
 *
 *     GET  /api/info      pattern and reaction names, which don't change
 *     GET  /api/state     the devices and what's playing
 *     POST /api/level?d=<slot>&m=<motor>&v=<level>[&release=1]
 *     POST /api/play?d=<slot, or -1 for every device>&p=<pattern, or -1 to stop>
 *     POST /api/react?m=<mode>
 *     POST /api/speed?v=<percent>
 *     POST /api/scan?on=<1 or 0>
 *     POST /api/connect?d=<slot>      connect, or disconnect if connected
 *     POST /api/stop                  everything, as the Stop button
 *
 *   The POSTs answer with the state, so the page shows the change at once.
 *   Each request has its own connection ("Connection: close"), which keeps
 *   this simple, and is quick enough on the board's own network.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/lovense.h"
#include "app/patterns.h"
#include "app/reactive.h"
#include "app/remote.h"
#include "app/settings.h"
#include "audio/voice_lab.h"
#include "hal.h"
#include "lvgl.h"

#if RCAT_HAVE_NETWORK

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

#ifndef HTTP_PORT
#define HTTP_PORT           80
#endif
#ifndef DNS_PORT
#define DNS_PORT            53
#endif
#define MAX_CLIENTS         4
#define REQUEST_MAX         1024
#define BODY_MAX            3072
#define CLIENT_TIMEOUT_MS   5000
#define SCAN_MS             30000       // a search from the page runs this long
#define MAX_KNOWN           8           // phones that have opened the page
#define PASSWORD_LEN        10

extern const char remote_page[];
extern const size_t remote_page_len;

typedef struct
{
    int fd;                 // -1 when the slot's free
    uint32_t since_ms;
    uint32_t addr;
    size_t in_len;
    char in[REQUEST_MAX + 1];
    bool responding;
    char head[256];
    size_t head_len;
    const char *body;
    size_t body_len;
    size_t sent;            // of the head, then the body
    char json[BODY_MAX];
} client_t;

static bool running;
static int http_sock = -1;
static int dns_sock = -1;
static client_t *clients;           // MAX_CLIENTS of them, from LVGL's memory (PSRAM)
static uint32_t known[MAX_KNOWN];   // addresses that have opened the page
static size_t known_next;
static uint32_t scan_until;
static bool scan_ours;              // the page started the search, so it stops it
static const char *notice;          // for the page, after a request that couldn't be done
static uint32_t now;
static char join_text[80];

/* ----- The hotspot's name and password ----- */

static char random_char(const char *alphabet)
{
    size_t n = strlen(alphabet);
    uint32_t limit = UINT32_MAX - UINT32_MAX % n;     // so each one is equally likely
    uint32_t r;
    do
    {
        r = rcat_hal_random();
    } while (r >= limit);
    return alphabet[r % n];
}

/* Letters and numbers that can't be mistaken for one another */
static void make_credentials(bool new_name)
{
    const settings_t *s = settings_get();
    char ssid[SETTINGS_REMOTE_LEN];
    char password[SETTINGS_REMOTE_LEN];
    snprintf(ssid, sizeof(ssid), "%s", s->remote_ssid);
    if (new_name || ssid[0] == '\0')
    {
        strcpy(ssid, "RCAT-");
        for (int i = 0; i < 4; i++)
        {
            ssid[5 + i] = random_char("ABCDEFGHJKMNPQRSTUVWXYZ23456789");
        }
        ssid[9] = '\0';
    }
    for (int i = 0; i < PASSWORD_LEN; i++)
    {
        password[i] = random_char("abcdefghjkmnpqrstuvwxyz23456789");
    }
    password[PASSWORD_LEN] = '\0';
    settings_set_remote(ssid, password);
}

static void ensure_credentials(void)
{
    if (settings_get()->remote_ssid[0] == '\0' || strlen(settings_get()->remote_password) < 8)
    {
        make_credentials(true);
    }
    snprintf(join_text, sizeof(join_text), "WIFI:T:WPA;S:%s;P:%s;;", settings_get()->remote_ssid,
             settings_get()->remote_password);
}

const char *remote_ssid(void)
{
    ensure_credentials();
    return settings_get()->remote_ssid;
}

const char *remote_password(void)
{
    ensure_credentials();
    return settings_get()->remote_password;
}

const char *remote_join_text(void)
{
    ensure_credentials();
    return join_text;
}

/* ----- Sockets ----- */

static int open_socket(int type, uint16_t port)
{
    int fd = socket(AF_INET, type, 0);
    if (fd < 0)
    {
        return -1;
    }
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in addr = { 0 };
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0
        || (type == SOCK_STREAM && listen(fd, MAX_CLIENTS) != 0))
    {
        close(fd);
        return -1;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    return fd;
}

/* Say once whether a socket opened, or why it didn't. */
static void report_socket(const char *what, int fd)
{
    static int last_errno[2] = { -1, -1 };
    int which = strcmp(what, "DNS") == 0;
    if (fd >= 0)
    {
        printf("remote: %s ready\n", what);
        last_errno[which] = -1;
    }
    else if (errno != last_errno[which])
    {
        printf("remote: %s socket didn't open (errno %d), trying again\n", what, errno);
        last_errno[which] = errno;
    }
}

static void close_client(client_t *c)
{
    if (c->fd >= 0)
    {
        close(c->fd);
    }
    c->fd = -1;
}

static void close_all(void)
{
    if (clients != NULL)
    {
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            close_client(&clients[i]);
        }
    }
    if (http_sock >= 0)
    {
        close(http_sock);
    }
    if (dns_sock >= 0)
    {
        close(dns_sock);
    }
    http_sock = -1;
    dns_sock = -1;
}

/* ----- DNS: every name is the board ----- */

static int dns_logged;

static void answer_dns(void)
{
    uint8_t buf[512];
    struct sockaddr_in from;
    socklen_t from_len = sizeof(from);
    int len;
    while ((len = recvfrom(dns_sock, buf, sizeof(buf) - 16, MSG_DONTWAIT, (struct sockaddr *)&from, &from_len)) > 0)
    {
        from_len = sizeof(from);
        // A query (QR 0, opcode 0) with at least one question
        if (len < 12 || (buf[2] & 0xF8) != 0 || (buf[4] << 8 | buf[5]) == 0)
        {
            continue;
        }
        // Find the end of the first question: its name, then type and class.
        int at = 12;
        while (at < len && buf[at] != 0)
        {
            at += buf[at] + 1;
        }
        at += 1 + 4;
        if (at > len)
        {
            continue;
        }
        uint16_t type = (uint16_t)(buf[at - 4] << 8 | buf[at - 3]);
        bool is_a = type == 1 && buf[at - 2] == 0 && buf[at - 1] == 1;     // A, IN
        if (dns_logged < 20)
        {
            // The first few, to see what phones look up
            char name[64];
            size_t n = 0;
            for (int i = 12; i < at - 5 && buf[i] != 0 && n < sizeof(name) - 1;)
            {
                int label = buf[i++];
                if (n > 0 && n < sizeof(name) - 1)
                {
                    name[n++] = '.';
                }
                for (int k = 0; k < label && i < at - 5 && n < sizeof(name) - 1; k++)
                {
                    name[n++] = (char)buf[i++];
                }
            }
            name[n] = '\0';
            printf("remote: DNS %s (type %u) from %s\n", name, type, inet_ntoa(from.sin_addr));
            dns_logged++;
        }

        buf[2] = 0x80 | (buf[2] & 0x01);    // a response, keeping RD
        buf[3] = 0x80;                      // RA, no error
        buf[4] = 0;
        buf[5] = 1;                         // the one question
        buf[6] = 0;
        buf[7] = is_a ? 1 : 0;              // an answer, for an address; none for anything else
        memset(buf + 8, 0, 4);
        if (is_a)
        {
            struct in_addr ip;
            inet_aton(RCAT_HAL_HOTSPOT_ADDRESS, &ip);
            const uint8_t answer[12] = {
                0xC0, 0x0C,         // the name in the question
                0, 1, 0, 1,         // A, IN
                0, 0, 0, 10,        // for 10 seconds
                0, 4,
            };
            memcpy(buf + at, answer, sizeof(answer));
            memcpy(buf + at + sizeof(answer), &ip.s_addr, 4);
            at += sizeof(answer) + 4;
        }
        sendto(dns_sock, buf, at, MSG_DONTWAIT, (struct sockaddr *)&from, sizeof(from));
    }
}

/* ----- JSON ----- */

typedef struct
{
    char *p;
    size_t len;
    size_t cap;
} json_t;

static void jprintf(json_t *j, const char *fmt, ...)
{
    if (j->len >= j->cap)
    {
        return;
    }
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(j->p + j->len, j->cap - j->len, fmt, args);
    va_end(args);
    if (n > 0)
    {
        j->len += (size_t)n;
        if (j->len > j->cap)
        {
            j->len = j->cap;
        }
    }
}

static void jstring(json_t *j, const char *s)
{
    jprintf(j, "\"");
    for (; *s != '\0'; s++)
    {
        unsigned char ch = (unsigned char)*s;
        if (ch == '"' || ch == '\\')
        {
            jprintf(j, "\\%c", ch);
        }
        else if (ch < 0x20)
        {
            jprintf(j, "\\u%04x", ch);
        }
        else
        {
            jprintf(j, "%c", ch);
        }
    }
    jprintf(j, "\"");
}

static const char *state_name(lovense_device_state_t state)
{
    switch (state)
    {
    case LOVENSE_CONNECTING: return "connecting";
    case LOVENSE_CONNECTED: return "connected";
    case LOVENSE_FAILED: return "failed";
    case LOVENSE_LOST: return "lost";
    default: return "found";
    }
}

static void write_state(json_t *j)
{
    char status[40] = "";
    if (reactive_mode() != REACTIVE_OFF)
    {
        reactive_status(status, sizeof(status));
    }
    jprintf(j, "{\"scan\":%d,\"speed\":%u,\"hold\":%d,\"react\":%d,\"status\":", lovense_scanning() ? 1 : 0,
            lovense_speed(), settings_get()->lovense_release_to_zero ? 0 : 1, (int)reactive_mode());
    jstring(j, status);
    jprintf(j, ",\"notice\":");
    jstring(j, notice != NULL ? notice : "");
    notice = NULL;
    jprintf(j, ",\"devices\":[");
    bool first = true;
    for (size_t i = 0; i < LOVENSE_MAX_DEVICES; i++)
    {
        const lovense_device_t *d = lovense_device(i);
        if (d == NULL)
        {
            continue;
        }
        jprintf(j, "%s{\"slot\":%u,\"name\":", first ? "" : ",", (unsigned)i);
        first = false;
        jstring(j, d->name);
        jprintf(j, ",\"model\":");
        jstring(j, d->model != NULL ? d->model : d->name);
        jprintf(j, ",\"state\":\"%s\",\"battery\":%d,\"pattern\":%d,\"driven\":%d,\"motors\":[",
                state_name(d->state), d->battery, d->pattern, lovense_driven(d) ? 1 : 0);
        for (uint8_t m = 0; m < d->motor_count; m++)
        {
            const lovense_motor_t *motor = &d->motors[m];
            jprintf(j, "%s{\"kind\":\"%s\",\"max\":%u,\"level\":%u,\"cap\":%u}", m ? "," : "",
                    lovense_motor_name(motor->kind), motor->max, motor->level, motor->cap);
        }
        jprintf(j, "]}");
    }
    jprintf(j, "]}");
}

static void write_info(json_t *j)
{
    jprintf(j, "{\"patterns\":[");
    for (size_t i = 0; i < patterns_count(); i++)
    {
        jprintf(j, "%s", i ? "," : "");
        jstring(j, patterns_name(i));
    }
    jprintf(j, "],\"modes\":[");
    bool first = true;
    for (int m = REACTIVE_OFF + 1; m < REACTIVE_COUNT; m++)
    {
        if (reactive_available((reactive_mode_t)m))
        {
            jprintf(j, "%s[%d,", first ? "" : ",", m);
            jstring(j, reactive_name((reactive_mode_t)m));
            jprintf(j, "]");
            first = false;
        }
    }
    jprintf(j, "],\"max_connected\":%d}", LOVENSE_MAX_CONNECTED);
}

/* ----- Requests ----- */

/* The value of name in a query string ("a=1&b=2"), or fallback. */
static int query_int(const char *query, const char *name, int fallback)
{
    size_t n = strlen(name);
    for (const char *q = query; q != NULL && *q != '\0';)
    {
        if (strncmp(q, name, n) == 0 && q[n] == '=')
        {
            return atoi(q + n + 1);
        }
        q = strchr(q, '&');
        if (q != NULL)
        {
            q++;
        }
    }
    return fallback;
}

static bool is_connected(int slot)
{
    const lovense_device_t *d = slot >= 0 ? lovense_device((size_t)slot) : NULL;
    return d != NULL && d->state == LOVENSE_CONNECTED;
}

static int connected_count(void)
{
    int n = 0;
    for (int i = 0; i < LOVENSE_MAX_DEVICES; i++)
    {
        n += is_connected(i);
    }
    return n;
}

/* Carry out an API command. Returns false if there's no such command. */
static bool run_command(const char *name, const char *query)
{
    if (strcmp(name, "level") == 0)
    {
        int slot = query_int(query, "d", -1);
        int motor = query_int(query, "m", 0);
        int level = query_int(query, "v", 0);
        if (is_connected(slot) && motor >= 0 && level >= 0)
        {
            // Let go, with Motor Hold off: back to 0, as on the screen
            if (query_int(query, "release", 0) && settings_get()->lovense_release_to_zero
                && !lovense_driven(lovense_device((size_t)slot)))
            {
                level = 0;
            }
            lovense_set_level((size_t)slot, (uint8_t)motor, (uint8_t)(level > 255 ? 255 : level));
        }
    }
    else if (strcmp(name, "play") == 0)
    {
        int slot = query_int(query, "d", -1);
        int pattern = query_int(query, "p", -1);
        if (connected_count() == 0)
        {
            notice = "Connect a device first";
        }
        for (int i = 0; i < LOVENSE_MAX_DEVICES; i++)
        {
            if (is_connected(i) && (slot < 0 || slot == i))
            {
                lovense_play((size_t)i, pattern);
            }
        }
    }
    else if (strcmp(name, "react") == 0)
    {
        int mode = query_int(query, "m", REACTIVE_OFF);
        if (mode < 0 || mode >= REACTIVE_COUNT || !reactive_available((reactive_mode_t)mode))
        {
            mode = REACTIVE_OFF;
        }
        reactive_set_mode((reactive_mode_t)mode);
        lovense_set_reactive(mode != REACTIVE_OFF);
    }
    else if (strcmp(name, "speed") == 0)
    {
        int speed = query_int(query, "v", 100);
        lovense_set_speed((uint16_t)(speed < 25 ? 25 : speed > 300 ? 300 : speed));
    }
    else if (strcmp(name, "scan") == 0)
    {
        if (query_int(query, "on", 1))
        {
            if (!lovense_scanning())
            {
                if (lovense_scan_start())
                {
                    scan_ours = true;
                }
                else
                {
                    notice = "No Bluetooth";
                }
            }
            scan_until = now + SCAN_MS;
        }
        else if (scan_ours)
        {
            lovense_scan_stop();
            scan_ours = false;
            scan_until = 0;
        }
    }
    else if (strcmp(name, "connect") == 0)
    {
        int slot = query_int(query, "d", -1);
        const lovense_device_t *d = slot >= 0 ? lovense_device((size_t)slot) : NULL;
        if (d != NULL)
        {
            bool connecting = d->state != LOVENSE_CONNECTING && d->state != LOVENSE_CONNECTED;
            if (!lovense_toggle((size_t)slot) && connecting)
            {
                notice = connected_count() >= LOVENSE_MAX_CONNECTED ? "Only 3 at once" : "Wait for the other one";
            }
        }
    }
    else if (strcmp(name, "stop") == 0)
    {
        lovense_stop_all();
        reactive_set_mode(REACTIVE_OFF);
    }
    else
    {
        return false;
    }
    return true;
}

static void respond(client_t *c, const char *status, const char *type, const char *body, size_t body_len,
                    const char *extra_header)
{
    c->head_len = (size_t)snprintf(c->head, sizeof(c->head),
                                   "HTTP/1.1 %s\r\n"
                                   "Content-Type: %s\r\n"
                                   "Content-Length: %u\r\n"
                                   "Cache-Control: no-store\r\n"
                                   "Connection: close\r\n"
                                   "%s\r\n",
                                   status, type, (unsigned)body_len, extra_header != NULL ? extra_header : "");
    if (c->head_len >= sizeof(c->head))
    {
        c->head_len = sizeof(c->head) - 1;
    }
    c->body = body;
    c->body_len = body_len;
    c->sent = 0;
    c->responding = true;
}

static void respond_json(client_t *c, void (*write)(json_t *))
{
    json_t j = { c->json, 0, sizeof(c->json) };
    write(&j);
    respond(c, "200 OK", "application/json", c->json, j.len, NULL);
}

static void redirect_to_page(client_t *c)
{
    respond(c, "302 Found", "text/html", "", 0, "Location: http://" RCAT_HAL_HOTSPOT_ADDRESS "/\r\n");
}

static bool is_known(uint32_t addr)
{
    for (size_t i = 0; i < MAX_KNOWN; i++)
    {
        if (known[i] == addr)
        {
            return true;
        }
    }
    return false;
}

/* The value of a header, or "" */
static void header_value(const char *request, const char *name, char *out, size_t len)
{
    out[0] = '\0';
    size_t n = strlen(name);
    for (const char *line = strstr(request, "\r\n"); line != NULL; line = strstr(line + 2, "\r\n"))
    {
        const char *h = line + 2;
        if (strncasecmp(h, name, n) == 0 && h[n] == ':')
        {
            h += n + 1;
            while (*h == ' ')
            {
                h++;
            }
            size_t vlen = strcspn(h, "\r\n");
            snprintf(out, len, "%.*s", (int)vlen, h);
            return;
        }
    }
}

static void handle_request(client_t *c)
{
    // "GET /path?query HTTP/1.1"
    char method[8], target[200];
    if (sscanf(c->in, "%7s %199s", method, target) != 2)
    {
        respond(c, "400 Bad Request", "text/plain", "", 0, NULL);
        return;
    }
    char host[64];
    header_value(c->in, "Host", host, sizeof(host));
    host[strcspn(host, ":")] = '\0';
    char *query = strchr(target, '?');
    if (query != NULL)
    {
        *query++ = '\0';
    }
    if (strcmp(target, "/api/state") != 0)     // the page asks for that every 0.4 s
    {
        struct in_addr from = { .s_addr = c->addr };
        printf("remote: %s %s %s (host %s)\n", inet_ntoa(from), method, target, host[0] ? host : "none");
    }

    if (host[0] != '\0' && strcmp(host, RCAT_HAL_HOTSPOT_ADDRESS) != 0)
    {
        // A phone checking for the internet, or something else that isn't
        // the page. An iPhone that has seen the page is told it worked, or it
        // offers to leave the network; anything else is sent to the page.
        static const char success[] = "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>";
        if (strcmp(target, "/hotspot-detect.html") == 0 && is_known(c->addr))
        {
            respond(c, "200 OK", "text/html", success, sizeof(success) - 1, NULL);
        }
        else
        {
            redirect_to_page(c);
        }
        return;
    }

    if (strcmp(target, "/") == 0 || strcmp(target, "/index.html") == 0)
    {
        if (!is_known(c->addr))
        {
            known[known_next] = c->addr;
            known_next = (known_next + 1) % MAX_KNOWN;
        }
        respond(c, "200 OK", "text/html; charset=utf-8", remote_page, remote_page_len, NULL);
    }
    else if (strcmp(target, "/api/info") == 0)
    {
        respond_json(c, write_info);
    }
    else if (strcmp(target, "/api/state") == 0)
    {
        respond_json(c, write_state);
    }
    else if (strncmp(target, "/api/", 5) == 0 && strcmp(method, "POST") == 0 && run_command(target + 5, query))
    {
        respond_json(c, write_state);
    }
    else if (strncmp(target, "/api/", 5) == 0)
    {
        respond(c, "404 Not Found", "text/plain", "", 0, NULL);
    }
    else
    {
        redirect_to_page(c);
    }
}

/* Read what's arrived, answer once the request is all there, and send as
 * much of the answer as the connection takes. */
static void serve_client(client_t *c)
{
    if (!c->responding)
    {
        int n = recv(c->fd, c->in + c->in_len, REQUEST_MAX - c->in_len, MSG_DONTWAIT);
        if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK))
        {
            close_client(c);
            return;
        }
        if (n > 0)
        {
            c->in_len += (size_t)n;
            c->in[c->in_len] = '\0';
        }
        if (strstr(c->in, "\r\n\r\n") != NULL)
        {
            handle_request(c);      // the API takes no body, so the headers are all of it
        }
        else if (c->in_len >= REQUEST_MAX)
        {
            respond(c, "431 Request Header Fields Too Large", "text/plain", "", 0, NULL);
        }
        else
        {
            return;
        }
    }

    while (c->sent < c->head_len + c->body_len)
    {
        const char *from;
        size_t len;
        if (c->sent < c->head_len)
        {
            from = c->head + c->sent;
            len = c->head_len - c->sent;
        }
        else
        {
            from = c->body + (c->sent - c->head_len);
            len = c->head_len + c->body_len - c->sent;
        }
        int n = send(c->fd, from, len, MSG_DONTWAIT);
        if (n < 0)
        {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
            {
                close_client(c);
            }
            return;     // full for now: carry on next time
        }
        c->sent += (size_t)n;
    }
    close_client(c);    // all sent
}

static int accepted_logged;

static void accept_clients(void)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        client_t *c = &clients[i];
        if (c->fd >= 0)
        {
            continue;
        }
        struct sockaddr_in from;
        socklen_t from_len = sizeof(from);
        int fd = accept(http_sock, (struct sockaddr *)&from, &from_len);
        if (fd < 0)
        {
            return;     // nobody waiting
        }
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
        if (accepted_logged < 5)
        {
            printf("remote: connection from %s\n", inet_ntoa(from.sin_addr));
            accepted_logged++;
        }
        c->fd = fd;
        c->addr = from.sin_addr.s_addr;
        c->since_ms = now;
        c->in_len = 0;
        c->in[0] = '\0';
        c->responding = false;
    }
}

/* ----- Running it ----- */

bool remote_available(void)
{
    return true;
}

bool remote_start(void)
{
    if (running)
    {
        return true;
    }
    if (clients == NULL)
    {
        clients = lv_malloc(MAX_CLIENTS * sizeof(client_t));
        if (clients == NULL)
        {
            return false;
        }
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            clients[i].fd = -1;
        }
    }
    ensure_credentials();
    if (voice_lab_running())
    {
        voice_lab_stop();   // it uses the WiFi too
    }
    if (!rcat_hal_hotspot_start(settings_get()->remote_ssid, settings_get()->remote_password))
    {
        return false;
    }
    running = true;
    return true;
}

void remote_stop(void)
{
    if (!running)
    {
        return;
    }
    close_all();
    rcat_hal_hotspot_stop();
    running = false;
    printf("remote: off\n");
    memset(known, 0, sizeof(known));
    if (scan_ours)
    {
        lovense_scan_stop();
        scan_ours = false;
    }
    scan_until = 0;
}

bool remote_running(void)
{
    return running;
}

void remote_new_password(void)
{
    bool was_running = running;
    remote_stop();
    make_credentials(false);
    ensure_credentials();
    if (was_running)
    {
        remote_start();
    }
}

int remote_phones(void)
{
    return running ? rcat_hal_hotspot_clients() : 0;
}

void remote_poll(uint32_t now_ms)
{
    now = now_ms;
    if (scan_until != 0 && (int32_t)(now - scan_until) >= 0)
    {
        scan_until = 0;
        if (scan_ours)
        {
            lovense_scan_stop();
            scan_ours = false;
        }
    }
    if (!running)
    {
        return;
    }
    // The sockets can only be opened once the network's up.
    if (http_sock < 0)
    {
        http_sock = open_socket(SOCK_STREAM, HTTP_PORT);
        report_socket("web page", http_sock);
    }
    if (dns_sock < 0)
    {
        dns_sock = open_socket(SOCK_DGRAM, DNS_PORT);
        report_socket("DNS", dns_sock);
    }
    if (dns_sock >= 0)
    {
        answer_dns();
    }
    if (http_sock < 0)
    {
        return;
    }
    accept_clients();
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        client_t *c = &clients[i];
        if (c->fd < 0)
        {
            continue;
        }
        if (now - c->since_ms > CLIENT_TIMEOUT_MS)
        {
            close_client(c);
            continue;
        }
        serve_client(c);
    }
}

#else   // no network

bool remote_available(void) { return false; }
bool remote_start(void) { return false; }
void remote_stop(void) {}
bool remote_running(void) { return false; }
const char *remote_ssid(void) { return ""; }
const char *remote_password(void) { return ""; }
const char *remote_join_text(void) { return ""; }
void remote_new_password(void) {}
int remote_phones(void) { return 0; }
void remote_poll(uint32_t now_ms) { (void)now_ms; }

#endif
