#ifndef WIFI_SSID
#define WIFI_SSID "realme C55"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "Woah1234"
#endif

#define TEST_TCP_SERVER_IP "10.106.197.163"

#include <string>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <algorithm>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include "hardware/flash.h"
#include "hardware/sync.h"

#if !defined(TEST_TCP_SERVER_IP)
#error TEST_TCP_SERVER_IP not defined
#endif

#define TCP_PORT 8200
#define DEBUG_printf printf
#define BUF_SIZE 4096   // increase a bit to comfortably hold headers+body

#define BUTTON_PIN 10   // GPIO pin where the button is connected
#define LED_PIN 7       // Onboard LED for Pico

const uint32_t FLASH_TARGET_OFFSET = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE;



// global app state
uint32_t last_id = -1;
std::string last_response = "";
bool occupied = false; // initial state
int blinking_state = 0; // 0=normal, 1=blinking

static const uint32_t MAGIC = 0xDEADBEEF;

struct FlashData 
{
    uint32_t magic;
    uint32_t last_id;
};

typedef struct TCP_CLIENT_T_ 
{
    struct tcp_pcb *tcp_pcb;
    ip_addr_t remote_addr;
    uint8_t buffer[BUF_SIZE];
    int buffer_len;
    int sent_len;
    bool connected;
} TCP_CLIENT_T;

// ---------- flash storage ----------
void save_last_id(uint32_t id)
{
    FlashData data;
    data.magic = MAGIC;
    data.last_id = id;

    uint32_t ints = save_and_disable_interrupts();

    // Erase a full sector (4 KB)
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);

    // Write exactly sizeof(FlashData) bytes
    flash_range_program(FLASH_TARGET_OFFSET, (const uint8_t*)&data, sizeof(FlashData));

    restore_interrupts(ints);

    printf("Saved last_id=%u to flash.\n", id);
}


uint32_t load_last_id()
{
    const FlashData* data = reinterpret_cast<const FlashData*>(XIP_BASE + FLASH_TARGET_OFFSET);

    // If flash never written → magic is 0xFFFFFFFF or garbage
    if (data->magic != MAGIC)
    {
        printf("Flash uninitialized, using last_id=-1.\n");
        return -1;
    }

    printf("Flash OK, loaded last_id=%u\n", data->last_id);
    return data->last_id;
}



// ---------- helpers ----------
static std::string strip_http_headers(const std::string &resp)
{
    size_t pos = resp.find("\r\n\r\n");
    if (pos != std::string::npos) return resp.substr(pos + 4);
    return resp;
}

static int parse_content_length(const char *headers, int headers_len) 
{
    // naive parse for "Content-Length: <num>"
    const char *p = headers;
    const char *end = headers + headers_len;
    while (p < end) 
    {
        // find end of this header line
        const char *line_end = (const char*)memchr(p, '\n', end - p);
        if (!line_end) break;
        // line is [p .. line_end)
        // look for "Content-Length" (case-insensitive not required for your server)
        if ((line_end - p) >= 15 && strncasecmp(p, "Content-Length:", 15) == 0) 
        {
            // skip label
            const char *val = p + 15;
            // skip spaces
            while (val < line_end && (*val == ' ' || *val == '\t')) ++val;
            int content_len = 0;
            if (sscanf(val, "%d", &content_len) == 1) return content_len;
        }
        p = line_end + 1;
    }
    return -1;
}


// ---------- lwIP callbacks ----------
static err_t tcp_client_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) 
{
    (void)tpcb;
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    DEBUG_printf("tcp_client_sent %u\n", len);
    state->sent_len += len;
    return ERR_OK;
}

static err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err) 
{
    (void)tpcb;
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    if (err != ERR_OK) 
    {
        printf("connect failed %d\n", err);
        state->connected = false;
        return err;
    }
    state->connected = true;
    DEBUG_printf("tcp connected\n");
    return ERR_OK;
}

static void tcp_client_err(void *arg, err_t err) 
{
    (void)err;
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    DEBUG_printf("tcp_client_err %d\n", err);
    state->connected = false;
}

