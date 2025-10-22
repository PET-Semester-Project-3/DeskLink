#include "Connection.h"
#include "dbop.h"

// Static member definitions
std::vector<tCGI> Connector::m_cgi_handlers;            // Routes
std::vector<SSIHandler> Connector::m_ssi_registry;      // Dynamic variables
std::vector<const char*> Connector::m_tag_names;        // Dynamic variables, private for the C functions
std::vector<std::string> Connector::m_tag_name_storage; // Dynamic variables, private storage of the data, to keep it persistent
std::vector<PostHandler> Connector::m_post_handlers;    // POST handlers, what should happen for a given post
std::vector<PostContext> Connector::m_post_context;     // Data stored between POSTs about the POST, used as context for a function to handle it
absolute_time_t Connector::m_wifi_connected_time;       // Connection time


// Used to setup and start the server, should be used only once and only AFTER adding SSIs and CGIs
// returns 0 on success
// returns -1 on connection error (tries 3 times)
int Connector::Init(void)
{
    char hostname[sizeof(CYW43_HOST_NAME) + 4];
    memcpy(&hostname[0], CYW43_HOST_NAME, sizeof(CYW43_HOST_NAME) - 1);
    get_mac_ascii(CYW43_HAL_MAC_WLAN0, 8, 4, &hostname[sizeof(CYW43_HOST_NAME) - 1]);
    hostname[sizeof(hostname) - 1] = '\0';


    netif_set_hostname(&cyw43_state.netif[CYW43_ITF_STA], hostname);


    printf("Connecting to WiFi...\n");
    int its = 0;
    bool success = false;
    while(!success && its < 3)
    {
        if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) 
        {
        printf("failed to connect. Retrying \n");
        
        } else 
        {
        printf("Connected.\n");
        success=true;
        }
        its++;
    }

    if(!success)
    {
        printf("failed to connect. Exiting\n");
        return -1;
    }

    printf("\nReady, running httpd at %s\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));

    // start http server
    m_wifi_connected_time = get_absolute_time();

    #if LWIP_MDNS_RESPONDER
    dns_init(hostname);
    #endif

    printf("m_tag_names set up");
    m_tag_names.reserve(m_tag_name_storage.size());

    for (int i = 0; i < m_tag_name_storage.size();i++)
    {
        m_tag_names.push_back(m_tag_name_storage[i].c_str());
    }



    cyw43_arch_lwip_begin();

    httpd_init();

    http_set_cgi_handlers(m_cgi_handlers.data(), m_cgi_handlers.size());

    http_set_ssi_handler(ssi_handler_function, m_tag_names.data(), m_tag_names.size());
    sleep_ms(2000);
    cyw43_arch_lwip_end();

    return 0;

}


// Used to add CGI Handlers, which are the routes
// returns true on success
bool Connector::AddCGIHandler(tCGI& handler)
{
    m_cgi_handlers.reserve(m_cgi_handlers.capacity() + 1); // In order to prevent dynamic vector size increase, which is 2n, doubling the capacity if not enough
    m_cgi_handlers.push_back(handler);
    return true;
}

// Get the reference to the vector with CGIs (Routes)
// returns a const reference, so that it cannot be changed
const std::vector<tCGI>& Connector::GetCGIHandlers(void)
{
    return m_cgi_handlers;
}


// Used to get MAC of the device as ASCII
// returns size of the array
// returned mac is in dest_in
size_t Connector::get_mac_ascii(int idx, size_t chr_off, size_t chr_len, char *dest_in)
{
    static const char hexchr[17] = "0123456789ABCDEF";
    uint8_t mac[6];
    char *dest = dest_in;
    assert(chr_off + chr_len <= (2 * sizeof(mac)));
    cyw43_hal_get_mac(idx, mac);
    for (; chr_len && (chr_off >> 1) < sizeof(mac); ++chr_off, --chr_len) 
    {
        *dest++ = hexchr[mac[chr_off >> 1] >> (4 * (1 - (chr_off & 1))) & 0xf];
    }
    return dest - dest_in;
}


