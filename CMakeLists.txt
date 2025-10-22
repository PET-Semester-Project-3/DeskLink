# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(DeskLink C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1
add_executable(DeskLink DeskLink.cpp models/Led.cpp models/PushButton.cpp Connection.cpp)

pico_set_program_name(DeskLink "DeskLink")
pico_set_program_version(DeskLink "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(DeskLink 1)
pico_enable_stdio_usb(DeskLink 0)

# Add the standard library to the build
target_link_libraries(DeskLink
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(DeskLink PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts
        ${PICO_LWIP_CONTRIB_PATH}/apps/httpd

)

# Add any user requested libraries
target_link_libraries(DeskLink 
        pico_httpd_content
        pico_lwip_http
        pico_lwip_mdns
        pico_cyw43_arch_lwip_threadsafe_background
        )

pico_add_extra_outputs(DeskLink)

pico_add_library(pico_httpd_content NOFLAG)

pico_set_lwip_httpd_content(pico_httpd_content INTERFACE
         ${CMAKE_CURRENT_LIST_DIR}/content/404.html
        ${CMAKE_CURRENT_LIST_DIR}/content/index.shtml
        ${CMAKE_CURRENT_LIST_DIR}/content/test.shtml
        ${CMAKE_CURRENT_LIST_DIR}/content/ledpass.shtml
        ${CMAKE_CURRENT_LIST_DIR}/content/ledfail.shtml
        ${CMAKE_CURRENT_LIST_DIR}/content/img/rpi.png.gz
        )