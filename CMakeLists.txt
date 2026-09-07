# OTAA Library - ESP-IDF Component
#
# This file allows OTAA-Arduino to be used as an ESP-IDF component
# via idf_component_register() or as a PlatformIO library.
#
# Usage with ESP-IDF:
#   1. Copy/symlink this directory to your project's components/ folder
#   2. Or use idf_component.yml with ESP Component Registry

cmake_minimum_required(VERSION 3.16)

if(IDF_TARGET)
    # ESP-IDF component mode
    idf_component_register(
        SRCS "src/OTAA.cpp"
        INCLUDE_DIRS "src"
        REQUIRES
            esp_http_client
            esp_wifi
            esp_netif
            esp_system
            app_update
            esp_timer
            mbedtls
            json
            ArduinoJson
    )
else()
    # PlatformIO / standalone mode — no CMake action needed
    message(STATUS "OTAA-Arduino: PlatformIO mode (CMakeLists.txt not used)")
endif()
