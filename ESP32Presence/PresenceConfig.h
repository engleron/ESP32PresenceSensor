#pragma once

// Firmware metadata
#define FIRMWARE_VERSION "2.2.0"

// Compile-time feature flags
// Uncomment to enable native HomeKit support (requires arduino-homekit-esp32 library)
// #define ENABLE_HOMEKIT

// Hardware/IO defaults
#define NUM_PIXELS        1
#define DNS_PORT          53
#define BAUD_RATE_DEBUG   115200
#define BAUD_RATE_SENSOR  256000
#define PIN_RESET         0

// Integration defaults
#define DEFAULT_INSTEON_HUB_PORT  "25105"
#define DEFAULT_HA_PORT           "8123"

// Timing defaults
#define HEARTBEAT_INTERVAL      1000
#define SENSOR_CHECK_INTERVAL   500
#define RESET_HOLD_TIME         5000
#define SESSION_TIMEOUT_MS      (15UL * 60UL * 1000UL)
#define LOGIN_LOCKOUT_MS        (60UL * 1000UL)
#define MAX_LOGIN_ATTEMPTS      5
#define STATUS_UPDATE_INTERVAL  2000
#define WDT_TIMEOUT_SECONDS     8

// Board-specific default pin assignments
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  #define BOARD_TYPE       "ESP32-S3"
  #define SENSOR_OUT_PIN   4
  #define SENSOR_TX_PIN    17
  #define SENSOR_RX_PIN    16
  #define RGB_LED_PIN      48
  #warning "Compiling for ESP32-S3"
#elif defined(CONFIG_IDF_TARGET_ESP32)
  #define BOARD_TYPE       "ESP32-DevKitC-32E"
  #define SENSOR_OUT_PIN   4
  #define SENSOR_TX_PIN    18
  #define SENSOR_RX_PIN    17
  #define RGB_LED_PIN      16
  #warning "Compiling for ESP32"
#else
  #error "Unsupported board! Please select ESP32 or ESP32-S3 in Arduino IDE (Tools -> Board)"
#endif
