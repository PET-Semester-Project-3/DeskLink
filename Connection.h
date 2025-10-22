#ifndef CONNECTION_H
#define CONNECTION_H

// DO NOT CHANGE THE INCLUDE ORDER
// Otherwise the program may fail 
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

// Could be moved into the CMake, describes the WiFi, should be changed for each user
#ifndef WIFI_SSID
#define WIFI_SSID "realme C55"
#endif

// Could be moved into the CMake, describes the WiFi, should be changed for each user
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "Woah1234"
#endif


/**
 * @brief SSI handler is used for the dynamic variables in the shtml file.
 *
 * Putting <!--#tag--> inside the shtml and adding a handler will change each occurrence of it into the result of function "func".
 */
struct SSIHandler
{
  std::string tag;                      // Tag in the shtml <!--#tag-->
  std::function<std::string()> func;    // Function that will generate the result that should be placed
};


/**
 * @brief State machine for the POST Context, it checks where in the POST process is a connection
 * 
 * BEGIN - Connection ensured 
 * 
 * RECEIVE - Data buffer is received
 * 
 * END - User defined function in Post Handler is run
 * 
 * REMOVE - Data is being cleaned
 */
enum POST_STATUS
{
  BEGIN,
  RECEIVE,
  END,
  REMOVE
};


/**
 * @brief Context for the function handling POST operation
 *
 * url contains the url that was used to POST
 * 
 * buf contains a pbuf which holds the data from the POST, usually should be used with Connector::httpd_param_value()
 * 
 * ps and connection are mostly internal 
 */
struct PostContext
{
  void* connection;       // Connection, type depends on the system and doesn't matter, used for comparisons internally
  std::string url;        // URL that was used to POST
  POST_STATUS ps;         // Status of the POST process, inside the function it will always be END, used internally

  struct pbuf* buf;       // Buffer for the POST data, from RECEIVE stage, usually should be used with Connector::httpd_param_value() to get a parameter
};


/**
 * @brief Handler object, defines what should happen in case POST for a given URL was triggered
 *
 * url is the URL where the POST was triggered
 * 
 * placeholder_page is a page that opens during the POST process, between BEGIN and END and in case of failure. NULL for no change
 * 
 * fn is the function that is ran to handle the POST, it should return the page shtml file for success, NULL for no change.
 */
struct PostHandler 
{
    std::string url;
    std::string placeholder_page = "";                             
    std::function<std::string(void* connection, PostContext* pc)> fn ;    
};

/**
 * @brief Class used for handling and setup of the basic HTTP Server
 * 
 * Currently for the details about the functions and variables head to Connection.cpp
 */
class Connector
{
public:
  static absolute_time_t m_wifi_connected_time;

  static int Init(void);

  static bool AddCGIHandler(tCGI& handler);

  static const std::vector<tCGI>& GetCGIHandlers(void);

  static size_t get_mac_ascii(int idx, size_t chr_off, size_t chr_len, char *dest_in);

  static char *httpd_param_value(struct pbuf *p, const char *param_name, char *value_buf, size_t value_buf_len);

  static bool AddSSIHandler(const SSIHandler& handler);

  static const std::vector<SSIHandler>& GetSSIHandlers(void);

  static void cleanup(void);


#if LWIP_HTTPD_SUPPORT_POST
  static bool AddPostHandler(PostHandler& handler);

  static const std::vector<PostHandler>& GetPostHandler(void);

  static err_t httpd_post_begin(void *connection, 
                                const char *uri, 
                                const char *http_request,
                                u16_t http_request_len, 
                                int content_len, 
                                char *response_uri,
                                u16_t response_uri_len, 
                                u8_t *post_auto_wnd);

  static err_t httpd_post_receive_data(void *connection, struct pbuf *p);

  static void httpd_post_finished(void *connection, char *response_uri, u16_t response_uri_len);

  static std::vector<PostHandler> m_post_handlers;
  static std::vector<PostContext> m_post_context;
#endif

private:

#if LWIP_MDNS_RESPONDER
  static void srv_txt(struct mdns_service *service, void *txt_userdata);

  static void dns_init(char* hostname);
#endif


static u16_t ssi_handler_function(int iIndex, char *pcInsert, int iInsertLen
#if LWIP_HTTPD_SSI_MULTIPART
  , uint16_t current_tag_part, uint16_t *next_tag_part
#endif
);

  static std::vector<tCGI> m_cgi_handlers;
  static std::vector<SSIHandler> m_ssi_registry;
  static std::vector<std::string> m_tag_name_storage;
  static std::vector<const char*> m_tag_names;

};

#endif // CONNECTION_H

