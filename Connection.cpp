#include "Connection.h"

// Static member definitions
std::vector<tCGI> Connector::m_cgi_handlers;
std::vector<SSIHandler> Connector::m_ssi_registry;
std::vector<const char*> Connector::m_tag_names;
std::vector<std::string> Connector::m_tag_name_storage;
std::vector<PostHandler> Connector::m_post_handlers;
std::vector<PostContext> Connector::m_post_context;
absolute_time_t Connector::m_wifi_connected_time;


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