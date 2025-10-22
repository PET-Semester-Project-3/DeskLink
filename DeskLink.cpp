#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "dbop.h"
#include "DeskLink.h"
#include "Connection.h"


static bool LED_state = false;

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

    // Add an SSI for status - pass
    Connector::AddSSIHandler({"status", 
        []() -> std::string 
        {
            C_DeskLink("status");
            return "pass";
        }
    });

    // Add an SSI for welcome - Hello from Pico
    Connector::AddSSIHandler({"welcome", 
        []() -> std::string 
        {
            C_DeskLink("welcome");
            return "Hello from Pico";
        }
    });

    // Add an SSI for uptime - time since connected
    Connector::AddSSIHandler({"uptime",
        []() -> std::string 
        {
            C_DeskLink("uptime");
            uint64_t uptime_s = absolute_time_diff_us(Connector::m_wifi_connected_time, get_absolute_time()) / 1e6;
            C_DeskLink("uptime_s: " + std::to_string(uptime_s));
            return std::to_string(uptime_s);
        }
    });

    // Add an SSI for the led state - ON : OFF
    Connector::AddSSIHandler({"ledstate",
        []() -> std::string 
        {
            C_DeskLink("LED State");
            return LED_state ? "ON" : "OFF";
        }
    });

    // Add an SSI for the inverse of the led state - ON : OFF
    Connector::AddSSIHandler({"ledinv",
        []() -> std::string 
        {
            C_DeskLink("LED Inv");
            return !LED_state ? "ON" : "OFF";
        }
    });

    // Add an SSI for table 
    Connector::AddSSIHandler({"table", // tag
        []() -> std::string 
        {
            C_DeskLink("table");
            return "<tr><td>This is table row</td></tr>";
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


    // Add a CGI for the index.shtml
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

    // Current CGIs use the same function, could be different, could be made into a global one

    // ===================================================================

    // Setup for the POST responses

#if LWIP_HTTPD_SUPPORT_POST
    // Add a simple POST handler
    PostHandler ph;
    ph.url = "/led.cgi";                            // URL for the post
    ph.placeholder_page = "/ledfail.shtml";         // URL for waiting and incorrect
    ph.fn = [](void* conn, PostContext* pc) -> std::string // Function to handle the URL, change the LED state
    {
        C_DeskLink("Post handler");
        char buf[4];
        char *val = Connector::httpd_param_value(pc->buf, "led_state=", buf, sizeof(buf));
        if (val)                                    // Response exists
        {
            C_DeskLink("flip flopped");
            LED_state = !LED_state; C_DeskLink("State: " + LED_state ? "ON" : "OFF");
            cyw43_gpio_set(&cyw43_state, 0, LED_state);     // Change the LED state
        }

        return "/ledpass.shtml";                    // URL on success
    };
    Connector::AddPostHandler(ph);
#endif



    Connector::Init();                              // INIT of the Connector, start of the webserver

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