// Used to get the parameter value from a pbuf (POST buffer object)
// retuns the pointer to the buffer
// returned value is stored inside value_buf
// returns NULL on fail
char *Connector::httpd_param_value(struct pbuf *p, const char *param_name, char *value_buf, size_t value_buf_len) 
{
    size_t param_len = strlen(param_name);
    u16_t param_pos = pbuf_memfind(p, param_name, param_len, 0);
    if (param_pos != 0xFFFF) 
    {
        u16_t param_value_pos = param_pos + param_len;
        u16_t param_value_len = 0;
        u16_t tmp = pbuf_memfind(p, "&", 1, param_value_pos);
        if (tmp != 0xFFFF) 
        {
            param_value_len = tmp - param_value_pos;
        } else 
        {
            param_value_len = p->tot_len - param_value_pos;
        }
        if (param_value_len > 0 && param_value_len < value_buf_len)
        {
            char *result = (char *)pbuf_get_contiguous(p, value_buf, value_buf_len, param_value_len, param_value_pos);
            if (result)
            {
                result[param_value_len] = 0;
                return result;
            }
        }
    }
    return NULL;
}


// Used to add SSI Handler, which is a dynamic variable
// The function can fail based on LWIP_HTTPD_MAX_TAG_NAME_LEN, as it is the limit of the amount of dynamic variables
// The function can also fail in case of the tag already existing
// The function copies the handler and important information, so persistency is not needed
// returns true on success
// returns false on fail
// Be aware of LWIP_HTTPD_MAX_TAG_NAME_LEN
bool Connector::AddSSIHandler(const SSIHandler& handler) 
{
    if(m_ssi_registry.size() >= LWIP_HTTPD_MAX_TAG_NAME_LEN)
        return false;

    for (auto& h : m_ssi_registry)
        if (h.tag == handler.tag) return false;
            

    C_DeskLink("SSI added");

    m_tag_name_storage.reserve(m_tag_name_storage.capacity()+1);
    m_tag_name_storage.push_back(handler.tag);

    m_ssi_registry.reserve(m_ssi_registry.capacity() + 1); // In order to prevent dynamic vector size increase, which is 2n, doubling the capacity if not enough
    m_ssi_registry.push_back(handler);
    return true;
}


// Get the reference to the vector with SSIs (Dynamic Variables)
// returns a const reference, so that it cannot be changed
const std::vector<SSIHandler>& Connector::GetSSIHandlers(void)
{
    return m_ssi_registry;
}


// Used to clean up the connector before closing the program, removes netif
void Connector::cleanup(void)
{

#if LWIP_MDNS_RESPONDER
    mdns_resp_remove_netif(&cyw43_state.netif[CYW43_ITF_STA]);
#endif
    cyw43_arch_deinit();
}



// Are POSTs supported?
#if LWIP_HTTPD_SUPPORT_POST

// Used to add Post Handler, so what happens for a given post
// returns true on success
bool Connector::AddPostHandler(PostHandler& handler)
{
    m_post_handlers.reserve(m_post_handlers.capacity() + 1); // In order to prevent dynamic vector size increase, which is 2n, doubling the capacity if not enough
    m_post_handlers.push_back(handler);
    return true;
}


// Get the reference to the vector with POST handlers
// returns a const reference, so that it cannot be changed
const std::vector<PostHandler>& Connector::GetPostHandler(void)
{
    return m_post_handlers;
}


// Internal function to handle begining of the POST
// It creates Post Context for a given connection and stores important data
// Placeholder page is places
// Sets state of the Context connection to be BEGIN
// returns ERR_OK on success
// returns ERR_VAL on fail: No handler found; this connection already has on POST which is being processed
err_t Connector::httpd_post_begin(void *connection, const char *uri, const char *http_request,u16_t http_request_len, int content_len, char *response_uri,u16_t response_uri_len, u8_t *post_auto_wnd)
{
    for (auto& h : m_post_handlers) 
    {
        if (strcmp(uri, h.url.c_str()) == 0) 
        {
            for(auto& p : m_post_context)
            {
                if(p.connection == connection)
                    return ERR_VAL;
            }

            PostContext pc{connection, uri, BEGIN, nullptr};

            m_post_context.push_back(pc);

            snprintf(response_uri, response_uri_len, "%s", h.placeholder_page.c_str());

            *post_auto_wnd = 1;
            return ERR_OK;
        }
    }

    return ERR_VAL; // no handler found
            
}


// Internal function to handle receiving data from the POST
// Saves the buffer and changes the state of a context to RECEIVE
// returns ERR_OK on success
// returns ERR_VAL on fail, in case no context was found (which should not happened ever)
err_t Connector::httpd_post_receive_data(void *connection, struct pbuf *p) 
{
    LWIP_ASSERT("NULL pbuf", p != NULL); // could be removed

    for (auto& pc : m_post_context) 
    {
        if (pc.connection == connection && pc.ps == BEGIN)
        {
            pc.ps = RECEIVE;
            pc.buf = p;

            return ERR_OK;
        }
        // pbuf_free(p); Cannot free it here, as the lambda to determine what to do runs later on and uses it
    }

    return ERR_VAL;
}


