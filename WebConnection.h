#ifndef WEBCONNECTION_H
#define WEBCONNECTION_H

#include <string>

#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "hardware/flash.h"
#include "hardware/sync.h"

#include "GlobalVariables.h"


#define BUF_SIZE 4096   //  Size for a buffer for header + body response

const uint32_t FLASH_TARGET_OFFSET = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE;


typedef struct TCP_CLIENT_T_ 
{
    struct tcp_pcb *tcp_pcb;
    ip_addr_t remote_addr;
    uint8_t buffer[BUF_SIZE];
    int buffer_len;
    int sent_len;
    bool connected;
} TCP_CLIENT_T;


// ---------- helpers ----------

// Strip HTTP headers from a response string, return body only
std::string strip_http_headers(const std::string &resp);


// Parse Content-Length header from HTTP headers. Returns -1 if not found.
int parse_content_length(const char *headers, int headers_len);


// ---------- lwIP callbacks ----------

// Called when data has been successfully sent, appends the sent_len
err_t tcp_client_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);

// Called when connection is established, sets connected flag depending on success
err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err);

// Called on fatal error, sets connected flag to false
void tcp_client_err(void *arg, err_t err);

// This recv accumulates bytes into state->buffer, then when it has the full HTTP body
// (determined by Content-Length header) it stores only the body into last_response.
err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);


// ---------- connection helpers ----------

// Initialize TCP client state
bool tcp_client_open(TCP_CLIENT_T *state);

// Initialize TCP client state
TCP_CLIENT_T* tcp_client_init(void);

// Close TCP client connection and free PCB
err_t tcp_client_close(TCP_CLIENT_T *state);


// ---------- sending ----------

// Send data over TCP connection
bool tcp_client_send_data(TCP_CLIENT_T *state, const char *data, size_t len);

// Send HTTP POST request with JSON body
// On success, the response body will be available in last_response after recv callback
err_t tcp_send_post(TCP_CLIENT_T *state, const char *host, const char *path, const char *body);


// Send HTTP POST request with JSON body
// Wait for the server response body (returns body only). This function clears last_response for next call.
// Returns empty string on timeout or error.
std::string tcp_send_request_and_wait(TCP_CLIENT_T *state, const std::string &path, const std::string &body, int timeout_ms = 5000);




#endif // WEBCONNECTION_H