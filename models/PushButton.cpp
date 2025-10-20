
#include <stdio.h>
#include "PushButton.h"

Button* Button::instances[MAX_GPIO] = {nullptr};
Button::Button(uint pinNumber, uint32_t edgeType)
    : pin(pinNumber), edge(edgeType),
      pressCount(0), toggleStateValue(false),
      debounceAlarm(0), lastState(false),
      eventFlag(false)
{
    gpio_init(pin);                                             C_PushButton("Initialize Button GPIO");
    gpio_set_dir(pin, GPIO_IN);                                 C_PushButton("Configure Button as input");
    gpio_pull_down(pin);                                        C_PushButton("Button configured with pull-down (active-high)");
    instances[pin] = this;                                      C_PushButton("Store this instance in static array for ISR lookup");
    gpio_set_irq_enabled_with_callback(pin, edge, true, &Button::gpio_isr); 
                                                                C_PushButton("Interrupt enabled on Button pin: selected edge registered");
}
void Button::gpio_isr(uint gpio, uint32_t events) {
    if (instances[gpio]) {
        instances[gpio]->startDebounce();                       C_PushButton("Dispatch to correct Button instance startDebounce()");
    }
}
void Button::startDebounce() {
    lastState = gpio_get(pin);                                  C_PushButton("Read current button state for debounce");
    cancel_alarm(debounceAlarm);                                C_PushButton("Cancel any previous debounce alarm");
    debounceAlarm = add_alarm_in_ms(DEBOUNCE_MS, &Button::debounceTimerCallback, (void*)this, false); 
                                                                C_PushButton("Start debounce timer");
}
int64_t Button::debounceTimerCallback(alarm_id_t id, void* user_data) {
    Button* self = reinterpret_cast<Button*>(user_data);        C_PushButton("Static timer callback calls handleDebounce() on instance");
    return self->handleDebounce();
}
int64_t Button::handleDebounce() {
    bool currentState = gpio_get(pin);                          C_PushButton("Read button state at debounce timeout");
    if (currentState == lastState && currentState == true) {
        pressCount++;                                           C_PushButton("Increment press counter");
        toggleStateValue = !toggleStateValue;                   C_PushButton("Toggle internal state");
        eventFlag = true;                                       C_PushButton("Set event flag for hasEvent()");
                                                                C_PushButton("Debounce passed: valid button press");
    } else {
                                                                C_PushButton("Debounce failed: bounce ignored");
    }
    return 0;
}
int Button::getPressCount() const {
    return pressCount;                                          C_PushButton("Return press count");
}
bool Button::isPressed() const {
    return gpio_get(pin);                                       C_PushButton("Return current button level");
}
bool Button::toggleState() const {
    return toggleStateValue;                                    C_PushButton("Return toggle state");
}
bool Button::hasEvent() {
    if (eventFlag) {
        eventFlag = false;                                      C_PushButton("Clear event flag after reading");
        return true;
    }
    return false;
}