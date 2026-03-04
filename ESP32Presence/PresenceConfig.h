#pragma once

// Firmware metadata
#define FIRMWARE_VERSION "2.3.0"

// Compile-time feature flags
// Uncomment to enable native HomeKit support (requires arduino-homekit-esp32 library)
// #define ENABLE_HOMEKIT

// Runtime web UI + OTA services in normal operation.
// 0 = disabled by default for lowest control-loop latency
// 1 = enabled in normal operation
#define ENABLE_RUNMODE_WEB_OTA_DEFAULT 0

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
#define MAIN_LOOP_DELAY_MS      30
#define RESET_HOLD_TIME         5000
#define SESSION_TIMEOUT_MS      (15UL * 60UL * 1000UL)
#define LOGIN_LOCKOUT_MS        (60UL * 1000UL)
#define MAX_LOGIN_ATTEMPTS      5
#define STATUS_UPDATE_INTERVAL  2000
#define WDT_TIMEOUT_SECONDS     8

// Integration command timing (reduce blocking latency in control loop)
#define LIGHT_COMMAND_RETRY_INTERVAL_MS   1200
// Sensor entity: re-publish state to HA even without change (keeps entity alive)
#define HA_SENSOR_REPUBLISH_MS            60000
#define INSTEON_HTTP_CONNECT_TIMEOUT_MS   1200
#define INSTEON_HTTP_TIMEOUT_MS           1200
#define INSTEON_INTER_REQUEST_DELAY_MS    350
#define INSTEON_RETRY_DELAY_MS            150

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
