#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "dbop.h"
#include "DeskLink.h"
#include "Connection.h"


static bool Occupied = false;

int main()
{

    stdio_init_all();
    if (cyw43_arch_init()) 
    {
        printf("failed to initialise\n");
        return 1;
    }
    cyw43_arch_enable_sta_mode();
    

    // SSI - Dynamic variable in the SHTML <!--#tag--> format

    // Add an SSI for uptime - time since connected
    HTTPServer::AddSSIHandler({"uptime",
        []() -> std::string 
        {
            C_DeskLink("uptime");
            uint64_t uptime_s = absolute_time_diff_us(HTTPServer::m_wifi_connected_time, get_absolute_time()) / 1e6;
            return std::to_string(uptime_s);
        }
    });

    // Add an SSI for the occupied state Occupied : Unused
    HTTPServer::AddSSIHandler({"occupied",
        []() -> std::string 
        {
            C_DeskLink("Get occupied state");
            return Occupied ? "Occupied" : "Unused";
        }
    });

    // Add an SSI for the inverse of the occupied state - Occupied : Unused
    HTTPServer::AddSSIHandler({"INV",
        []() -> std::string 
        {
            C_DeskLink("Get inverse of an occupied state");
            return !Occupied ? "Occupied" : "Unused";
        }
    });



    // ===================================================================
    
    // CGIs are possible Routes

    // Add a CGI for base url "/"
    tCGI cgi;
    cgi.pcCGIName = "/"; //url
    cgi.pfnCGIHandler = [](int iIndex, int iNumParams, char *pcParam[], char *pcValue[]) -> const char* 
    {
        C_DeskLink("/ CGI");

        return "/index.shtml";
    };
    HTTPServer::AddCGIHandler(cgi);


    // Add a CGI for the index.shtml
    tCGI cgi1;
    cgi1.pcCGIName = "/index.shtml";
    cgi1.pfnCGIHandler = [](int iIndex, int iNumParams, char *pcParam[], char *pcValue[]) -> const char* 
    {
        C_DeskLink("Index CGI");

        return "/index.shtml";
    };
    HTTPServer::AddCGIHandler(cgi1);

    // Current CGIs use the same function, could be different, could be made into a global one

    // ===================================================================

    // Setup for the POST responses

#if LWIP_HTTPD_SUPPORT_POST
    // Add a simple POST handler
    PostHandler ph;
    ph.url = "/Login.cgi";                              // URL for the post
    ph.placeholder_page = "";                         // URL for waiting and incorrect
    ph.fn = [](void* conn, PostContext* pc) -> std::string // Function to handle the URL, change the LED state
    {
        C_DeskLink("Post handler");
        char buf[12];
        char *val = HTTPServer::httpd_param_value(pc->buf, "desk_state=", buf, sizeof(buf));
        C_DeskLink(val);
        if (val)                                    // Response exists
        {
            C_DeskLink("flip flopped");
            Occupied = !Occupied;                           C_DeskLink(Occupied ? "Occupied" : "Unused");
            cyw43_gpio_set(&cyw43_state, 0, Occupied);     // Change the LED state
        }else
        {
            C_DeskLink("No value");
        }

        return "/";                                  // URL on success
    };
    HTTPServer::AddPostHandler(ph);
#endif



    HTTPServer::Init();                              // INIT of the HTTPServer, start of the webserver

    while(true)
    {
        sleep_ms(10);                               // Could also be tight loop, nothing here as of now, as everything is just interupt
    }

    /*
    Led RedLED(7);                                                              C_DeskLink("Create Red LED object on GPIO7");
    Button button1(10, GPIO_IRQ_EDGE_RISE);                                     C_DeskLink("Create Button object on GPIO10");

                                                                                C_DeskLink("Print startup message");

    while (true) {
        if (button1.hasEvent()) {                                               C_DeskLink("Check for new button event");
            RedLED.setState(button1.toggleState());                             C_DeskLink("Set LED state based on button toggle state");
            printf("New event: Button pressed %d times, LED state: %s\n",
                   button1.getPressCount(),
                   RedLED.isOn() ? "ON" : "OFF");                               C_DeskLink("Print current counts and state");
        }

        sleep_ms(10);
    }
        */
}
