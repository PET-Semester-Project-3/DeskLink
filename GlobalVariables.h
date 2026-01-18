#ifndef GLOBALVARIABLES_H
#define GLOBALVARIABLES_H

#include <string>
#include <cstdint>

#ifndef WIFI_SSID
#define WIFI_SSID "realme C55"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "Woah1234"
#endif

#define TCP_SERVER_IP "10.251.103.163"

#if !defined(TCP_SERVER_IP)
#error TCP_SERVER_IP not defined
#endif

#define TCP_PORT 3000
#define DEBUG_printf printf



inline std::string last_id = "-1";
inline std::string last_response = "";
inline bool occupied = false; // initial state
inline int blinking_state = 0; // 0=normal, 1=blinking

inline const uint32_t MAGIC = 0xAEADBEEF;


#endif // GLOBALVARIABLES_H