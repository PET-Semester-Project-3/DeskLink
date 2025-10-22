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
    Connector::AddSSIHandler({
        "uptime", // tag
        []() -> std::string 
        {
            printf("SSI");
            // Return uptime in seconds as string
            return std::to_string(time_us_64() / 1000000);
        }
    });

    // Add a simple CGI handler
    tCGI cgi;
    cgi.pcCGIName = "/hello.cgi";
    cgi.pfnCGIHandler = [](int iIndex, int iNumParams, char *pcParam[], char *pcValue[]) -> const char* 
    {
        printf("index shtml");
        return "/index.shtml";
    };
   Connector::AddCGIHandler(cgi);


#if LWIP_HTTPD_SUPPORT_POST
    // Add a simple POST handler
    PostHandler ph;
    ph.url = "/led.cgi";
    ph.placeholder_page = "/404.shtml";
    ph.fn = [](void* conn, PostContext* pc) -> std::string 
    {
        // Just print the first param if exists
        char buf[32];
        char* val = Connector::httpd_param_value(pc->buf, "led_state=",buf,sizeof(buf));
        if(val) 
        {
            printf("POST led_state=%s\n", val);
        }
        return "/index.shtml";
    };
    Connector::AddPostHandler(ph);
#endif

    printf("AAAA");
    printf("dg");
    Connector::Init();

    printf("BBB");
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
