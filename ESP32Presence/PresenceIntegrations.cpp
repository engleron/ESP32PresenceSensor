#include "PresenceIntegrations.h"
#include "PresenceCore.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
bool isValidHexChar(char c) {
  return (c >= '0' && c <= '9') ||
         (c >= 'A' && c <= 'F') ||
         (c >= 'a' && c <= 'f');
}

bool isValidInsteonHexAddress(const String& hex) {
  if (hex.length() != 6) return false;
  for (size_t i = 0; i < hex.length(); i++) {
    if (!isValidHexChar(hex.charAt(i))) return false;
  }
  return true;
}

TaskHandle_t gIntegrationWorkerHandle = nullptr;
volatile bool gInsteonDesiredOn = false;
volatile bool gInsteonCommandQueued = false;
volatile bool gInsteonCommandInFlight = false;
volatile unsigned long gInsteonNextAllowedAttemptMs = 0;

bool timeReached(unsigned long now, unsigned long target) {
  return (long)(now - target) >= 0;
}

int sendInsteonHttpGet(const String& url) {
  HTTPClient http;
  http.setConnectTimeout(INSTEON_HTTP_CONNECT_TIMEOUT_MS);
  http.setTimeout(INSTEON_HTTP_TIMEOUT_MS);
  http.begin(url);
  http.addHeader("Connection", "close");
  http.setAuthorization(insteonHubUser.c_str(), insteonHubPass.c_str());
  int code = http.GET();
  http.end();
  return code;
}

bool clearInsteonHubBuffer() {
  String clearUrl = "http://" + insteonHubIP + ":" + insteonHubPort + "/1?XB=M=1";
  int clearCode = sendInsteonHttpGet(clearUrl);
  if (clearCode <= 0) {
    serialPrintln("Insteon Hub clear-buffer failed: " + HTTPClient::errorToString(clearCode) + " (" + String(clearCode) + ")");
    return false;
  }
  serialPrintln("Insteon Hub clear-buffer HTTP: " + String(clearCode));
  return (clearCode == 200);
}

