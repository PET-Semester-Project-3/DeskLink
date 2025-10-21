#ifndef CONNECTION_H
#define CONNECTION_H


#include "pico/stdlib.h"

#include "pico/cyw43_driver.h"

#include "lwipopts.h"

#include "pico/cyw43_arch.h"

#include "lwip/init.h"
#include "lwip/netif.h"    
#include "lwip/ip4_addr.h"
#include "lwip/apps/mdns.h"
#include "lwip/apps/httpd.h"


#include <vector>
#include <string>
#include <functional>

#ifndef CYW43_HOST_NAME
#define CYW43_HOST_NAME "PicoW"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID "realme C55"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "Woah1234"
#endif

struct SSIHandler
{
  std::string tag;  
  std::function<std::string()> func;
};

enum POST_STATUS
{
  BEGIN,
  RECEIVE,
  END,
  REMOVE
};

struct PostContext
{
  void* connection;
  std::string url;  
  POST_STATUS ps;

  struct pbuf* buf;
};

struct PostHandler 
{
    std::string url;
    std::string placeholder_page = "";                             
    std::function<std::string(void* connection, PostContext* pc)> fn ;    
};







class Connector
{
public:

  /// Setup the handlers before it!!
  static int Init()
  {
    printf("11");
    char hostname[sizeof(CYW43_HOST_NAME) + 4];
    memcpy(&hostname[0], CYW43_HOST_NAME, sizeof(CYW43_HOST_NAME) - 1);
    get_mac_ascii(CYW43_HAL_MAC_WLAN0, 8, 4, &hostname[sizeof(CYW43_HOST_NAME) - 1]);
    hostname[sizeof(hostname) - 1] = '\0';

    printf("22");


    netif_set_hostname(&cyw43_state.netif[CYW43_ITF_STA], hostname);

    printf("33");


    printf("Connecting to WiFi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) 
    {
        printf("failed to connect.\n");
        exit(1);
    } else 
    {
        printf("Connected.\n");
    }
    printf("\nReady, running httpd at %s\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));

    // start http server
    m_wifi_connected_time = get_absolute_time();

    #if LWIP_MDNS_RESPONDER
    dns_init(hostname);
    #endif

    printf("44\n");

    cyw43_arch_lwip_begin();
      printf("55\n");

    httpd_init();
        printf("66\n");

    http_set_cgi_handlers(m_cgi_handlers.data(), m_cgi_handlers.size());
        printf("77\n");

    http_set_ssi_handler(ssi_handler_function, m_tag_names.data(), m_tag_names.size());
        printf("88\n");
    sleep_ms(2000);
    //cyw43_arch_lwip_end();
        printf("99\n");

  }

  static bool AddCGIHandler(tCGI& handler)
  {
    m_cgi_handlers.reserve(m_cgi_handlers.capacity() + 1); // In order to prevent dynamic vector size increase, which is 2n, doubling the capacity if not enough
    m_cgi_handlers.push_back(handler);
    return true;
  }

  static const std::vector<tCGI>& GetCGIHandlers(void)
  {
    return m_cgi_handlers;
  }

  /// Get MAC as ASCI
  static size_t get_mac_ascii(int idx, size_t chr_off, size_t chr_len, char *dest_in)
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

  /// Return a value for a parameter
  static char *httpd_param_value(struct pbuf *p, const char *param_name, char *value_buf, size_t value_buf_len) 
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


  // Be aware of LWIP_HTTPD_MAX_TAG_NAME_LEN
  static bool AddSSIHandler(const SSIHandler& handler) 
  {
    if(m_ssi_registry.size() >= LWIP_HTTPD_MAX_TAG_NAME_LEN)
      return false;

    for (auto& h : m_ssi_registry)
        if (h.tag == handler.tag) return false;
          

    m_tag_names.reserve(m_tag_names.capacity()+1);
    m_tag_names.push_back(handler.tag.c_str());

    m_ssi_registry.reserve(m_ssi_registry.capacity() + 1); // In order to prevent dynamic vector size increase, which is 2n, doubling the capacity if not enough
    m_ssi_registry.push_back(handler);
    return true;
  }

  static const std::vector<SSIHandler>& GetSSIHandlers(void)
  {
    return m_ssi_registry;
  }

#if LWIP_HTTPD_SUPPORT_POST
  static bool AddPostHandler(PostHandler& handler)
  {
    m_post_handlers.reserve(m_post_handlers.capacity() + 1); // In order to prevent dynamic vector size increase, which is 2n, doubling the capacity if not enough
    m_post_handlers.push_back(handler);
    return true;
  }

  static const std::vector<PostHandler>& GetPostHandler(void)
  {
    return m_post_handlers;
  }
#endif

#if LWIP_HTTPD_SUPPORT_POST
  // Add size limit to the amount of contexts 
  static err_t httpd_post_begin(void *connection, const char *uri, const char *http_request,u16_t http_request_len, int content_len, char *response_uri,u16_t response_uri_len, u8_t *post_auto_wnd)
  {
    for (auto& h : m_post_handlers) 
    {
      if (strcmp(uri, h.url.c_str()) == 0) 
      {
        PostContext pc{connection, uri, BEGIN, nullptr};

        m_post_context.push_back(pc);

        snprintf(response_uri, response_uri_len, "%s", h.placeholder_page.c_str());

        *post_auto_wnd = 1;
        return ERR_OK;
      }
    }

    return ERR_VAL; // no handler found
          
  }

  static err_t httpd_post_receive_data(void *connection, struct pbuf *p) 
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
      // pbuf_free(p); Cannot free it here, as the lambda to determine what to do runs later on
    }
    
    return ERR_VAL;
  }

  static void httpd_post_finished(void *connection, char *response_uri, u16_t response_uri_len) 
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

  static std::vector<PostHandler> m_post_handlers;
  static std::vector<PostContext> m_post_context;
  

#endif


  static void cleanup()
  {

#if LWIP_MDNS_RESPONDER
    mdns_resp_remove_netif(&cyw43_state.netif[CYW43_ITF_STA]);
#endif
    cyw43_arch_deinit();
  }

private:
/// DNS Black Magic
#if LWIP_MDNS_RESPONDER
  static void srv_txt(struct mdns_service *service, void *txt_userdata)
  {
    err_t res;
    LWIP_UNUSED_ARG(txt_userdata);

    res = mdns_resp_add_service_txtitem(service, "path=/", 6);
    LWIP_ERROR("mdns add service txt failed\n", (res == ERR_OK), return);
  }

  static void dns_init(char* hostname)
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


  static u16_t ssi_handler_function(int iIndex, char *pcInsert, int iInsertLen
#if LWIP_HTTPD_SSI_MULTIPART
    , uint16_t current_tag_part, uint16_t *next_tag_part
#endif
)
{

      if (iIndex < 0 || iIndex >= (int)m_ssi_registry.size())
          return 0;

      std::string result = m_ssi_registry[iIndex].func();

      // Copy safely into buffer
      strncpy(pcInsert, result.c_str(), iInsertLen);
      pcInsert[iInsertLen - 1] = '\0'; // ensure null termination

      return result.size();
  }




  static std::vector<tCGI> m_cgi_handlers;
  static std::vector<SSIHandler> m_ssi_registry;
  static std::vector<const char*> m_tag_names;

  static absolute_time_t m_wifi_connected_time;

};




#endif // CONNECTION_H

