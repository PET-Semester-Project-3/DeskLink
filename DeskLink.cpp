#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "dbop.h"
#include "DeskLink.h"
#include "Connection.h"

int main()
{
    stdio_init_all();
    if (cyw43_arch_init()) 
    {
        printf("failed to initialise\n");
        return 1;
    }
    cyw43_arch_enable_sta_mode();
    

     // Add a simple SSI handler
    Connector::AddSSIHandler({"status", // tag
        []() -> std::string 
        {
            C_DeskLink("status");
            return "pass";
        }
    });

    // Add a simple SSI handler
    Connector::AddSSIHandler({"welcome", // tag
        []() -> std::string 
        {
            C_DeskLink("welcome");
            return "Hello from Pico";
        }
    });

    // Add a simple SSI handler
    Connector::AddSSIHandler({"uptime", // tag
        []() -> std::string 
        {
            C_DeskLink("uptime");
            uint64_t uptime_s = absolute_time_diff_us(Connector::m_wifi_connected_time, get_absolute_time()) / 1e6;
            C_DeskLink("uptime_s: " + std::to_string(uptime_s));
            return std::to_string(uptime_s);
        }
    });

    // Add a simple SSI handler
    Connector::AddSSIHandler({"ledstate", // tag
        []() -> std::string 
        {
            C_DeskLink("LED State");
            return "OFF";
        }
    });

    // Add a simple SSI handler
    Connector::AddSSIHandler({"ledinv", // tag
        []() -> std::string 
        {
            C_DeskLink("LED Inv");
            return "OFF";
        }
    });

    // Add a simple SSI handler
    Connector::AddSSIHandler({"table", // tag
        []() -> std::string 
        {
            //
            return "<tr><td>This is table row</td></tr>";
        }
    });


    // Add a simple CGI handler
    tCGI cgi;
    cgi.pcCGIName = "/";
    cgi.pfnCGIHandler = [](int iIndex, int iNumParams, char *pcParam[], char *pcValue[]) -> const char* 
    {
        C_DeskLink("/ CGI");

        if (iNumParams > 0) 
        {
            if (strcmp(pcParam[0], "test") == 0)
            {
                C_DeskLink("returning test");

                return "/test.shtml";
            }
            
        }
        C_DeskLink("returning index");

        return "/index.shtml";
    };
   Connector::AddCGIHandler(cgi);


    // Add a simple CGI handler
    tCGI cgi1;
    cgi1.pcCGIName = "/index.shtml";
    cgi1.pfnCGIHandler = [](int iIndex, int iNumParams, char *pcParam[], char *pcValue[]) -> const char* 
    {
        C_DeskLink("Index CGI");

        if (iNumParams > 0) 
        {
            if (strcmp(pcParam[0], "test") == 0)
            {
                C_DeskLink("returning test");

                return "/test.shtml";
            }
            
        }
        C_DeskLink("returning index");

        return "/index.shtml";
    };
   Connector::AddCGIHandler(cgi1);


#if LWIP_HTTPD_SUPPORT_POST
    // Add a simple POST handler
    PostHandler ph;
    ph.url = "/led.cgi";
    ph.placeholder_page = "/ledfail.shtml";
    ph.fn = [](void* conn, PostContext* pc) -> std::string 
    {
        C_DeskLink("Post handler");
        char buf[4];
        char *val = Connector::httpd_param_value(pc->buf, "led_state=", buf, sizeof(buf));
        if (val) 
        {
            C_DeskLink("flip flopped");
        }

        return "/ledpass.shtml";
    };
    Connector::AddPostHandler(ph);
#endif

    Connector::Init();

    while(true)
    {
        sleep_ms(10);
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
