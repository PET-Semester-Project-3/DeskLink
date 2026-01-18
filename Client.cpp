#include <string>
#include <cstdio>
#include <ctime>
#include <algorithm>

#include "WebConnection.h"
#include "GlobalVariables.h"
#include "FlashStorage.h"

#define BUTTON_PIN 10   // GPIO pin where the button is connected
#define LED_PIN 7       // Onboard LED for Pico


bool OccupationChanged = false;

// Handles Locking - occupied, function
// Sends {id:last_id, occupied:boolean}
void button_pressed(TCP_CLIENT_T *state) 
{
    // Set flag to send heatbeat immediately
    OccupationChanged = true;
    // Flip the bool
    occupied = !occupied;
    // Send {id:last_id, occupied:boolean}
    std::string body = "{\"id\": \"" + last_id + "\", \"occupied\": " + (occupied ? "true" : "false") + "}";
    printf("Sending occupied update: %s\n", body.c_str());

    // Get Response
    std::string resp = tcp_send_request_and_wait(state, "/api/pico-occupied", body, 5000);
    if (!resp.empty()) 
    {
        printf("Server response body: %s\n", resp.c_str());
    } else 
    {
        printf("No response / timeout when sending occupied.\n");
    }

    // Change the LED state
    gpio_put(LED_PIN, occupied ? 1 : 0);
}


// LED blinking task, but also general LED behaviour 
void led_blink_task(int& time_passed_ms, bool& led_state) 
{
    if (blinking_state == 1) 
    {
        if (time_passed_ms >= 500) // toggle every 500 ms
        {
            gpio_put(LED_PIN, led_state ? 1 : 0);
            time_passed_ms = 0;
            led_state = !led_state;
        }
    } 
    else 
    {
        // ensure LED reflects occupied state when not blinking
        gpio_put(LED_PIN, occupied ? 1 : 0);
        time_passed_ms = 0;
        led_state = false;
    }
}


// Setup button GPIO and main loop
// This function blocks indefinitely
// It uses the provided TCP_CLIENT_T state for sending requests
void setup_button(TCP_CLIENT_T *state) 
{
    // Configure button GPIO
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    // Configure LED GPIO
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);

    // reset variables
    bool last_button_state = false; 
    int heartbeatTimer = 0;
    const int HeatbeatAmount = 600; // 60 seconds
    int led_time_passed_ms = 0;
    bool led_state_blink = false;

    DEBUG_printf("Starting button function...\n");
    // main loop
    while (true) 
    {
        // check button state
        bool current_state = gpio_get(BUTTON_PIN);
        if (last_button_state && !current_state) // falling edge
        { 
            DEBUG_printf("Button pressed detected.\n");
            button_pressed(state);
        }
        last_button_state = current_state;


        // heartbeat handling
        heartbeatTimer += 1;
        if (heartbeatTimer % 120 == 0)
        {
            DEBUG_printf("Heartbeat timer: %d / %d\n", heartbeatTimer, HeatbeatAmount);

        }
        if (heartbeatTimer >= HeatbeatAmount || OccupationChanged) 
        {
            OccupationChanged = false;
            // send heartbeat
            std::string body = "{\"id\": \"" + last_id + "\"}";            
            printf("Sending heartbeat: %s\n", body.c_str());

            std::string resp = tcp_send_request_and_wait(state, "/api/pico-heartbeat", body, 5000);
            if (!resp.empty()) 
            {
                printf("Server heartbeat response body: %s\n", resp.c_str());
                
                // looks in a json for {"state":integer}
                int server_state = -1;
                if (sscanf(resp.c_str(), "{\"state\":%d}", &server_state) == 1) 
                {
                    if (server_state == 1 && blinking_state == 0) 
                    {
                        // start blinking
                        blinking_state = 1;
                        printf("Starting LED blinking.\n");
                    } 
                    else if (server_state == 0 && blinking_state == 1) 
                    {
                        // stop blinking
                        blinking_state = 0;
                        gpio_put(LED_PIN, occupied ? 1 : 0);
                        printf("Stopping LED blinking.\n");
                    }
                } else 
                {
                    printf("Couldn't parse state from heartbeat response.\n");
                }

            } else 
            {
                printf("No response / timeout when sending heartbeat.\n");
            }

            heartbeatTimer = 0;
        }

    
        // LED blinking handling
        led_time_passed_ms += 100;
        led_blink_task(led_time_passed_ms, led_state_blink);

        sleep_ms(100); // debounce
    }
}

// Handshake with server to get an ID
TCP_CLIENT_T* run_tcp_client_test(void) 
{
    TCP_CLIENT_T *state = tcp_client_init();
    if (!state) return nullptr;

    if (!tcp_client_open(state)) 
    {
        free(state);
        return nullptr;
    }

    // get an id from server
    // send {"last_id": <last_id>}
    std::string req = "{\"last_id\": \"" + last_id + "\"}";
    std::string resp = tcp_send_request_and_wait(state, "/api/pico-connect", req, 3000);
    if (!resp.empty()) 
    {

        char id_buffer[ID_MAX_LEN + 1] = {0};
        if (sscanf(resp.c_str(), "{\"success\":true,\"id\":\"%36[^\"]\"}", id_buffer) == 1) 
        {
            last_id = std::string(id_buffer);
            printf("Got new id: %s\n", last_id.c_str());
        } else 
        {
            printf("Couldn't parse id from: %s\n", resp.c_str());
        }
    } else 
    {
        printf("No response for /api/pico-connect\n");
    }

    printf("TCP client handshake complete.\n");
    // leave connection open for button usage
    // do not close here; setup_button will reuse the same state
    // free(state) will be done on program exit
    return state;
}

int main() 
{
    stdio_init_all();

    if (cyw43_arch_init()) 
    {
        DEBUG_printf("failed to initialise\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();

    // Connect to Wi-Fi (retry till works)
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 10000)) 
    {
        printf("failed to connect, retrying...\n");
    }
    printf("Connected to Wi-Fi.\n");

    // Load last_id from flash
    std::string loaded = load_last_id();
    printf("Loaded last_id = %s from flash\n", loaded.c_str());
    last_id = loaded;

    TCP_CLIENT_T *state = run_tcp_client_test();

    // Save last_id to flash
    // After handshake, if server gives a new ID:
    if (last_id != loaded)
    {
        save_last_id(last_id);
    }
    DEBUG_printf("Starting button loop...\n");

    // Init TCP client for interactive use
    //TCP_CLIENT_T *state = tcp_client_init();
    if (!state || !tcp_client_open(state)) 
    {
        printf("Failed to init TCP client\n");
        return 1;
    }

        DEBUG_printf("Starting button function...\n");

    // Start button loop (blocking)
    setup_button(state);

    // never reached in normal flow, but keep cleanup for completeness
    tcp_client_close(state);
    free(state);
    cyw43_arch_deinit();
    return 0;
}
