#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "dbop.h"
#include "DeskLink.h"


int main()
{
    stdio_init_all();







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
