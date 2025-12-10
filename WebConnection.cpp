#include "WebConnection.h"

std::string strip_http_headers(const std::string &resp)
{
    size_t pos = resp.find("\r\n\r\n");
    if (pos != std::string::npos) return resp.substr(pos + 4);
    return resp;
}

int parse_content_length(const char *headers, int headers_len) 
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



err_t tcp_client_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) 
{
    (void)tpcb;
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    DEBUG_printf("tcp_client_sent %u\n", len);
    state->sent_len += len;
    return ERR_OK;
}

err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err) 
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

void tcp_client_err(void *arg, err_t err) 
{
    (void)err;
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    DEBUG_printf("tcp_client_err %d\n", err);
    state->connected = false;
}

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

        // Inform lwIP we've received it 
        cyw43_arch_lwip_begin();
        tcp_recved(tpcb, p->tot_len);
        cyw43_arch_lwip_end();
    }

    pbuf_free(p);

    // Null-terminate for safe string operations 
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

    } else 
    {
        // wait for remaining bytes
    }

    return ERR_OK;
}

bool tcp_client_open(TCP_CLIENT_T *state) 
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

    // set up callbacks
    tcp_arg(state->tcp_pcb, state);
    tcp_sent(state->tcp_pcb, tcp_client_sent);
    tcp_recv(state->tcp_pcb, tcp_client_recv);
    tcp_err(state->tcp_pcb, tcp_client_err);

    // reset state
    state->buffer_len = 0;
    state->sent_len = 0;

    // initiate connection
    cyw43_arch_lwip_begin();
    err_t res = tcp_connect(state->tcp_pcb, &state->remote_addr, TCP_PORT, tcp_client_connected);
    cyw43_arch_lwip_end();

    return res == ERR_OK;
}

TCP_CLIENT_T* tcp_client_init(void) 
{
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)calloc(1, sizeof(TCP_CLIENT_T));
    if (!state) 
    {
        DEBUG_printf("failed to allocate state\n");
        return NULL;
    }
    ip4addr_aton(TCP_SERVER_IP, &state->remote_addr);
    state->buffer_len = 0;
    state->tcp_pcb = NULL;
    state->connected = false;
    state->sent_len = 0;
    return state;
}

err_t tcp_client_close(TCP_CLIENT_T *state) 
{
    if (!state) return ERR_ARG;
    if (state->tcp_pcb) 
    {
        // clean up callbacks
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
    // reset state
    state->connected = false;
    state->buffer_len = 0;
    state->sent_len = 0;
    return ERR_OK;
}



bool tcp_client_send_data(TCP_CLIENT_T *state, const char *data, size_t len) 
{
    if (!state || !state->tcp_pcb || !state->connected) 
    {
        printf("Cannot send: not connected.\n");
        return false;
    }

    // send data
    cyw43_arch_lwip_begin();
    err_t err = tcp_write(state->tcp_pcb, data, (u16_t)len, TCP_WRITE_FLAG_COPY);
    if (err == ERR_OK) tcp_output(state->tcp_pcb);
    cyw43_arch_lwip_end();

    // check for error
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

    // construct HTTP POST request
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

    // send the request
    if (!tcp_client_send_data(state, request, (size_t)len)) 
    {
        return ERR_IF;
    }

    DEBUG_printf("POST request sent (len=%d)\n", len);
    return ERR_OK;
}

std::string tcp_send_request_and_wait(TCP_CLIENT_T *state, const std::string &path, const std::string &body, int timeout_ms) 
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

        // check if connected
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
    // responsible for timeout
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



