/*
 * PresenceHomeKit.c — HomeKit accessory definition (C linkage required).
 *
 * This file MUST be compiled as C (not C++) because the HOMEKIT_ACCESSORY,
 * HOMEKIT_SERVICE, and HOMEKIT_CHARACTERISTIC macros create compound literals
 * that need C's static-storage-duration guarantee at file scope.  If they
 * were placed in a .cpp file the compound-literal arrays would be temporaries
 * and could become dangling pointers after the initialiser scope ends.
 *
 * Compile-time gate: only active when ENABLE_HOMEKIT is defined.
 * Enable by uncommenting #define ENABLE_HOMEKIT in PresenceConfig.h and
 * installing the arduino-homekit-esp32 library (by Mixiaoxiao) via the
 * Arduino Library Manager.
 *
 * Exposed symbols (accessed via extern "C" in PresenceIntegrations.cpp):
 *   char                    hkPasswordBuf[12]  — pairing code "XXX-XX-XXX"
 *   homekit_characteristic_t hk_ch_occupancy   — occupancy characteristic
 *   homekit_server_config_t  hk_config          — server config for setup
 */

#ifdef ENABLE_HOMEKIT

#include "PresenceConfig.h"
#include <homekit/homekit.h>
#include <homekit/characteristics.h>

/*
 * hkPasswordBuf - Runtime pairing code buffer.
 * Written by initHomeKit() in PresenceIntegrations.cpp before
 * arduino_homekit_setup() is called.  Format: "XXX-XX-XXX\0" (11 chars).
 */
char hkPasswordBuf[12] = "111-22-333";

static void hkAccessoryIdentify(homekit_value_t v) { (void)v; }

/*
 * hk_ch_occupancy - OCCUPANCY_DETECTED characteristic.
 * Changed at runtime; PresenceIntegrations.cpp calls
 * homekit_characteristic_notify() whenever presence state changes.
 */
homekit_characteristic_t hk_ch_occupancy =
  HOMEKIT_CHARACTERISTIC_(OCCUPANCY_DETECTED, 0);

homekit_accessory_t* hk_accessories[] = {
  HOMEKIT_ACCESSORY(
    .category = homekit_accessory_category_sensor,
    .services = (homekit_service_t*[]) {
      HOMEKIT_SERVICE(ACCESSORY_INFORMATION,
        .characteristics = (homekit_characteristic_t*[]) {
          HOMEKIT_CHARACTERISTIC(NAME,              "Presence Sensor"),
          HOMEKIT_CHARACTERISTIC(MANUFACTURER,      "ESP32"),
          HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER,     "001"),
          HOMEKIT_CHARACTERISTIC(MODEL,             "LD2410C"),
          HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, FIRMWARE_VERSION),
          HOMEKIT_CHARACTERISTIC(IDENTIFY,          hkAccessoryIdentify),
          NULL
        }
      ),
      HOMEKIT_SERVICE(OCCUPANCY_SENSOR, .primary = true,
        .characteristics = (homekit_characteristic_t*[]) {
          &hk_ch_occupancy,
          NULL
        }
      ),
      NULL
    }
  ),
  NULL
};

homekit_server_config_t hk_config = {
  .accessories = hk_accessories,
  .password    = hkPasswordBuf,
  .setupId     = "1QJ8"
};

#endif  /* ENABLE_HOMEKIT */