void integrationWorkerTask(void*) {
  for (;;) {
    if (configMode) {
      gInsteonCommandQueued = false;
      gInsteonNextAllowedAttemptMs = 0;
      vTaskDelay(pdMS_TO_TICKS(25));
      continue;
    }

    if (integrationMode == "insteon_hub" && integrationConfigured && gInsteonCommandQueued && !gInsteonCommandInFlight) {
      unsigned long now = millis();
      if (timeReached(now, gInsteonNextAllowedAttemptMs)) {
        bool desired = gInsteonDesiredOn;
        gInsteonCommandQueued = false;
        gInsteonCommandInFlight = true;

        bool ok = sendInsteonHubCommand(desired);
        gInsteonCommandInFlight = false;

        if (ok) {
          lightOn = desired;
          gInsteonNextAllowedAttemptMs = 0;
          serialPrintln("Light state set to " + String(desired ? "ON" : "OFF"));
        } else {
          gInsteonDesiredOn = desired;
          gInsteonCommandQueued = true;
          gInsteonNextAllowedAttemptMs = millis() + LIGHT_COMMAND_RETRY_INTERVAL_MS;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(25));
  }
}
}  // namespace


/*
 * ================================================================
 * SECTION: LIGHT CONTROL (EISY/ISY)
 * ================================================================
 */

/*
 * sendISYCommand - Send a REST command to the EISY/ISY controller.
 * @param url Full REST URL to send GET request to
 * @return true if HTTP 200 received, false on error
 */
bool sendISYCommand(const String& url) {
  if (WiFi.status() != WL_CONNECTED) {
    serialPrintln(F("WiFi not connected - cannot send command"));
    return false;
  }

  HTTPClient http;
  bool success = false;

  if (useHTTPS) {
    WiFiClientSecure* client = new WiFiClientSecure;
    if (client) {
      client->setInsecure();  // Accept self-signed certs (local EISY)
      http.begin(*client, url);
      http.setAuthorization(isyUsername.c_str(), isyPassword.c_str());
      int code = http.GET();
      success = (code == 200);
      verbosePrint("ISY HTTPS response: " + String(code));
      http.end();
      delete client;
    }
  } else {
    http.begin(url);
    http.setAuthorization(isyUsername.c_str(), isyPassword.c_str());
    int code = http.GET();
    success = (code == 200);
    verbosePrint("ISY HTTP response: " + String(code));
    http.end();
  }

  return success;
}

/*
 * turnLightOn - Send DON command to EISY/ISY to turn light on.
 */
void turnLightOn() {
  serialPrintln(F("Turning light ON..."));
  String deviceID = isyDeviceID;
  deviceID.replace(" ", "%20");
  String protocol = useHTTPS ? "https" : "http";
  String url = protocol + "://" + isyIP + "/rest/nodes/" + deviceID + "/cmd/DON";

  if (sendISYCommand(url)) {
    lightOn = true;
    serialPrintln(F("Light ON"));
  } else {
    serialPrintln(F("Failed to turn light ON"));
  }
}

/*
 * turnLightOff - Send DOF command to EISY/ISY to turn light off.
 */
void turnLightOff() {
  serialPrintln(F("Turning light OFF..."));
  String deviceID = isyDeviceID;
  deviceID.replace(" ", "%20");
  String protocol = useHTTPS ? "https" : "http";
  String url = protocol + "://" + isyIP + "/rest/nodes/" + deviceID + "/cmd/DOF";

  if (sendISYCommand(url)) {
    lightOn = false;
    serialPrintln(F("Light OFF"));
  } else {
    serialPrintln(F("Failed to turn light OFF"));
  }
}

/*
 * insteonAddrToHex - Convert "1A.2B.3C" to "1A2B3C" for Insteon Hub API.
 * @param addr Insteon address with dot separators
 * @return Hex string without separators, uppercase
 */
String insteonAddrToHex(const String& addr) {
  String h = addr;
  h.replace(".", "");
  h.toUpperCase();
  return h;
}

/*
 * sendInsteonHubCommand - Send a direct ON/OFF command to Insteon Hub 2 (Model 2245).
 * @param on true to turn on, false to turn off
 * @return true if HTTP 200 received
 */
bool sendInsteonHubCommand(bool on) {
  if (WiFi.status() != WL_CONNECTED) {
    serialPrintln(F("Insteon Hub command skipped: WiFi not connected"));
    return false;
  }
  if (insteonHubIP == "" || insteonHubPort == "" || insteonHubUser == "" || insteonHubPass == "" || insteonHubAddr == "") {
    serialPrintln(F("Insteon Hub command skipped: configuration incomplete (need IP/port/user/pass/address)"));
    return false;
  }

  String hex = insteonAddrToHex(insteonHubAddr);
  if (!isValidInsteonHexAddress(hex)) {
    serialPrintln("Insteon Hub command skipped: invalid device address format '" + insteonHubAddr + "' (expected AA.BB.CC)");
    return false;
  }

  String cmd1 = on ? "11" : "13";
  String cmd2 = on ? "FF" : "00";
  String action = on ? "ON" : "OFF";
  serialPrintln("Insteon Hub: sending " + action + " to " + insteonHubAddr + " via " + insteonHubIP + ":" + insteonHubPort);

  // Direct send first; clear-buffer retry only on failure.
  String cmdUrl = "http://" + insteonHubIP + ":" + insteonHubPort +
                  "/3?0262" + hex + "0F" + cmd1 + cmd2 + "=I=3";
  int code = sendInsteonHttpGet(cmdUrl);
  if (code == 200) {
    serialPrintln("Insteon Hub command accepted (HTTP 200): " + action);
    return true;
  }

  if (code <= 0) {
    serialPrintln("Insteon Hub command attempt 1 transport error: " + HTTPClient::errorToString(code) + " (" + String(code) + ")");
  } else {
    serialPrintln("Insteon Hub command attempt 1 failed: HTTP " + String(code));
  }

  clearInsteonHubBuffer();
  delay(INSTEON_INTER_REQUEST_DELAY_MS);
  delay(INSTEON_RETRY_DELAY_MS);

  code = sendInsteonHttpGet(cmdUrl);
  if (code == 200) {
    serialPrintln("Insteon Hub command accepted (HTTP 200) after retry: " + action);
    return true;
  }

  if (code <= 0) {
    serialPrintln("Insteon Hub command failed: " + HTTPClient::errorToString(code) + " (" + String(code) + ")");
  } else {
    serialPrintln("Insteon Hub command failed: HTTP " + String(code));
  }
  return false;
}

/*
 * sendHACommand - Send turn_on / turn_off to the Home Assistant REST API.
 * @param on true to turn on, false to turn off
 * @return true if HTTP 200 or 201 received
 */
bool sendHACommand(bool on) {
  if (WiFi.status() != WL_CONNECTED) return false;
  String service = on ? "turn_on" : "turn_off";
  String url = String(haHTTPS ? "https" : "http") + "://" + haIP + ":" + haPort +
               "/api/services/homeassistant/" + service;
  String body = "{\"entity_id\":\"" + haEntityId + "\"}";
  bool success = false;

  HTTPClient http;
  if (haHTTPS) {
    WiFiClientSecure* client = new WiFiClientSecure;
    client->setInsecure();
    http.begin(*client, url);
    http.addHeader("Authorization", "Bearer " + haToken);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(body);
    success = (code == 200 || code == 201);
    verbosePrint("HA HTTPS response: " + String(code));
    http.end();
    delete client;
  } else {
    http.begin(url);
    http.addHeader("Authorization", "Bearer " + haToken);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(body);
    success = (code == 200 || code == 201);
    verbosePrint("HA HTTP response: " + String(code));
    http.end();
  }
  return success;
}

/*
 * sendHASensorState - POST a presence state string to the HA REST state endpoint.
 * Creates or updates a sensor entity in Home Assistant with the given state value.
 * When haEntitySrc == "uart" and fresh UART data is available, distance and
 * energy attributes are included so HA automations can use the richer data.
 * @param state Presence state string
 * @return true if HTTP 200 or 201 received
 */
bool sendHASensorState(const String& state) {
  if (WiFi.status() != WL_CONNECTED) return false;
  String url = String(haHTTPS ? "https" : "http") + "://" + haIP + ":" + haPort +
               "/api/states/" + haEntityId;

  // Base attributes always included.
  String body = "{\"state\":\"" + state + "\","
                "\"attributes\":{\"friendly_name\":\"Presence Sensor\"";

  // Append LD2410C UART detail attributes when available and fresh (< 3 s old).
  bool uartFresh = (haEntitySrc == "uart" &&
                    lastUartUpdateMs > 0 &&
                    (millis() - lastUartUpdateMs) < 3000UL);
  if (uartFresh) {
    body += ",\"moving_distance_cm\":"     + String(uartMovingDistance);
    body += ",\"moving_energy\":"          + String(uartMovingEnergy);
    body += ",\"stationary_distance_cm\":" + String(uartStationaryDistance);
    body += ",\"stationary_energy\":"      + String(uartStationaryEnergy);
    body += ",\"detection_distance_cm\":"  + String(uartDetectionDistance);
  }
  body += "}}";

  bool success = false;
  HTTPClient http;
  if (haHTTPS) {
    WiFiClientSecure client;
    client.setInsecure();
    http.begin(client, url);
    http.addHeader("Authorization", "Bearer " + haToken);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(body);
    success = (code == 200 || code == 201);
    verbosePrint("HA sensor POST HTTPS: " + String(code));
    http.end();
  } else {
    http.begin(url);
    http.addHeader("Authorization", "Bearer " + haToken);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(body);
    success = (code == 200 || code == 201);
    verbosePrint("HA sensor POST HTTP: " + String(code));
    http.end();
  }
  return success;
}

/*
 * updateHASensorEntity - Push current presence state to HA as a sensor entity.
 * Sends on state change or after republish interval to keep the entity alive.
 *
 * haEntitySrc == "uart": reports "movement" | "stationary" | "presence" | "clear"
 *   derived from LD2410C UART data; falls back to OUT-pin as "presence" | "clear"
 *   when UART is stale so state vocabulary stays consistent.
 * haEntitySrc == "out_pin": reports "detected" | "clear" from GPIO OUT pin.
 */
void updateHASensorEntity() {
  static String lastSentState = "";
  static unsigned long lastPublishMs = 0;

  if (!integrationConfigured || WiFi.status() != WL_CONNECTED) return;

  String state;
  if (haEntitySrc == "uart") {
    bool uartFresh = (lastUartUpdateMs > 0 && (millis() - lastUartUpdateMs) < 3000UL);
    if (!uartFresh) {
      // UART not connected or stale — fall back to OUT pin using same state vocabulary.
      state = presenceDetected ? "presence" : "clear";
    } else {
      bool moving     = (uartTargetState == 0x01 || uartTargetState == 0x03);
      bool stationary = (uartTargetState == 0x02 || uartTargetState == 0x03);
      if (moving && stationary) {
        state = "presence";    // both simultaneously
      } else if (moving) {
        state = "movement";
      } else if (stationary) {
        state = "stationary";
      } else {
        state = "clear";
      }
    }
  } else {
    state = presenceDetected ? "detected" : "clear";
  }

  unsigned long now = millis();
  if (state == lastSentState && (now - lastPublishMs) < HA_SENSOR_REPUBLISH_MS) return;

  if (sendHASensorState(state)) {
    lastSentState = state;
    lastPublishMs = now;
    serialPrintln("HA sensor entity: " + state);
  }
}

void initIntegrationWorker() {
  if (gIntegrationWorkerHandle != nullptr) return;
  BaseType_t ok = xTaskCreatePinnedToCore(
    integrationWorkerTask,
    "integrationWorker",
    8192,
    nullptr,
    1,
    &gIntegrationWorkerHandle,
    0
  );
  if (ok == pdPASS) {
    serialPrintln(F("Integration worker task started"));
  } else {
    serialPrintln(F("Integration worker task failed to start"));
  }
}

/*
 * controlLight - Decide whether to turn the light on or off based on presence.
 * Dispatches to the configured integration (ISY, Insteon Hub, or Home Assistant).
 */
void controlLight() {
  static bool loggedDisabled = false;
  static unsigned long lastAttemptMs = 0;
  static bool lastAttemptWasOn = false;
  const unsigned long retryIntervalMs = LIGHT_COMMAND_RETRY_INTERVAL_MS;

  if (!integrationConfigured) {
    if (integrationMode != "none" && !loggedDisabled) {
      serialPrintln("Light control disabled: integration '" + integrationMode + "' is not fully configured");
      loggedDisabled = true;
    }
    return;
  }
  loggedDisabled = false;

#ifdef ENABLE_HOMEKIT
  if (integrationMode == "homekit") {
    updateHomeKitState();
    return;
  }
#else
  if (integrationMode == "homekit") return;  // HomeKit disabled at compile time
#endif

  if (integrationMode == "ha" && haMode == "sensor_entity") {
    updateHASensorEntity();
    return;
  }

  bool shouldBeOn = presenceDetected ||
    (millis() - lastDetectionTime < (unsigned long)noDetectionTimeout * 1000UL);
  if (integrationMode == "insteon_hub") {
    if (shouldBeOn == lightOn && !gInsteonCommandQueued && !gInsteonCommandInFlight) return;
    if ((gInsteonCommandQueued || gInsteonCommandInFlight) && gInsteonDesiredOn == shouldBeOn) return;

    gInsteonDesiredOn = shouldBeOn;
    gInsteonCommandQueued = true;
    gInsteonNextAllowedAttemptMs = millis();
    serialPrintln("Light control queued: " + String(shouldBeOn ? "ON" : "OFF") + " via insteon_hub");
    return;
  }

  if (shouldBeOn == lightOn) return;

  unsigned long now = millis();
  if ((now - lastAttemptMs) < retryIntervalMs && lastAttemptWasOn == shouldBeOn) {
    return;
  }
  lastAttemptMs = now;
  lastAttemptWasOn = shouldBeOn;

  serialPrintln("Light control request: " + String(shouldBeOn ? "ON" : "OFF") + " via " + integrationMode);

  bool ok = false;
  if (shouldBeOn) {
    if      (integrationMode == "isy")         { turnLightOn();  return; }
    else if (integrationMode == "ha")          ok = sendHACommand(true);
    if (ok) {
      lightOn = true;
      lastAttemptMs = 0;
      serialPrintln(F("Light state set to ON"));
    } else {
      serialPrintln("Light ON command failed via " + integrationMode);
    }
  } else {
    if      (integrationMode == "isy")         { turnLightOff(); return; }
    else if (integrationMode == "ha")          ok = sendHACommand(false);
    if (ok) {
      lightOn = false;
      lastAttemptMs = 0;
      serialPrintln(F("Light state set to OFF"));
    } else {
      serialPrintln("Light OFF command failed via " + integrationMode);
    }
  }
}


#ifdef ENABLE_HOMEKIT
/*
 * ================================================================
 * SECTION: NATIVE HOMEKIT (HomeSpan)
 * ================================================================
 *
 * Uses the HomeSpan library (available in the Arduino Library Manager).
 * Install: Arduino IDE → Tools → Manage Libraries → search "HomeSpan".
 *
 * Exposes TWO services inside a single "Presence Sensor" accessory:
 *
 *   MotionSensor   "Motion"    — Immediate ON for movement; clears after
 *                               hkMotionClearSecs of no movement (default 20 s).
 *                               Use this for "someone just walked in" automations.
 *
 *   OccupancySensor "Occupancy" — ON for any valid presence (moving OR still).
 *                               Stays ON until noDetectionTimeout seconds of
 *                               continuous no-presence (default 5 min). This is
 *                               the right trigger for "keep the light on" rules.
 *
 * Both timers run entirely inside ESP32 firmware — no HomeKit shortcut
 * waits or external automation delays are needed.
 *
 * State source priority:
 *   1. UART (uartTargetState) when a valid frame arrived within 2 s.
 *      Bit 0 = moving target present; Bit 1 = stationary target present.
 *   2. OUT pin (presenceDetected) as binary fallback when UART is stale.
 */

#include <HomeSpan.h>

// MotionSensor service — "Motion" tile in Apple Home.
struct HKMotionSensor : Service::MotionSensor {
  SpanCharacteristic *motionDetected;
  HKMotionSensor() : Service::MotionSensor() {
    new Characteristic::Name("Motion");
    motionDetected = new Characteristic::MotionDetected(0);
  }
};

// OccupancySensor service — "Occupancy" tile in Apple Home.
struct HKOccupancySensor : Service::OccupancySensor {
  SpanCharacteristic *occupancyDetected;
  HKOccupancySensor() : Service::OccupancySensor() {
    new Characteristic::Name("Occupancy");
    occupancyDetected = new Characteristic::OccupancyDetected(0);
  }
};

static HKMotionSensor    *hkMotion      = nullptr;
static HKOccupancySensor *hkSensor      = nullptr;
static bool               hkInitialized = false;

// Internal state — track what we last reported to HomeKit and when.
static bool          hkMotionActive       = false;
static bool          hkOccupancyActive    = false;
static unsigned long hkMotionLastOnMs     = 0;   // millis() of last movement frame
static unsigned long hkOccupancyLastOnMs  = 0;   // millis() of last any-presence frame

/*
 * initHomeKit - Configure pairing code and start the HomeSpan accessory server.
 * Must be called once after WiFi connects.
 */
void initHomeKit() {
  if (hkInitialized || integrationMode != "homekit") return;

  // Convert stored 8-digit code (e.g. "11122333") to "111-22-333" for display.
  String code = homekitCode;
  if (code.length() != 8) code = "11122333";
  String fmt = code.substring(0, 3) + "-" + code.substring(3, 5) + "-" + code.substring(5);

  homeSpan.setLogLevel(0);                          // suppress HomeSpan serial noise
  homeSpan.setPortNum(1234);                        // HAP port — avoids conflict with WebServer on 80
  homeSpan.setPairingCode(code.c_str());            // raw 8 digits — HomeSpan rejects hyphens
  homeSpan.begin(Category::Sensors, "Presence Sensor", "presence");  // mDNS hostname: presence.local

  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Name("Presence Sensor");
      new Characteristic::Manufacturer("ESP32");
      new Characteristic::SerialNumber("001");
      new Characteristic::Model("LD2410C");
      new Characteristic::FirmwareRevision(FIRMWARE_VERSION);
      new Characteristic::Identify();
    hkMotion = new HKMotionSensor();
    hkSensor = new HKOccupancySensor();

  hkInitialized = true;
  serialPrintln("HomeKit accessory started. Pairing code: " + fmt);
  serialPrintln(F("Open Apple Home > + > Add Accessory > More Options to pair."));
  serialPrintln("HomeKit motion clear: " + String(hkMotionClearSecs) + "s  occupancy timeout: " +
                String(noDetectionTimeout) + "s");
}

/*
 * homeKitLoop - Drive the HomeSpan server; call every main-loop tick.
 */
void homeKitLoop() {
  if (hkInitialized) homeSpan.poll();
}

/*
 * updateHomeKitState - Two-sensor state machine driven by LD2410C data.
 *
 * Motion:    ON immediately when movement seen; OFF after hkMotionClearSecs
 *            of no movement (handles brief dropouts without false clears).
 * Occupancy: ON immediately when any presence (moving or still); OFF only
 *            after noDetectionTimeout seconds of continuous no-presence
 *            (the office occupancy hold timer).
 *
 * Called every loop tick from controlLight() when integrationMode == "homekit".
 */
void updateHomeKitState() {
  if (!hkInitialized || !hkMotion || !hkSensor) return;

  unsigned long now = millis();

  // Use UART if a valid frame arrived within the last 2 s; otherwise fall
  // back to the binary OUT pin signal for occupancy only. For motion, treat
  // stale UART as "no new motion" and let the motion timer clear it.
  const unsigned long uartStaleLimitMs = 2000UL;
  bool uartFresh = (lastUartUpdateMs > 0) && (now - lastUartUpdateMs < uartStaleLimitMs);

  bool movingNow      = uartFresh ? (uartTargetState == 1 || uartTargetState == 3)
                                  : false;
  bool anyPresenceNow = uartFresh ? (uartTargetState != 0)
                                   : presenceDetected;

  // ---- Motion sensor ----
  if (movingNow) {
    hkMotionLastOnMs = now;
    if (!hkMotionActive) {
      hkMotionActive = true;
      hkMotion->motionDetected->setVal(1);
      serialPrintln(F("HomeKit: motion ACTIVE"));
    }
  } else if (hkMotionActive) {
    unsigned long motionClearMs = (unsigned long)hkMotionClearSecs * 1000UL;
    if (now - hkMotionLastOnMs >= motionClearMs) {
      hkMotionActive = false;
      hkMotion->motionDetected->setVal(0);
      serialPrintln(F("HomeKit: motion CLEAR"));
    }
  }

  // ---- Occupancy sensor ----
  if (anyPresenceNow) {
    hkOccupancyLastOnMs = now;
    if (!hkOccupancyActive) {
      hkOccupancyActive = true;
      hkSensor->occupancyDetected->setVal(1);
      serialPrintln(F("HomeKit: occupancy DETECTED"));
    }
  } else if (hkOccupancyActive) {
    unsigned long occupancyTimeoutMs = (unsigned long)noDetectionTimeout * 1000UL;
    if (now - hkOccupancyLastOnMs >= occupancyTimeoutMs) {
      hkOccupancyActive = false;
      hkSensor->occupancyDetected->setVal(0);
      serialPrintln(F("HomeKit: occupancy CLEAR"));
    }
  }
}

#endif  // ENABLE_HOMEKIT