// This recv accumulates bytes into state->buffer, then when it has the full HTTP body
// (determined by Content-Length header) it stores only the body into last_response.
err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) 
{
    TCP_CLIENT_T* state = (TCP_CLIENT_T*)arg;
    if (!p) 
    {
        DEBUG_printf("Connection closed by server\n");
        state->connected = false;
        return ERR_OK;
    }

    cyw43_arch_lwip_check();

    if (p->tot_len > 0) 
    {
        // limit copy so we don't overflow the buffer
        size_t copy_len = std::min<size_t>(p->tot_len, BUF_SIZE - 1 - state->buffer_len);
        state->buffer_len += pbuf_copy_partial(p, state->buffer + state->buffer_len, copy_len, 0);

        // Inform lwIP we've received it (this must be called inside lwip begin/end for background threadsafe mode)
        cyw43_arch_lwip_begin();
        tcp_recved(tpcb, p->tot_len);
        cyw43_arch_lwip_end();
    }

    pbuf_free(p);

    // Null-terminate for safe string operations (we keep buffer_len unchanged until we consume)
    state->buffer[state->buffer_len] = '\0';

    // If we haven't seen the header-body separator yet, don't try to parse length
    char *header_end = (char*)strstr((char*)state->buffer, "\r\n\r\n");
    if (!header_end) 
    {
        // wait for more data
        return ERR_OK;
    }

    // compute header length
    int headers_len = (int)(header_end - (char*)state->buffer) + 4; // include the "\r\n\r\n"

    // parse content length header
    int content_len = parse_content_length((char*)state->buffer, headers_len);
    if (content_len < 0) 
    {
        // no content-length: assume the rest after headers is the body available right now
        // return everything after headers as body
        char *body_ptr = (char*)state->buffer + headers_len;
        last_response = std::string(body_ptr);
        return ERR_OK;
    }

    // check if we have the full body yet
    int total_needed = headers_len + content_len;
    if (state->buffer_len >= total_needed) 
    {
        // extract body only (exact content_len bytes)
        last_response = std::string((char*)state->buffer + headers_len, content_len);

        // leave the buffer contents as-is for debug; the caller/waiter will reset state->buffer_len when it consumes response
    } else 
    {
        // wait for remaining bytes
    }

    return ERR_OK;
}


// ---------- connection helpers ----------
static bool tcp_client_open(TCP_CLIENT_T *state) 
{
    if (!state) return false;

    DEBUG_printf("Connecting to %s port %u\n", ip4addr_ntoa(&state->remote_addr), TCP_PORT);

    // if we already have a pcb, free it first
    if (state->tcp_pcb) 
    {
        tcp_arg(state->tcp_pcb, NULL);
        tcp_sent(state->tcp_pcb, NULL);
        tcp_recv(state->tcp_pcb, NULL);
        tcp_err(state->tcp_pcb, NULL);
        tcp_close(state->tcp_pcb);
        state->tcp_pcb = NULL;
    }

    state->tcp_pcb = tcp_new_ip_type(IP_GET_TYPE(&state->remote_addr));
    if (!state->tcp_pcb) 
    {
        DEBUG_printf("failed to create pcb\n");
        return false;
    }

    tcp_arg(state->tcp_pcb, state);
    tcp_sent(state->tcp_pcb, tcp_client_sent);
    tcp_recv(state->tcp_pcb, tcp_client_recv);
    tcp_err(state->tcp_pcb, tcp_client_err);

    state->buffer_len = 0;
    state->sent_len = 0;

    cyw43_arch_lwip_begin();
    err_t res = tcp_connect(state->tcp_pcb, &state->remote_addr, TCP_PORT, tcp_client_connected);
    cyw43_arch_lwip_end();

    return res == ERR_OK;
}

static TCP_CLIENT_T* tcp_client_init(void) 
{
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)calloc(1, sizeof(TCP_CLIENT_T));
    if (!state) 
    {
        DEBUG_printf("failed to allocate state\n");
        return NULL;
    }
    ip4addr_aton(TEST_TCP_SERVER_IP, &state->remote_addr);
    state->buffer_len = 0;
    state->tcp_pcb = NULL;
    state->connected = false;
    state->sent_len = 0;
    return state;
}

