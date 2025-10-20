#ifndef LED_H
#define LED_H

#include "pico/stdlib.h"
#include "dbop.h"   // Provides COMMENT() and DEBUG_INFO macros

class Led {
private:
    uint pin;       // GPIO pin for the LED
    bool state;     // Current LED state
public:
    Led(uint pinNumber);       // Constructor – initializes the LED GPIO
    void on();                 // Turn LED ON
    void off();                // Turn LED OFF
    void toggle();             // Toggle LED state
    void setState(bool s);     // Set LED state directly
    bool isOn() const;         // Return current LED state
};

#endif // LED_H