// Internal function to handle finishing the POST 
// Sets the state to END at the start
// Runs the function in the handler, saves the result as the new uri (NULL for no change)
// If things went well, state changes to REMOVE, so it cleans up the POST buffer and erases currect context, so that a user can do another POST
void Connector::httpd_post_finished(void *connection, char *response_uri, u16_t response_uri_len) 
{
    for (int i = 0; i < m_post_context.size(); i++) 
    {
        PostContext& pc = m_post_context[i];

        if (pc.connection == connection && pc.ps == RECEIVE)
        {
            pc.ps = END;

            for(PostHandler& ph : m_post_handlers)
            {
                if(ph.url == pc.url)
                {
                    std::string result = ph.fn(connection,&pc);
                    snprintf(response_uri, response_uri_len, "%s", result.c_str());
                    break;
                }
            }

            pc.ps = REMOVE;
            pbuf_free(pc.buf);
            m_post_context.erase(m_post_context.begin() + i);
            return;
        }
    }
}



#endif // LWIP_HTTPD_SUPPORT_POST


// Should we support DNS server?
#if LWIP_MDNS_RESPONDER

// Adds a text record to the DNS
// The page is accessible from the "/" path
void Connector::srv_txt(struct mdns_service *service, void *txt_userdata)
{
    err_t res;
    LWIP_UNUSED_ARG(txt_userdata);

    res = mdns_resp_add_service_txtitem(service, "path=/", 6);
    LWIP_ERROR("mdns add service txt failed\n", (res == ERR_OK), return);
}


// Sets so that the page can be accessed through a host name (hostname)
void Connector::dns_init(char* hostname)
{
    // Setup mdns
    cyw43_arch_lwip_begin();
    mdns_resp_init();
    printf("mdns host name %s.local\n", hostname);
#if LWIP_VERSION_MAJOR >= 2 && LWIP_VERSION_MINOR >= 2
    mdns_resp_add_netif(&cyw43_state.netif[CYW43_ITF_STA], hostname);
    mdns_resp_add_service(&cyw43_state.netif[CYW43_ITF_STA], "pico_httpd", "_http", DNSSD_PROTO_TCP, 80, srv_txt, NULL);
#else
    mdns_resp_add_netif(&cyw43_state.netif[CYW43_ITF_STA], hostname, 60);
    mdns_resp_add_service(&cyw43_state.netif[CYW43_ITF_STA], "pico_httpd", "_http", DNSSD_PROTO_TCP, 80, 60, srv_txt, NULL);
#endif
    cyw43_arch_lwip_end();
}


#endif


// A small hack to make the C linker access the correct functions for the POST operations
// Because just having them static doesn't work with the linker 
#if LWIP_HTTPD_SUPPORT_POST
extern "C" {

err_t httpd_post_begin(void *conn, const char *uri, const char *http_request,
                       u16_t http_request_len, int content_len, char *response_uri,
                       u16_t response_uri_len, u8_t *post_auto_wnd)
{
    return Connector::httpd_post_begin(conn, uri, http_request, http_request_len,
                                       content_len, response_uri, response_uri_len, post_auto_wnd);
}

err_t httpd_post_receive_data(void *conn, struct pbuf *p)
{
    return Connector::httpd_post_receive_data(conn, p);
}

void httpd_post_finished(void *conn, char *response_uri, u16_t response_uri_len)
{
    Connector::httpd_post_finished(conn, response_uri, response_uri_len);
}

} // extern "C"
#endif


// Internal, private function to handle SSI (Dynamic functions), function for the C lib
// Checks if the name exists in saved states, if so put it into the buffer, so it can be changed on the page
// return the size of the inserted buffer
u16_t Connector::ssi_handler_function(int iIndex, char *pcInsert, int iInsertLen
#if LWIP_HTTPD_SSI_MULTIPART
, uint16_t current_tag_part, uint16_t *next_tag_part
#endif
)
{
    if (iIndex < 0 || iIndex >= (int)m_ssi_registry.size())
        return 0;

    std::string result = m_ssi_registry[iIndex].func(); C_DeskLink("Running func");

    // Copy safely into buffer
    strncpy(pcInsert, result.c_str(), iInsertLen);
    pcInsert[iInsertLen - 1] = '\0'; // ensure null termination
    size_t inserted = strnlen(pcInsert, iInsertLen);

    return inserted;
}