static err_t tcp_client_close(TCP_CLIENT_T *state) 
{
    if (!state) return ERR_ARG;
    if (state->tcp_pcb) 
    {
        tcp_arg(state->tcp_pcb, NULL);
        tcp_sent(state->tcp_pcb, NULL);
        tcp_recv(state->tcp_pcb, NULL);
        tcp_err(state->tcp_pcb, NULL);
        err_t err = tcp_close(state->tcp_pcb);
        if (err != ERR_OK) 
        {
            tcp_abort(state->tcp_pcb);
        }
        state->tcp_pcb = NULL;
    }
    state->connected = false;
    state->buffer_len = 0;
    state->sent_len = 0;
    return ERR_OK;
}


// ---------- sending ----------
bool tcp_client_send_data(TCP_CLIENT_T *state, const char *data, size_t len) 
{
    if (!state || !state->tcp_pcb || !state->connected) 
    {
        printf("Cannot send: not connected.\n");
        return false;
    }

    cyw43_arch_lwip_begin();
    err_t err = tcp_write(state->tcp_pcb, data, (u16_t)len, TCP_WRITE_FLAG_COPY);
    if (err == ERR_OK) tcp_output(state->tcp_pcb);
    cyw43_arch_lwip_end();

    if (err != ERR_OK) 
    {
        printf("tcp_write failed: %d\n", err);
        return false;
    }
    return true;
}

err_t tcp_send_post(TCP_CLIENT_T *state, const char *host, const char *path, const char *body) 
{
    if (!state || !state->tcp_pcb || !host || !path || !body) return ERR_ARG;

    char request[1024];
    int len = snprintf(request, sizeof(request),
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        path,
        host,
        strlen(body),
        body
    );

    if (len <= 0 || (size_t)len >= sizeof(request)) 
    {
        printf("Request too long!\n");
        return ERR_BUF;
    }

    if (!tcp_client_send_data(state, request, (size_t)len)) 
    {
        return ERR_IF;
    }
    DEBUG_printf("POST request sent (len=%d)\n", len);
    return ERR_OK;
}


// Wait for the server response body (returns body only). This function clears last_response for next call.
std::string tcp_send_request_and_wait(TCP_CLIENT_T *state, const std::string &path, const std::string &body, int timeout_ms = 5000) 
{
    if (!state) return "";

    // ensure connected
    if (!state->connected || !state->tcp_pcb) 
    {
        printf("TCP not connected, (re)connecting...\n");
        if (!tcp_client_open(state)) 
        {
            printf("Failed to (re)connect\n");
            return "";
        }
        // give some time to establish connection callback (connected flag)
        int wait = 0;
        while (!state->connected && wait < 1000) 
        {
            sleep_ms(10);
            wait += 10;
        }
        if (!state->connected) 
        {
            printf("Connection didn't establish\n");
            return "";
        }
    }

    // reset any previous leftover
    last_response.clear();
    state->buffer_len = 0;

    // send
    if (tcp_send_post(state, ip4addr_ntoa(&state->remote_addr), path.c_str(), body.c_str()) != ERR_OK) 
    {
        printf("tcp_send_post failed\n");
        return "";
    }

    // wait for last_response (which will be set by recv once full body received)
    uint32_t start = to_ms_since_boot(get_absolute_time());
    while (last_response.empty()) 
    {
        sleep_ms(10);
        if (to_ms_since_boot(get_absolute_time()) - start > (uint32_t)timeout_ms) 
        {
            printf("Timed out waiting for response\n");
            return "";
        }
    }

    // copy response (body only)
    std::string resp = last_response;
    last_response.clear();

    // reset buffer_len so next request starts fresh
    state->buffer_len = 0;

    return resp;
}


// ---------- button and run logic ----------
void button_pressed(TCP_CLIENT_T *state) 
{
    occupied = !occupied;
    std::string body = "{\"id\": " + std::to_string(last_id) + ", \"occupied\": " + (occupied ? "true" : "false") + "}";
    printf("Sending occupied update: %s\n", body.c_str());

    std::string resp = tcp_send_request_and_wait(state, "/api/pico-occupied", body, 5000);
    if (!resp.empty()) 
    {
        printf("Server response body: %s\n", resp.c_str());
    } else 
    {
        printf("No response / timeout when sending occupied.\n");
    }

    gpio_put(LED_PIN, occupied ? 1 : 0);
}

void led_blink_task(int& time_passed_ms, bool& led_state) 
{
    if (blinking_state == 1) 
    {
        if (time_passed_ms >= 500) // toggle every 500 ms
        {
            gpio_put(LED_PIN, led_state ? 1 : 0);
        }
    } 
    else 
    {
        // ensure LED reflects occupied state when not blinking
        gpio_put(LED_PIN, occupied ? 1 : 0);
        time_passed_ms = 0;
        led_state = false;
    }
}


void setup_button(TCP_CLIENT_T *state) 
{
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);

    bool last_button_state = false; 

    int heartbeatTimer = 0;
    const int HeatbeatAmount = 6000; // 60 seconds
    int led_time_passed_ms = 0;
    bool led_state_blink = false;
    while (true) 
    {
        bool current_state = gpio_get(BUTTON_PIN);
        if (last_button_state && !current_state) // falling edge
        { 
            button_pressed(state);
        }
        last_button_state = current_state;
        heartbeatTimer += 1;

        if (heartbeatTimer >= HeatbeatAmount) 
        {
            // send heartbeat
            std::string body = "{\"id\": " + std::to_string(last_id) + "}";
            printf("Sending heartbeat: %s\n", body.c_str());

            std::string resp = tcp_send_request_and_wait(state, "/api/pico-heartbeat", body, 5000);
            if (!resp.empty()) 
            {
                printf("Server heartbeat response body: %s\n", resp.c_str());
                
                // look in a json for {"state":integer}
                int server_state = -1;
                if (sscanf(resp.c_str(), "{\"state\":%d}", &server_state) == 1) 
                {
                    if (server_state == 1 && blinking_state == 0) 
                    {
                        // start blinking
                        blinking_state = 1;
                        printf("Starting LED blinking.\n");
                    } 
                    else if (server_state == 0 && blinking_state == 1) 
                    {
                        // stop blinking
                        blinking_state = 0;
                        gpio_put(LED_PIN, occupied ? 1 : 0);
                        printf("Stopping LED blinking.\n");
                    }
                } else 
                {
                    printf("Couldn't parse state from heartbeat response.\n");
                }

            } else 
            {
                printf("No response / timeout when sending heartbeat.\n");
            }

            heartbeatTimer = 0;
        }

    
        led_time_passed_ms += 100;
        led_blink_task(led_time_passed_ms, led_state_blink);

        sleep_ms(100); // debounce
    }
}

void run_tcp_client_test(void) 
{
    TCP_CLIENT_T *state = tcp_client_init();
    if (!state) return;

    if (!tcp_client_open(state)) 
    {
        free(state);
        return;
    }

    // get an id from server
    std::string req = "{\"last_id\": " + std::to_string(last_id) + "}";
    std::string resp = tcp_send_request_and_wait(state, "/api/pico-connect", req, 3000);
    if (!resp.empty()) 
    {
        // parse body like {"id":12}
        int new_id = -1;
        if (sscanf(resp.c_str(), "{\"id\":%d}", &new_id) == 1) 
        {
            last_id = new_id;
            printf("Got new id: %d\n", last_id);
        } else 
        {
            printf("Couldn't parse id from: %s\n", resp.c_str());
        }
    } else 
    {
        printf("No response for /api/pico-connect\n");
    }

    // leave connection open for button usage
    // do not close here; setup_button will reuse the same state
    // free(state) will be done on program exit
}

int main() 
{
    stdio_init_all();

    if (cyw43_arch_init()) 
    {
        DEBUG_printf("failed to initialise\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();

    // Connect to Wi-Fi (retry)
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 10000)) 
    {
        printf("failed to connect, retrying...\n");
    }
    printf("Connected to Wi-Fi.\n");

    // Load last_id from flash
    uint32_t loaded = load_last_id();
    printf("Loaded last_id = %u from flash\n", loaded);

    run_tcp_client_test();

    // Save last_id to flash
    // After handshake, if server gives a new ID:
    if (last_id != loaded)
    {
        last_id = loaded;
        save_last_id(last_id);
    }

    // Init TCP client for interactive use
    TCP_CLIENT_T *state = tcp_client_init();
    if (!state || !tcp_client_open(state)) 
    {
        printf("Failed to init TCP client\n");
        return 1;
    }

    // Start button loop (blocking)
    setup_button(state);

    // never reached in normal flow, but keep cleanup for completeness
    tcp_client_close(state);
    free(state);
    cyw43_arch_deinit();
    return 0;
}
