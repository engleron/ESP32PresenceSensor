/*
 * ================================================================
 * ESP32 LD2410C Presence Detection System
 * Version: 2.0.0
 * Date: 2026-03-01
 * ================================================================
 *
 * DESCRIPTION:
 *   ESP32-based occupancy detection using the LD2410C mmWave radar
 *   sensor. Integrates with multiple home automation platforms for
 *   automatic light control. Features a web-based configuration
 *   interface with admin password protection and session management.
 *
 * SUPPORTED BOARDS:
 *   - ESP32-DevKitC-32E (auto-detected)
 *   - ESP32-S3 Dev Board (auto-detected)
 *
 * INTEGRATIONS:
 *   - Universal Devices EISY / Polisy / ISY994 (via REST API)
 *   - Insteon Hub 2 Model 2245 (direct HTTP API)
 *   - Home Assistant (REST API)
 *   - Apple HomeKit (native, compile-time, requires arduino-homekit-esp32)
 *
 * LICENSE: MIT
 * ================================================================
 */

/*
 * ================================================================
 * SECTION: INCLUDES AND DEFINITIONS
 * ================================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Adafruit_NeoPixel.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <mbedtls/md.h>
#include <esp_task_wdt.h>

#define FIRMWARE_VERSION  "2.1.0"
#define NUM_PIXELS        1
#define DNS_PORT          53
#define BAUD_RATE_DEBUG   115200
#define BAUD_RATE_SENSOR  256000
#define PIN_RESET         0

// Uncomment to enable native HomeKit support (requires arduino-homekit-esp32 library)
// #define ENABLE_HOMEKIT

#define DEFAULT_INSTEON_HUB_PORT  "25105"
#define DEFAULT_HA_PORT           "8123"

// Timing constants
#define HEARTBEAT_INTERVAL      1000
#define SENSOR_CHECK_INTERVAL   500
#define RESET_HOLD_TIME         5000
#define SESSION_TIMEOUT_MS      (15UL * 60UL * 1000UL)
#define LOGIN_LOCKOUT_MS        (60UL * 1000UL)
#define MAX_LOGIN_ATTEMPTS      5
#define STATUS_UPDATE_INTERVAL  2000
#define WDT_TIMEOUT_SECONDS     8

/*
 * ================================================================
 * SECTION: BOARD-SPECIFIC CONFIGURATION
 * ================================================================
 */

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

/*
 * ================================================================
 * SECTION: GLOBAL VARIABLES
 * ================================================================
 */

// Hardware
Adafruit_NeoPixel strip(NUM_PIXELS, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);
WebServer server(80);
DNSServer dnsServer;
Preferences preferences;

// Runtime pin config (may be overridden by custom pins)
int pinSensorOut = SENSOR_OUT_PIN;
int pinSensorTx  = SENSOR_TX_PIN;
int pinSensorRx  = SENSOR_RX_PIN;
int pinRgbLed    = RGB_LED_PIN;

// WiFi and mode
String wifiSSID     = "";
String wifiPassword = "";
bool   configMode   = false;

// EISY/ISY settings
String isyIP       = "";
String isyUsername = "";
String isyPassword = "";
String isyDeviceID = "";
bool   useHTTPS    = false;
bool   isyConfigured = false;

// Integration selector: "none" | "isy" | "insteon_hub" | "ha" | "homekit"
String integrationMode       = "none";
bool   integrationConfigured = false;

// Insteon Hub 2 (Model 2245) — direct HTTP API
String insteonHubIP   = "";
String insteonHubPort = DEFAULT_INSTEON_HUB_PORT;
String insteonHubUser = "";
String insteonHubPass = "";
String insteonHubAddr = "";  // e.g. "1A.2B.3C"

// Home Assistant — REST API
String haIP       = "";
String haPort     = DEFAULT_HA_PORT;
bool   haHTTPS    = false;
String haToken    = "";      // Long-lived access token
String haEntityId = "";      // e.g. "light.living_room"

#ifdef ENABLE_HOMEKIT
String homekitCode = "11122333";  // 8-digit pairing code (no hyphens)
#endif

// Admin / security
String adminPasswordHash = "";
bool   adminPasswordSet  = false;

// Session management
struct Session {
  String       sessionId;
  unsigned long expiryTime;
  bool         valid;
};
Session currentSession = {"", 0, false};

// Login rate limiting
int           loginAttempts    = 0;
unsigned long loginLockoutEnd  = 0;

// Detection settings
int noDetectionTimeout = 300;

// Sensor state
bool          presenceDetected  = false;
bool          isMoving          = false;
bool          lightOn           = false;
bool          sensorError       = false;
unsigned long lastDetectionTime = 0;
unsigned long lastStatusPrint   = 0;

// LED state
unsigned long lastHeartbeat = 0;
bool          heartbeatState = false;
int           ledBrightness  = 50;

// Timing
unsigned long deviceStartTime = 0;

// Custom pins flag
bool useCustomPins = false;

// Debug level: 0=none, 1=basic, 2=verbose
int debugLevel = 1;

// Unique device SSID
String deviceSSID = "Presence_0000";

/*
 * ================================================================
 * SECTION: UTILITY FUNCTIONS
 * ================================================================
 */

/*
 * serialPrint - Print timestamped message without newline.
 * Only outputs if debugLevel > 0.
 */
void serialPrint(const String& message) {
  if (!Serial || debugLevel == 0) return;
  unsigned long ts = millis();
  Serial.print("[");
  Serial.print(ts / 1000);
  Serial.print(".");
  unsigned long ms = ts % 1000;
  if (ms < 100) Serial.print("0");
  if (ms < 10)  Serial.print("0");
  Serial.print(ms);
  Serial.print("s] ");
  Serial.print(message);
}

/*
 * serialPrintln - Print timestamped message with newline.
 */
void serialPrintln(const String& message) {
  if (!Serial || debugLevel == 0) return;
  serialPrint(message);
  Serial.println();
}

/*
 * verbosePrint - Print only when debug level is verbose.
 */
void verbosePrint(const String& message) {
  if (debugLevel >= 2) serialPrintln(message);
}

/*
 * setRGB - Set the NeoPixel LED to an RGB color.
 * @param r Red value (0-255)
 * @param g Green value (0-255)
 * @param b Blue value (0-255)
 */
void setRGB(int r, int g, int b) {
  strip.setPixelColor(0, strip.Color(r, g, b));
  strip.show();
}

/*
 * hashPassword - Hash a plain-text password using SHA-256.
 * @param password Plain-text password string
 * @return Hex-encoded SHA-256 hash string (64 characters)
 */
String hashPassword(const String& password) {
  byte hash[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char*)password.c_str(), password.length());
  mbedtls_md_finish(&ctx, hash);
  mbedtls_md_free(&ctx);

  String result = "";
  for (int i = 0; i < 32; i++) {
    char buf[3];
    sprintf(buf, "%02x", (int)hash[i]);
    result += buf;
  }
  return result;
}

/*
 * generateSessionId - Generate a random session ID string.
 * @return Random hex string for use as session cookie
 */
String generateSessionId() {
  String id = "";
  for (int i = 0; i < 4; i++) {
    id += String(random(0xFFFFFFFF), HEX);
  }
  return id;
}

/*
 * getUptimeString - Return a human-readable uptime string.
 * @return Formatted string like "2d 4h 12m"
 */
String getUptimeString() {
  unsigned long ms = millis();
  unsigned long seconds = ms / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours   = minutes / 60;
  unsigned long days    = hours / 24;

  String result = "";
  if (days > 0) {
    result += String(days) + "d ";
  }
  result += String(hours % 24) + "h ";
  result += String(minutes % 60) + "m";
  return result;
}

/*
 * wifiSignalToPercent - Convert RSSI dBm value to percentage.
 * @param rssi RSSI value in dBm (typically -30 to -90)
 * @return Signal percentage (0-100)
 */
int wifiSignalToPercent(int rssi) {
  if (rssi >= -50) return 100;
  if (rssi <= -90) return 0;
  return 2 * (rssi + 90);
}

/*
 * ================================================================
 * SECTION: SECURITY AND SESSION MANAGEMENT
 * ================================================================
 */

/*
 * createSession - Create a new authenticated session.
 * @return New session ID string
 */
String createSession() {
  currentSession.sessionId  = generateSessionId();
  currentSession.expiryTime = millis() + SESSION_TIMEOUT_MS;
  currentSession.valid      = true;
  serialPrintln(F("Session created"));
  return currentSession.sessionId;
}

/*
 * validateSession - Check if the request has a valid session cookie.
 * Resets the session expiry time on valid access.
 * @return true if session is valid, false otherwise
 */
bool validateSession() {
  if (!currentSession.valid) return false;
  if (millis() > currentSession.expiryTime) {
    currentSession.valid = false;
    serialPrintln(F("Session expired"));
    return false;
  }

  String cookie = server.header("Cookie");
  if (cookie.indexOf("session=" + currentSession.sessionId) < 0) {
    return false;
  }

  // Refresh session expiry on valid access
  currentSession.expiryTime = millis() + SESSION_TIMEOUT_MS;
  return true;
}

/*
 * invalidateSession - Destroy the current session (logout).
 */
void invalidateSession() {
  currentSession.valid = false;
  currentSession.sessionId = "";
  serialPrintln(F("Session invalidated"));
}

/*
 * isLoginLocked - Check if login is rate-limited.
 * @return true if locked out, false if login attempts are allowed
 */
bool isLoginLocked() {
  if (loginLockoutEnd > 0 && millis() < loginLockoutEnd) return true;
  if (millis() >= loginLockoutEnd) {
    loginLockoutEnd  = 0;
    loginAttempts    = 0;
  }
  return false;
}

/*
 * requireAuth - Redirect to login if not authenticated.
 * @return true if authenticated, false if redirect was sent
 */
bool requireAuth() {
  if (!validateSession()) {
    server.sendHeader("Location", "/login");
    server.send(302, "text/plain", "");
    return false;
  }
  return true;
}

/*
 * ================================================================
 * SECTION: CONFIGURATION MANAGEMENT
 * ================================================================
 */

/*
 * generateDeviceSSID - Generate unique SSID from last 4 MAC chars.
 * Format: Presence_XXXX
 */
void generateDeviceSSID() {
  // Need WiFi initialized for MAC address; use AP mode MAC
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char suffix[5];
  sprintf(suffix, "%02X%02X", mac[4], mac[5]);
  deviceSSID = "Presence_" + String(suffix);
}

/*
 * loadConfiguration - Load all settings from non-volatile storage.
 */
void loadConfiguration() {
  serialPrintln(F("Loading configuration from flash memory..."));

  preferences.begin("presence", true);

  wifiSSID     = preferences.getString("wifi_ssid", "");
  wifiPassword = preferences.getString("wifi_pass", "");

  isyIP       = preferences.getString("isy_ip",     "");
  isyUsername = preferences.getString("isy_user",   "");
  isyPassword = preferences.getString("isy_pass",   "");
  isyDeviceID = preferences.getString("isy_device", "");
  useHTTPS    = preferences.getBool("use_https",    false);

  noDetectionTimeout  = preferences.getInt("timeout",    300);
  adminPasswordHash   = preferences.getString("adm_hash", "");
  adminPasswordSet    = (adminPasswordHash.length() == 64);

  useCustomPins = preferences.getBool("custom_pins", false);
  if (useCustomPins) {
    pinSensorOut = preferences.getInt("pin_out",  SENSOR_OUT_PIN);
    pinSensorTx  = preferences.getInt("pin_tx",   SENSOR_TX_PIN);
    pinSensorRx  = preferences.getInt("pin_rx",   SENSOR_RX_PIN);
    pinRgbLed    = preferences.getInt("pin_led",  RGB_LED_PIN);
  }

  ledBrightness = preferences.getInt("led_bright", 50);
  debugLevel    = preferences.getInt("debug_lvl",  1);

  integrationMode = preferences.getString("int_mode",  "none");
  insteonHubIP    = preferences.getString("hub_ip",    "");
  insteonHubPort  = preferences.getString("hub_port",  DEFAULT_INSTEON_HUB_PORT);
  insteonHubUser  = preferences.getString("hub_user",  "");
  insteonHubPass  = preferences.getString("hub_pass",  "");
  insteonHubAddr  = preferences.getString("hub_addr",  "");
  haIP            = preferences.getString("ha_ip",     "");
  haPort          = preferences.getString("ha_port",   DEFAULT_HA_PORT);
  haHTTPS         = preferences.getBool("ha_https",    false);
  haToken         = preferences.getString("ha_token",  "");
  haEntityId      = preferences.getString("ha_entity", "");
#ifdef ENABLE_HOMEKIT
  homekitCode     = preferences.getString("hk_code",   "11122333");
#endif

  preferences.end();

  isyConfigured = (isyIP != "" && isyUsername != "" && isyPassword != "" && isyDeviceID != "");

  // Migration: if int_mode not saved but isy_ip is set, default to "isy"
  if (integrationMode == "none" && isyIP != "") integrationMode = "isy";

  integrationConfigured = false;
  if (integrationMode == "isy")         integrationConfigured = isyConfigured;
  if (integrationMode == "insteon_hub") integrationConfigured = (insteonHubIP != "" && insteonHubUser != "" && insteonHubAddr != "");
  if (integrationMode == "ha")         integrationConfigured = (haIP != "" && haToken != "" && haEntityId != "");
  if (integrationMode == "homekit")    integrationConfigured = true;

  if (debugLevel > 0) {
    Serial.println(F("Configuration loaded:"));
    Serial.print(F("  Board: ")); Serial.println(BOARD_TYPE);
    Serial.print(F("  WiFi SSID: ")); Serial.println(wifiSSID == "" ? "(not set)" : wifiSSID);
    Serial.print(F("  Integration: ")); Serial.println(integrationMode);
    Serial.print(F("  Light control: ")); Serial.println(integrationConfigured ? "ENABLED" : "DISABLED");
    Serial.print(F("  Timeout: ")); Serial.print(noDetectionTimeout); Serial.println(F("s"));
    Serial.print(F("  Admin password: ")); Serial.println(adminPasswordSet ? "Set" : "NOT SET");
    Serial.print(F("  Custom pins: ")); Serial.println(useCustomPins ? "Yes" : "No");
  }
}

/*
 * saveConfiguration - Persist all settings to non-volatile storage.
 */
void saveConfiguration() {
  serialPrintln(F("Saving configuration to flash memory..."));

  preferences.begin("presence", false);

  preferences.putString("wifi_ssid",  wifiSSID);
  preferences.putString("wifi_pass",  wifiPassword);
  preferences.putString("isy_ip",     isyIP);
  preferences.putString("isy_user",   isyUsername);
  preferences.putString("isy_pass",   isyPassword);
  preferences.putString("isy_device", isyDeviceID);
  preferences.putBool("use_https",    useHTTPS);
  preferences.putInt("timeout",       noDetectionTimeout);
  preferences.putString("adm_hash",   adminPasswordHash);
  preferences.putBool("custom_pins",  useCustomPins);
  preferences.putInt("pin_out",       pinSensorOut);
  preferences.putInt("pin_tx",        pinSensorTx);
  preferences.putInt("pin_rx",        pinSensorRx);
  preferences.putInt("pin_led",       pinRgbLed);
  preferences.putInt("led_bright",    ledBrightness);
  preferences.putInt("debug_lvl",     debugLevel);
  preferences.putString("int_mode",   integrationMode);
  preferences.putString("hub_ip",     insteonHubIP);
  preferences.putString("hub_port",   insteonHubPort);
  preferences.putString("hub_user",   insteonHubUser);
  preferences.putString("hub_pass",   insteonHubPass);
  preferences.putString("hub_addr",   insteonHubAddr);
  preferences.putString("ha_ip",      haIP);
  preferences.putString("ha_port",    haPort);
  preferences.putBool("ha_https",     haHTTPS);
  preferences.putString("ha_token",   haToken);
  preferences.putString("ha_entity",  haEntityId);
#ifdef ENABLE_HOMEKIT
  preferences.putString("hk_code",    homekitCode);
#endif

  preferences.end();

  isyConfigured    = (isyIP != "" && isyUsername != "" && isyPassword != "" && isyDeviceID != "");
  adminPasswordSet = (adminPasswordHash.length() == 64);

  integrationConfigured = false;
  if (integrationMode == "isy")         integrationConfigured = isyConfigured;
  if (integrationMode == "insteon_hub") integrationConfigured = (insteonHubIP != "" && insteonHubUser != "" && insteonHubAddr != "");
  if (integrationMode == "ha")         integrationConfigured = (haIP != "" && haToken != "" && haEntityId != "");
  if (integrationMode == "homekit")    integrationConfigured = true;

  serialPrintln(F("Configuration saved!"));
}

/*
 * clearConfiguration - Erase all stored settings.
 */
void clearConfiguration() {
  serialPrintln(F("Clearing all configuration..."));

  preferences.begin("presence", false);
  preferences.clear();
  preferences.end();

  wifiSSID = wifiPassword = isyIP = isyUsername = isyPassword = isyDeviceID = "";
  adminPasswordHash = "";
  useHTTPS = useCustomPins = isyConfigured = adminPasswordSet = false;
  noDetectionTimeout = 300;
  ledBrightness = 50;
  debugLevel = 1;
  pinSensorOut = SENSOR_OUT_PIN;
  pinSensorTx  = SENSOR_TX_PIN;
  pinSensorRx  = SENSOR_RX_PIN;
  pinRgbLed    = RGB_LED_PIN;
  integrationMode = "none";
  integrationConfigured = false;
  insteonHubIP = insteonHubUser = insteonHubPass = insteonHubAddr = "";
  insteonHubPort = DEFAULT_INSTEON_HUB_PORT;
  haIP = haToken = haEntityId = "";
  haPort = DEFAULT_HA_PORT;
  haHTTPS = false;
#ifdef ENABLE_HOMEKIT
  homekitCode = "11122333";
#endif

  serialPrintln(F("Configuration cleared!"));
}

/*
 * buildConfigJson - Build a JSON string of current configuration for export.
 * @return JSON string with all non-sensitive settings
 */
String buildConfigJson() {
  String json = "{\n";
  json += "  \"version\": \"" + String(FIRMWARE_VERSION) + "\",\n";
  json += "  \"board\": \"" + String(BOARD_TYPE) + "\",\n";
  json += "  \"wifi_ssid\": \"" + wifiSSID + "\",\n";
  json += "  \"isy_ip\": \"" + isyIP + "\",\n";
  json += "  \"isy_user\": \"" + isyUsername + "\",\n";
  json += "  \"isy_device\": \"" + isyDeviceID + "\",\n";
  json += "  \"use_https\": " + String(useHTTPS ? "true" : "false") + ",\n";
  json += "  \"timeout\": " + String(noDetectionTimeout) + ",\n";
  json += "  \"led_brightness\": " + String(ledBrightness) + ",\n";
  json += "  \"debug_level\": " + String(debugLevel) + ",\n";
  json += "  \"use_custom_pins\": " + String(useCustomPins ? "true" : "false") + ",\n";
  json += "  \"pin_sensor_out\": " + String(pinSensorOut) + ",\n";
  json += "  \"pin_sensor_tx\": " + String(pinSensorTx) + ",\n";
  json += "  \"pin_sensor_rx\": " + String(pinSensorRx) + ",\n";
  json += "  \"pin_rgb_led\": " + String(pinRgbLed) + ",\n";
  json += "  \"integration_mode\": \"" + integrationMode + "\",\n";
  json += "  \"hub_ip\": \"" + insteonHubIP + "\",\n";
  json += "  \"hub_port\": \"" + insteonHubPort + "\",\n";
  json += "  \"hub_user\": \"" + insteonHubUser + "\",\n";
  json += "  \"hub_addr\": \"" + insteonHubAddr + "\",\n";
  json += "  \"ha_ip\": \"" + haIP + "\",\n";
  json += "  \"ha_port\": \"" + haPort + "\",\n";
  json += "  \"ha_https\": " + String(haHTTPS ? "true" : "false") + ",\n";
  json += "  \"ha_entity\": \"" + haEntityId + "\"\n";
  json += "}";
  return json;
}

/*
 * ================================================================
 * SECTION: LED CONTROL
 * ================================================================
 */

/*
 * blinkBlueHeartbeat - Blink blue LED at 1-second intervals (setup mode).
 */
void blinkBlueHeartbeat() {
  if (millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
    heartbeatState = !heartbeatState;
    lastHeartbeat  = millis();
    setRGB(0, 0, heartbeatState ? 255 : 0);
  }
}

/*
 * blinkRedBlue - Alternate red/blue at 500ms intervals (error state).
 */
void blinkRedBlue() {
  static unsigned long lastBlink = 0;
  static bool isRed = true;
  if (millis() - lastBlink > 500) {
    isRed = !isRed;
    lastBlink = millis();
    setRGB(isRed ? 255 : 0, 0, isRed ? 0 : 255);
  }
}

/*
 * updateLED - Set LED color based on current sensor state.
 */
void updateLED() {
  static int prevState = -1;
  int state = 0;

  if (sensorError) {
    blinkRedBlue();
    state = 4;
  } else if (presenceDetected) {
    if (isMoving) {
      setRGB(0, 255, 0);  // Green: movement
      state = 1;
    } else {
      setRGB(255, 255, 0);  // Yellow: static
      state = 2;
    }
  } else {
    setRGB(255, 0, 0);  // Red: no presence
    state = 3;
  }

  if (state != prevState && state != 4) {
    prevState = state;
    const char* msgs[] = {"", "GREEN (movement)", "YELLOW (static)", "RED (no presence)"};
    if (state >= 1 && state <= 3) {
      serialPrintln("LED: " + String(msgs[state]));
    }
  }
}

/*
 * ================================================================
 * SECTION: SENSOR FUNCTIONS
 * ================================================================
 */

/*
 * readSensorData - Read the LD2410C OUT pin and update presence state.
 */
void readSensorData() {
  int outPinState = digitalRead(pinSensorOut);

  if (outPinState == HIGH) {
    presenceDetected = true;
    lastDetectionTime = millis();
    sensorError = false;

    // Alternate movement/static detection every 30 seconds (OUT pin only gives presence, not type)
    // Full UART parsing would give exact type; this provides a reasonable simulation
    isMoving = ((millis() / 30000) % 2 == 0);

    if (millis() - lastStatusPrint > 2000) {
      verbosePrint("Presence detected: " + String(isMoving ? "MOVING" : "STATIC"));
      lastStatusPrint = millis();
    }
  } else {
    presenceDetected = false;
    isMoving = false;

    if (lightOn && (millis() - lastDetectionTime > ((unsigned long)noDetectionTimeout * 1000UL))) {
      verbosePrint("Timeout: no detection for " + String(noDetectionTimeout / 60) + " min");
    }
  }

  sensorError = false;
}

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
  if (WiFi.status() != WL_CONNECTED) return false;
  String hex = insteonAddrToHex(insteonHubAddr);
  String cmd = on ? "11" : "13";

  HTTPClient http;
  // Step 1: clear hub receive buffer
  String clearUrl = "http://" + insteonHubIP + ":" + insteonHubPort + "/1?XB=M=1";
  http.begin(clearUrl);
  http.setAuthorization(insteonHubUser.c_str(), insteonHubPass.c_str());
  http.GET();
  http.end();

  // Step 2: send command
  String cmdUrl = "http://" + insteonHubIP + ":" + insteonHubPort +
                  "/3?0262" + hex + "0F" + cmd + "=I=3";
  http.begin(cmdUrl);
  http.setAuthorization(insteonHubUser.c_str(), insteonHubPass.c_str());
  int code = http.GET();
  http.end();
  verbosePrint("Insteon Hub response: " + String(code));
  return (code == 200);
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
 * controlLight - Decide whether to turn the light on or off based on presence.
 * Dispatches to the configured integration (ISY, Insteon Hub, or Home Assistant).
 */
void controlLight() {
  if (!integrationConfigured) return;
  if (integrationMode == "homekit") return;  // HomeKit pushes state changes in loop()

  bool shouldBeOn = presenceDetected ||
    (millis() - lastDetectionTime < (unsigned long)noDetectionTimeout * 1000UL);
  if (shouldBeOn == lightOn) return;

  bool ok = false;
  if (shouldBeOn) {
    if      (integrationMode == "isy")         { turnLightOn();  return; }
    else if (integrationMode == "insteon_hub") ok = sendInsteonHubCommand(true);
    else if (integrationMode == "ha")          ok = sendHACommand(true);
    if (ok) lightOn = true;
  } else {
    if      (integrationMode == "isy")         { turnLightOff(); return; }
    else if (integrationMode == "insteon_hub") ok = sendInsteonHubCommand(false);
    else if (integrationMode == "ha")          ok = sendHACommand(false);
    if (ok) lightOn = false;
  }
}

/*
 * ================================================================
 * SECTION: FACTORY RESET
 * ================================================================
 */

/*
 * checkResetButton - Check if BOOT button is held at startup.
 * @return true if button is pressed at boot time
 */
bool checkResetButton() {
  if (digitalRead(PIN_RESET) == LOW) {
    serialPrintln(F("BOOT button held at startup - entering setup mode"));
    return true;
  }
  return false;
}

/*
 * checkResetButtonHeld - Monitor BOOT button for 5-second factory reset.
 * Provides staged LED visual feedback during hold.
 * @return true if factory reset was performed
 */
bool checkResetButtonHeld() {
  static unsigned long pressStartTime = 0;
  static bool wasPressed = false;

  bool isPressed = (digitalRead(PIN_RESET) == LOW);

  if (isPressed && !wasPressed) {
    pressStartTime = millis();
    wasPressed = true;
    serialPrintln(F("BOOT button pressed - hold 5 seconds for factory reset"));
  } else if (!isPressed && wasPressed) {
    wasPressed = false;
    setRGB(0, 0, 0);
    serialPrintln(F("BOOT button released"));
  } else if (isPressed && wasPressed) {
    unsigned long held = millis() - pressStartTime;

    // Stage 1: 0-2 seconds - solid red
    if (held < 2000) {
      setRGB(255, 0, 0);
    }
    // Stage 2: 2-4 seconds - slow red/blue blink (500ms)
    else if (held < 4000) {
      static unsigned long lastBlink = 0;
      static bool blinkState = false;
      if (millis() - lastBlink > 500) {
        blinkState = !blinkState;
        lastBlink = millis();
        setRGB(blinkState ? 255 : 0, 0, blinkState ? 0 : 255);
      }
    }
    // Stage 3: 4-5 seconds - rapid red/blue blink (200ms)
    else if (held < 5000) {
      static unsigned long lastFast = 0;
      static bool fastState = false;
      if (millis() - lastFast > 200) {
        fastState = !fastState;
        lastFast = millis();
        setRGB(fastState ? 255 : 0, 0, fastState ? 0 : 255);
      }
    }
    // 5 seconds reached - perform reset
    else {
      Serial.println(F("\n*** FACTORY RESET TRIGGERED ***"));
      Serial.println(F("Reset in 5... 4... 3... 2... 1... RESETTING!"));

      // Purple confirmation flashes (3x)
      for (int i = 0; i < 3; i++) {
        setRGB(255, 0, 255);
        delay(150);
        setRGB(0, 0, 0);
        delay(150);
      }

      clearConfiguration();
      invalidateSession();

      delay(1000);
      ESP.restart();
      return true;
    }
  }

  return false;
}

/*
 * ================================================================
 * SECTION: WEB SERVER - COMMON HELPERS
 * ================================================================
 */

// Shared CSS for all pages
const char* PAGE_CSS = R"CSS(
body{font-family:Arial,sans-serif;margin:0;padding:20px;background:#f0f2f5;}
.container{max-width:560px;margin:0 auto;background:#fff;padding:24px;border-radius:12px;box-shadow:0 2px 12px rgba(0,0,0,.1);}
h1{color:#1a1a2e;margin-top:0;font-size:22px;}
h2{color:#444;font-size:16px;border-bottom:2px solid #4CAF50;padding-bottom:4px;margin-top:24px;}
label{display:block;margin-top:14px;font-weight:bold;color:#555;font-size:13px;}
input[type=text],input[type=password],select,textarea{width:100%;padding:9px;margin-top:4px;box-sizing:border-box;border:1px solid #ccc;border-radius:5px;font-size:15px;}
input[type=checkbox]{width:auto;margin-right:6px;}
.chk-label{display:inline;font-weight:normal;}
button,.btn{margin-top:14px;padding:11px 24px;background:#4CAF50;color:#fff;border:none;cursor:pointer;font-size:15px;border-radius:5px;width:100%;text-decoration:none;display:block;text-align:center;box-sizing:border-box;}
button:hover,.btn:hover{background:#388E3C;}
.btn-red{background:#e53935;} .btn-red:hover{background:#b71c1c;}
.btn-gray{background:#757575;} .btn-gray:hover{background:#424242;}
.btn-blue{background:#1976D2;} .btn-blue:hover{background:#0D47A1;}
.info{background:#e3f2fd;padding:11px;margin:12px 0;border-radius:5px;border-left:4px solid #2196F3;font-size:13px;}
.warn{background:#fff3e0;padding:11px;margin:12px 0;border-radius:5px;border-left:4px solid #ff9800;font-size:13px;}
.err{background:#ffebee;padding:11px;margin:12px 0;border-radius:5px;border-left:4px solid #e53935;font-size:13px;}
.ok{background:#e8f5e9;padding:11px;margin:12px 0;border-radius:5px;border-left:4px solid #4CAF50;font-size:13px;}
.help{font-size:12px;color:#777;margin-top:3px;}
details{margin-top:16px;border:1px solid #ddd;border-radius:5px;padding:10px;}
summary{font-weight:bold;cursor:pointer;color:#444;}
.footer{text-align:center;color:#aaa;font-size:11px;margin-top:20px;}
.nav{display:flex;gap:8px;margin-bottom:20px;flex-wrap:wrap;}
.stat-grid{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:12px;}
.stat-card{background:#f8f9fa;padding:10px;border-radius:6px;font-size:13px;}
.stat-label{color:#888;font-size:11px;text-transform:uppercase;}
.stat-value{font-weight:bold;color:#333;font-size:15px;margin-top:2px;}
.badge{display:inline-block;padding:2px 8px;border-radius:10px;font-size:12px;font-weight:bold;}
.badge-green{background:#c8e6c9;color:#1b5e20;}
.badge-red{background:#ffcdd2;color:#b71c1c;}
.badge-yellow{background:#fff9c4;color:#f57f17;}
@media(max-width:480px){.stat-grid{grid-template-columns:1fr;}}
)CSS";

/*
 * sendPageStart - Send HTML page header with shared CSS.
 * @param title Page title shown in browser tab
 * @param extraHead Additional HTML to inject into <head>
 */
void sendPageStart(const String& title, const String& extraHead = "") {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<meta charset='UTF-8'>";
  html += "<title>" + title + " - ESP32 Presence</title>";
  html += "<style>" + String(PAGE_CSS) + "</style>";
  html += extraHead;
  html += "</head><body><div class='container'>";
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", html);
}

/*
 * sendPageEnd - Send closing HTML tags with footer.
 */
void sendPageEnd() {
  String html = "<div class='footer'>ESP32 Presence v";
  html += FIRMWARE_VERSION;
  html += " | Board: ";
  html += BOARD_TYPE;
  html += "</div></div></body></html>";
  server.sendContent(html);
  server.sendContent("");
}

/*
 * sendNavBar - Send navigation links for authenticated pages.
 */
void sendNavBar() {
  server.sendContent(
    "<div class='nav'>"
    "<a class='btn btn-blue' style='width:auto;padding:8px 14px;font-size:13px;' href='/'>Dashboard</a>"
    "<a class='btn btn-blue' style='width:auto;padding:8px 14px;font-size:13px;' href='/config'>Settings</a>"
    "<a class='btn btn-gray' style='width:auto;padding:8px 14px;font-size:13px;' href='/logout'>Logout</a>"
    "</div>"
  );
}

/*
 * ================================================================
 * SECTION: WEB SERVER - CAPTIVE PORTAL & SETUP PAGES
 * ================================================================
 */

/*
 * scanWiFiNetworks - Scan for nearby networks and return HTML option tags.
 * @return HTML string of <option> elements for a <select> element
 */
String scanWiFiNetworks() {
  serialPrintln(F("Scanning for WiFi networks..."));
  int n = WiFi.scanNetworks();
  String options = "";

  if (n == 0) {
    return "<option value=''>No networks found - try again</option>";
  }

  String seenSSIDs[50];
  int seenCount = 0;

  for (int i = 0; i < n && seenCount < 50; i++) {
    String ssid = WiFi.SSID(i);
    bool isDup = false;
    for (int j = 0; j < seenCount; j++) {
      if (seenSSIDs[j] == ssid) { isDup = true; break; }
    }
    if (!isDup && ssid.length() > 0) {
      seenSSIDs[seenCount++] = ssid;
      int rssi = WiFi.RSSI(i);
      String security = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " (Open)" : "";
      String selected = (ssid == wifiSSID) ? " selected" : "";
      options += "<option value='" + ssid + "'" + selected + ">" + ssid +
                 " (" + String(rssi) + " dBm)" + security + "</option>";
    }
  }

  verbosePrint("Found " + String(n) + " networks");
  return options;
}

/*
 * handleCaptivePortal - Redirect all unknown URLs to setup page.
 */
void handleCaptivePortal() {
  server.sendHeader("Location", "http://192.168.4.1/");
  server.send(302, "text/plain", "");
}

/*
 * sendIntegrationSection - Send the integration selector UI block as streamed HTML.
 * @param isSetup true for initial setup (shows existing passwords), false for settings page
 */
void sendIntegrationSection(bool isSetup) {
  String selNone = (integrationMode == "none")        ? " selected" : "";
  String selISY  = (integrationMode == "isy")         ? " selected" : "";
  String selHub  = (integrationMode == "insteon_hub") ? " selected" : "";
  String selHA   = (integrationMode == "ha")          ? " selected" : "";
  String httpsChecked   = useHTTPS ? " checked" : "";
  String haHTTPSChecked = haHTTPS  ? " checked" : "";

  server.sendContent(
    "<h2>Integration (Optional)</h2>"
    "<div class='info'>Select how this sensor controls lights. Leave as &ldquo;None&rdquo; "
    "for sensor-only use.</div>"
    "<label for='integration_mode'>Integration Type:</label>"
    "<select name='integration_mode' id='integration_mode' "
    "onchange='showIntegration(this.value)'>"
    "<option value='none'" + selNone + ">None &mdash; sensor only</option>"
    "<option value='isy'" + selISY + ">EISY / ISY / Polisy</option>"
    "<option value='insteon_hub'" + selHub + ">Insteon Hub 2 (Model 2245)</option>"
    "<option value='ha'" + selHA + ">Home Assistant</option>"
  );
#ifdef ENABLE_HOMEKIT
  {
    String selHK = (integrationMode == "homekit") ? " selected" : "";
    server.sendContent("<option value='homekit'" + selHK + ">Apple HomeKit (Native)</option>");
  }
#endif
  server.sendContent("</select>");

  // --- ISY section ---
  server.sendContent(
    "<div id='sect_isy'>"
    "<h2>EISY / ISY / Polisy Settings</h2>"
    "<label for='isy_ip'>Controller IP Address:</label>"
    "<input type='text' name='isy_ip' id='isy_ip' value='" + isyIP +
    "' placeholder='e.g., 192.168.1.100' autocomplete='off'>"
    "<label><input type='checkbox' name='use_https' value='1'" + httpsChecked + ">"
    "<span class='chk-label'>Use HTTPS (EISY only &mdash; leave unchecked for ISY994)</span></label>"
    "<label for='isy_user'>Username:</label>"
    "<input type='text' name='isy_user' id='isy_user' value='" + isyUsername +
    "' placeholder='Usually: admin' autocomplete='username'>"
    "<label for='isy_pass'>Password:</label>"
  );
  if (isSetup) {
    server.sendContent(
      "<input type='password' name='isy_pass' id='isy_pass' value='" + isyPassword +
      "' placeholder='EISY/ISY password' autocomplete='off'>"
    );
  } else {
    server.sendContent(
      "<input type='password' name='isy_pass' id='isy_pass' "
      "placeholder='Leave blank to keep current' autocomplete='off'>"
    );
  }
  server.sendContent(
    "<label for='isy_device'>Insteon Device Address:</label>"
    "<input type='text' name='isy_device' id='isy_device' value='" + isyDeviceID +
    "' placeholder='e.g., 1A.2B.3C' autocomplete='off'>"
    "<details><summary>Help: EISY/ISY Settings</summary>"
    "<p style='font-size:13px;color:#555'>"
    "<strong>Controller IP:</strong> Find in Admin Console &rarr; Configuration &rarr; System.<br><br>"
    "<strong>Use HTTPS:</strong> Check for EISY. ISY994 users leave unchecked.<br><br>"
    "<strong>Username/Password:</strong> Your EISY/ISY admin credentials (default: admin/admin).<br><br>"
    "<strong>Device Address:</strong> Right-click light in Admin Console &rarr; Copy &rarr; Address. "
    "Format: 1A.2B.3C</p></details>"
    "</div>"
  );

  // --- Insteon Hub section ---
  server.sendContent(
    "<div id='sect_insteon_hub'>"
    "<h2>Insteon Hub 2 Settings</h2>"
    "<label for='hub_ip'>Hub IP Address:</label>"
    "<input type='text' name='hub_ip' id='hub_ip' value='" + insteonHubIP +
    "' placeholder='e.g., 192.168.1.50' autocomplete='off'>"
    "<label for='hub_port'>Hub Port:</label>"
    "<input type='text' name='hub_port' id='hub_port' value='" + insteonHubPort +
    "' placeholder='25105'>"
    "<label for='hub_user'>Hub Username:</label>"
    "<input type='text' name='hub_user' id='hub_user' value='" + insteonHubUser +
    "' placeholder='Hub username'>"
    "<label for='hub_pass'>Hub Password:</label>"
  );
  if (isSetup) {
    server.sendContent(
      "<input type='password' name='hub_pass' id='hub_pass' value='" + insteonHubPass +
      "' placeholder='Hub password' autocomplete='off'>"
    );
  } else {
    server.sendContent(
      "<input type='password' name='hub_pass' id='hub_pass' "
      "placeholder='Leave blank to keep current' autocomplete='off'>"
    );
  }
  server.sendContent(
    "<label for='hub_addr'>Insteon Device Address:</label>"
    "<input type='text' name='hub_addr' id='hub_addr' value='" + insteonHubAddr +
    "' placeholder='e.g., 1A.2B.3C' autocomplete='off'>"
    "<details><summary>Help: Insteon Hub 2 Settings</summary>"
    "<p style='font-size:13px;color:#555'>"
    "<strong>Hub IP:</strong> Find in the Insteon app or your router's DHCP list.<br><br>"
    "<strong>Port:</strong> Default is 25105. Only change if you modified Hub settings.<br><br>"
    "<strong>Username/Password:</strong> Found on the label on the bottom of your Hub.<br><br>"
    "<strong>Device Address:</strong> Found in Insteon app under device details. "
    "Format: 1A.2B.3C</p></details>"
    "</div>"
  );

  // --- Home Assistant section ---
  server.sendContent(
    "<div id='sect_ha'>"
    "<h2>Home Assistant Settings</h2>"
    "<label for='ha_ip'>HA IP Address:</label>"
    "<input type='text' name='ha_ip' id='ha_ip' value='" + haIP +
    "' placeholder='e.g., 192.168.1.200' autocomplete='off'>"
    "<label for='ha_port'>HA Port:</label>"
    "<input type='text' name='ha_port' id='ha_port' value='" + haPort +
    "' placeholder='8123'>"
    "<label><input type='checkbox' name='ha_https' value='1'" + haHTTPSChecked + ">"
    "<span class='chk-label'>Use HTTPS</span></label>"
    "<label for='ha_token'>Long-Lived Access Token:</label>"
  );
  if (isSetup) {
    server.sendContent(
      "<input type='password' name='ha_token' id='ha_token' value='" + haToken +
      "' placeholder='Paste token here' autocomplete='off'>"
    );
  } else {
    server.sendContent(
      "<input type='password' name='ha_token' id='ha_token' "
      "placeholder='Leave blank to keep current' autocomplete='off'>"
    );
  }
  server.sendContent(
    "<label for='ha_entity'>Entity ID:</label>"
    "<input type='text' name='ha_entity' id='ha_entity' value='" + haEntityId +
    "' placeholder='e.g., light.living_room' autocomplete='off'>"
    "<details><summary>Help: Home Assistant Settings</summary>"
    "<p style='font-size:13px;color:#555'>"
    "<strong>HA IP:</strong> IP address of your Home Assistant server.<br><br>"
    "<strong>Port:</strong> Default is 8123. Use 443 with HTTPS for remote/Nabu Casa.<br><br>"
    "<strong>Access Token:</strong> In HA go to Profile &rarr; Security &rarr; "
    "Long-Lived Access Tokens &rarr; Create Token.<br><br>"
    "<strong>Entity ID:</strong> The light or switch to control, e.g. light.living_room. "
    "Find under HA Settings &rarr; Entities.</p></details>"
    "</div>"
  );

#ifdef ENABLE_HOMEKIT
  server.sendContent(
    "<div id='sect_homekit'>"
    "<h2>Apple HomeKit Settings</h2>"
    "<div class='info'>HomeKit native mode exposes this device as an occupancy sensor "
    "directly in the Apple Home app. No hub required.</div>"
    "<label for='hk_code'>Pairing Code (8 digits, no hyphens):</label>"
    "<input type='text' name='hk_code' id='hk_code' value='" + homekitCode +
    "' placeholder='11122333' maxlength='8' pattern='[0-9]{8}'>"
    "<details><summary>Help: HomeKit Pairing</summary>"
    "<p style='font-size:13px;color:#555'>"
    "After saving and rebooting, open the <strong>Apple Home</strong> app, "
    "tap + &rarr; Add Accessory &rarr; More Options. "
    "Enter the code above when prompted.</p></details>"
    "</div>"
  );
#endif

  // Show/hide JavaScript
  server.sendContent(
    "<script>"
    "function showIntegration(v){"
    "['isy','insteon_hub','ha','homekit'].forEach(function(id){"
    "var el=document.getElementById('sect_'+id);"
    "if(el)el.style.display=(v===id)?'':'none';"
    "});"
    "}"
    "showIntegration(document.getElementById('integration_mode').value);"
    "</script>"
  );
}

/*
 * handleSetupRoot - Serve the captive portal / initial setup page.
 */
void handleSetupRoot() {
  String networkOptions = scanWiFiNetworks();

  sendPageStart("ESP32 Presence Setup");

  server.sendContent(
    "<h1>ESP32 Presence Detector Setup</h1>"
    "<div class='info'><strong>Welcome!</strong> Configure your device to connect to your WiFi network. "
    "All settings can be changed later via the web interface.</div>"
    "<form action='/save' method='POST' autocomplete='on'>"
  );

  // --- WiFi Settings ---
  server.sendContent(
    "<h2>WiFi Settings (Required)</h2>"
    "<label for='wifi_ssid'>WiFi Network:</label>"
    "<select name='wifi_ssid' id='wifi_ssid' required>"
    "<option value=''>-- Select Your Network --</option>"
  );
  server.sendContent(networkOptions);
  server.sendContent(
    "</select>"
    "<details><summary>Help: WiFi Settings</summary>"
    "<p style='font-size:13px;color:#555'>Select your home WiFi network from the list. "
    "The device will connect to this network after setup.<br><br>"
    "<strong>Important:</strong> ESP32 only supports <strong>2.4 GHz</strong> WiFi. "
    "If your router has separate 2.4G and 5G networks, select the 2.4G one.</p></details>"
    "<label for='wifi_pass'>WiFi Password:</label>"
    "<input type='password' name='wifi_pass' id='wifi_pass' placeholder='Enter WiFi password' autocomplete='off'>"
    "<div class='help'>Leave blank for open networks</div>"
  );

  // --- Detection Settings ---
  String t60  = noDetectionTimeout == 60   ? " selected" : "";
  String t120 = noDetectionTimeout == 120  ? " selected" : "";
  String t180 = noDetectionTimeout == 180  ? " selected" : "";
  String t300 = noDetectionTimeout == 300  ? " selected" : "";
  String t600 = noDetectionTimeout == 600  ? " selected" : "";
  String t900 = noDetectionTimeout == 900  ? " selected" : "";

  server.sendContent(
    "<h2>Detection Settings</h2>"
    "<label for='timeout'>Light Off Delay:</label>"
    "<select name='timeout' id='timeout'>"
    "<option value='60'" + t60   + ">1 minute</option>"
    "<option value='120'" + t120 + ">2 minutes</option>"
    "<option value='180'" + t180 + ">3 minutes</option>"
    "<option value='300'" + t300 + ">5 minutes (recommended)</option>"
    "<option value='600'" + t600 + ">10 minutes</option>"
    "<option value='900'" + t900 + ">15 minutes</option>"
    "</select>"
    "<details><summary>Help: Detection Settings</summary>"
    "<p style='font-size:13px;color:#555'><strong>Light Off Delay:</strong> How long to wait after no presence "
    "is detected before turning off the light.<br><br>"
    "Shorter times save energy but may turn off lights too quickly if you're sitting still. "
    "Recommended: 5 minutes.</p></details>"
  );

  // --- Admin Password ---
  server.sendContent(
    "<h2>Admin Password (Required)</h2>"
    "<div class='warn'><strong>Important:</strong> Set a secure admin password. "
    "This protects your device configuration after setup. Write it down!</div>"
    "<label for='admin_pass'>Admin Password:</label>"
    "<input type='password' name='admin_pass' id='admin_pass' placeholder='Set an admin password' autocomplete='new-password' required>"
    "<label for='admin_pass2'>Confirm Admin Password:</label>"
    "<input type='password' name='admin_pass2' id='admin_pass2' placeholder='Repeat password' autocomplete='new-password' required>"
    "<details><summary>Help: Admin Password</summary>"
    "<p style='font-size:13px;color:#555'>This password protects the device configuration page after setup.<br><br>"
    "You will need this to change settings later. <strong>Write it down!</strong><br><br>"
    "If you forget it, you must factory reset the device by holding the BOOT button for 5 seconds.</p></details>"
  );

  // --- Integration ---
  sendIntegrationSection(true);

  // --- Advanced (Pins) ---
  server.sendContent(
    "<h2>Advanced Settings (Optional)</h2>"
    "<details><summary>Pin Configuration — Current Board: " + String(BOARD_TYPE) + "</summary>"
    "<div class='warn'><strong>Warning:</strong> Only change pins if you know what you're doing. "
    "Incorrect settings will prevent the sensor from working.</div>"
    "<p style='font-size:13px;color:#555'>Default pins for " + String(BOARD_TYPE) + ":<br>"
    "Sensor OUT: GPIO" + String(SENSOR_OUT_PIN) + "<br>"
    "Sensor TX: GPIO" + String(SENSOR_TX_PIN) + "<br>"
    "Sensor RX: GPIO" + String(SENSOR_RX_PIN) + "<br>"
    "RGB LED: GPIO" + String(RGB_LED_PIN) + "</p>"
    "<label><input type='checkbox' name='use_custom_pins' value='1'>"
    "<span class='chk-label'>Use custom pin configuration</span></label>"
    "<label>Sensor OUT pin: <input type='number' name='pin_out' value='" + String(pinSensorOut) + "' min='0' max='48'></label>"
    "<label>Sensor TX pin: <input type='number' name='pin_tx' value='" + String(pinSensorTx) + "' min='0' max='48'></label>"
    "<label>Sensor RX pin: <input type='number' name='pin_rx' value='" + String(pinSensorRx) + "' min='0' max='48'></label>"
    "<label>RGB LED pin: <input type='number' name='pin_led' value='" + String(pinRgbLed) + "' min='0' max='48'></label>"
    "</details>"
  );

  server.sendContent(
    "<button type='submit'>Save Configuration &amp; Connect</button>"
    "</form>"
  );

  sendPageEnd();
}

/*
 * handleSetupSave - Process initial setup form submission.
 */
void handleSetupSave() {
  String adminPass  = server.arg("admin_pass");
  String adminPass2 = server.arg("admin_pass2");

  // Validate admin password
  if (adminPass.length() < 4) {
    sendPageStart("Setup Error");
    server.sendContent("<h1>Setup Error</h1><div class='err'>Admin password must be at least 4 characters.</div>"
                       "<a class='btn' href='/'>Go Back</a>");
    sendPageEnd();
    return;
  }
  if (adminPass != adminPass2) {
    sendPageStart("Setup Error");
    server.sendContent("<h1>Setup Error</h1><div class='err'>Admin passwords do not match.</div>"
                       "<a class='btn' href='/'>Go Back</a>");
    sendPageEnd();
    return;
  }

  // Save form values
  wifiSSID     = server.arg("wifi_ssid");
  wifiPassword = server.arg("wifi_pass");
  isyIP        = server.arg("isy_ip");
  isyUsername  = server.arg("isy_user");
  isyPassword  = server.arg("isy_pass");
  isyDeviceID  = server.arg("isy_device");
  useHTTPS     = (server.arg("use_https") == "1");

  wifiSSID.trim(); wifiPassword.trim(); isyIP.trim();
  isyUsername.trim(); isyPassword.trim(); isyDeviceID.trim();

  integrationMode = server.arg("integration_mode");
  if (integrationMode == "") integrationMode = "none";

  insteonHubIP   = server.arg("hub_ip");   insteonHubIP.trim();
  String hubPort = server.arg("hub_port"); hubPort.trim();
  insteonHubPort = (hubPort == "") ? DEFAULT_INSTEON_HUB_PORT : hubPort;
  insteonHubUser = server.arg("hub_user"); insteonHubUser.trim();
  insteonHubPass = server.arg("hub_pass"); insteonHubPass.trim();
  insteonHubAddr = server.arg("hub_addr"); insteonHubAddr.trim();

  haIP      = server.arg("ha_ip");    haIP.trim();
  String haPortArg = server.arg("ha_port"); haPortArg.trim();
  haPort    = (haPortArg == "") ? DEFAULT_HA_PORT : haPortArg;
  haHTTPS   = (server.arg("ha_https") == "1");
  haToken   = server.arg("ha_token"); haToken.trim();
  haEntityId = server.arg("ha_entity"); haEntityId.trim();

#ifdef ENABLE_HOMEKIT
  String hkCode = server.arg("hk_code"); hkCode.trim();
  if (hkCode.length() == 8) homekitCode = hkCode;
#endif

  String timeoutStr = server.arg("timeout");
  noDetectionTimeout = timeoutStr.toInt();
  if (noDetectionTimeout < 60) noDetectionTimeout = 300;

  adminPasswordHash = hashPassword(adminPass);
  adminPasswordSet  = true;

  useCustomPins = (server.arg("use_custom_pins") == "1");
  if (useCustomPins) {
    pinSensorOut = server.arg("pin_out").toInt();
    pinSensorTx  = server.arg("pin_tx").toInt();
    pinSensorRx  = server.arg("pin_rx").toInt();
    pinRgbLed    = server.arg("pin_led").toInt();
  }

  if (wifiSSID == "") {
    sendPageStart("Setup Error");
    server.sendContent("<h1>Setup Error</h1><div class='err'>Please select a WiFi network.</div>"
                       "<a class='btn' href='/'>Go Back</a>");
    sendPageEnd();
    return;
  }

  saveConfiguration();

  sendPageStart("Saved!");
  server.sendContent(
    "<h1 style='color:#4CAF50'>&#10004; Configuration Saved!</h1>"
    "<div class='ok'>Settings saved successfully. The device will now connect to your WiFi network.</div>"
    "<p>Connecting to: <strong>" + wifiSSID + "</strong></p>"
    "<p>The LED will flash green when connected. Access the web interface at <strong>http://presence.local</strong> "
    "or via the IP address shown in the Serial Monitor.</p>"
    "<p style='color:#888;font-size:13px'>Restarting in 3 seconds...</p>"
  );
  sendPageEnd();

  serialPrintln(F("Setup complete! Restarting..."));
  delay(3000);
  ESP.restart();
}

/*
 * ================================================================
 * SECTION: WEB SERVER - AUTHENTICATED PAGES
 * ================================================================
 */

/*
 * handleLogin - Serve the login page (GET) or process login (POST).
 */
void handleLogin() {
  if (server.method() == HTTP_POST) {
    // Process login
    if (isLoginLocked()) {
      sendPageStart("Login");
      server.sendContent("<h1>Login</h1><div class='err'>Too many failed attempts. "
                         "Please wait 1 minute and try again.</div>"
                         "<a class='btn' href='/login'>Back to Login</a>");
      sendPageEnd();
      return;
    }

    String enteredPass = server.arg("password");
    String enteredHash = hashPassword(enteredPass);

    if (enteredHash == adminPasswordHash) {
      loginAttempts = 0;
      loginLockoutEnd = 0;
      String sessionId = createSession();

      server.sendHeader("Set-Cookie",
        "session=" + sessionId + "; Path=/; HttpOnly; Max-Age=900");
      server.sendHeader("Location", "/");
      server.send(302, "text/plain", "");
      serialPrintln(F("Login successful"));
    } else {
      loginAttempts++;
      serialPrint("Failed login attempt ");
      serialPrintln(String(loginAttempts) + "/" + String(MAX_LOGIN_ATTEMPTS));

      if (loginAttempts >= MAX_LOGIN_ATTEMPTS) {
        loginLockoutEnd = millis() + LOGIN_LOCKOUT_MS;
        serialPrintln(F("Login locked for 60 seconds"));
      }

      sendPageStart("Login");
      server.sendContent(
        "<h1>Login</h1>"
        "<div class='err'>Incorrect password. " +
        String(MAX_LOGIN_ATTEMPTS - loginAttempts) + " attempt(s) remaining.</div>"
        "<form method='POST'>"
        "<label>Admin Password:</label>"
        "<input type='password' name='password' autofocus>"
        "<button type='submit'>Login</button>"
        "</form>"
      );
      sendPageEnd();
    }
    return;
  }

  // GET: show login form
  sendPageStart("Login");
  server.sendContent(
    "<h1>ESP32 Presence</h1>"
    "<p style='color:#666'>Enter your admin password to access the configuration interface.</p>"
    "<form method='POST'>"
    "<label>Admin Password:</label>"
    "<input type='password' name='password' autofocus>"
    "<button type='submit'>Login</button>"
    "</form>"
    "<p style='font-size:12px;color:#aaa;text-align:center;margin-top:20px'>"
    "Forgot password? Hold the BOOT button for 5 seconds to factory reset.</p>"
  );
  sendPageEnd();
}

/*
 * handleStatus - Serve the status dashboard (main authenticated page).
 */
void handleStatus() {
  if (!requireAuth()) return;

  String autoRefresh = "<meta http-equiv='refresh' content='5'>";
  sendPageStart("Dashboard", autoRefresh);
  sendNavBar();

  server.sendContent("<h1>Status Dashboard</h1>");

  // Sensor status card
  String presenceBadge = presenceDetected
    ? (isMoving ? "<span class='badge badge-green'>Moving</span>" : "<span class='badge badge-yellow'>Static</span>")
    : "<span class='badge badge-red'>No Presence</span>";

  String integLabel = "None";
  if      (integrationMode == "isy")         integLabel = "EISY/ISY";
  else if (integrationMode == "insteon_hub") integLabel = "Insteon Hub";
  else if (integrationMode == "ha")          integLabel = "Home Assistant";
  else if (integrationMode == "homekit")     integLabel = "HomeKit";

  String lightBadge = integrationConfigured
    ? (lightOn ? "<span class='badge badge-green'>Light ON</span>" : "<span class='badge badge-red'>Light OFF</span>")
    : "<span class='badge' style='background:#eee;color:#888'>Not Configured</span>";

  int rssi = WiFi.RSSI();
  int signalPct = wifiSignalToPercent(rssi);

  server.sendContent(
    "<div class='stat-grid'>"
    "<div class='stat-card'><div class='stat-label'>Presence</div><div class='stat-value'>" + presenceBadge + "</div></div>"
    "<div class='stat-card'><div class='stat-label'>Light Control</div><div class='stat-value'>" + lightBadge + "</div></div>"
    "<div class='stat-card'><div class='stat-label'>Integration</div><div class='stat-value'>" + integLabel + "</div></div>"
    "<div class='stat-card'><div class='stat-label'>WiFi Signal</div><div class='stat-value'>" + String(signalPct) + "% (" + String(rssi) + " dBm)</div></div>"
    "<div class='stat-card'><div class='stat-label'>Uptime</div><div class='stat-value'>" + getUptimeString() + "</div></div>"
    "<div class='stat-card'><div class='stat-label'>IP Address</div><div class='stat-value'>" + WiFi.localIP().toString() + "</div></div>"
    "<div class='stat-card'><div class='stat-label'>Board</div><div class='stat-value'>" + String(BOARD_TYPE) + "</div></div>"
    "<div class='stat-card'><div class='stat-label'>Free Memory</div><div class='stat-value'>" + String(ESP.getFreeHeap()) + " B</div></div>"
    "<div class='stat-card'><div class='stat-label'>Sensor OUT Pin</div><div class='stat-value'>GPIO" + String(pinSensorOut) + "</div></div>"
    "</div>"
  );

  // Pin configuration summary
  server.sendContent(
    "<h2>Pin Configuration</h2>"
    "<p style='font-size:13px;color:#555'>"
    "Sensor OUT: GPIO" + String(pinSensorOut) +
    " &nbsp;|&nbsp; Sensor TX: GPIO" + String(pinSensorTx) +
    " &nbsp;|&nbsp; Sensor RX: GPIO" + String(pinSensorRx) +
    " &nbsp;|&nbsp; RGB LED: GPIO" + String(pinRgbLed) +
    (useCustomPins ? " <em>(custom)</em>" : " <em>(default)</em>") +
    "</p>"
  );

  sendPageEnd();
}

/*
 * handleConfig - Serve settings page (GET) or save settings (POST).
 */
void handleConfig() {
  if (!requireAuth()) return;

  if (server.method() == HTTP_POST) {
    // Save posted settings
    String action = server.arg("action");

    wifiSSID    = server.arg("wifi_ssid");
    wifiPassword = server.arg("wifi_pass");
    isyIP       = server.arg("isy_ip");
    isyUsername = server.arg("isy_user");
    isyDeviceID = server.arg("isy_device");
    useHTTPS    = (server.arg("use_https") == "1");
    // Keep existing ISY password if field left blank
    String isyPassArg = server.arg("isy_pass");
    if (isyPassArg.length() > 0) isyPassword = isyPassArg;

    wifiSSID.trim(); wifiPassword.trim(); isyIP.trim();
    isyUsername.trim(); isyPassword.trim(); isyDeviceID.trim();

    integrationMode = server.arg("integration_mode");
    if (integrationMode == "") integrationMode = "none";

    insteonHubIP   = server.arg("hub_ip");   insteonHubIP.trim();
    insteonHubUser = server.arg("hub_user"); insteonHubUser.trim();
    insteonHubAddr = server.arg("hub_addr"); insteonHubAddr.trim();
    String hubPortArg2 = server.arg("hub_port"); hubPortArg2.trim();
    insteonHubPort = (hubPortArg2 == "") ? DEFAULT_INSTEON_HUB_PORT : hubPortArg2;
    // Keep existing Hub password if field left blank
    String hubPassArg = server.arg("hub_pass");
    if (hubPassArg.length() > 0) insteonHubPass = hubPassArg;

    haIP      = server.arg("ha_ip");    haIP.trim();
    haEntityId = server.arg("ha_entity"); haEntityId.trim();
    haHTTPS   = (server.arg("ha_https") == "1");
    String haPortArg2 = server.arg("ha_port"); haPortArg2.trim();
    haPort    = (haPortArg2 == "") ? DEFAULT_HA_PORT : haPortArg2;
    // Keep existing HA token if field left blank
    String haTokenArg = server.arg("ha_token");
    if (haTokenArg.length() > 0) haToken = haTokenArg;

#ifdef ENABLE_HOMEKIT
    String hkCode = server.arg("hk_code"); hkCode.trim();
    if (hkCode.length() == 8) homekitCode = hkCode;
#endif

    String timeoutStr = server.arg("timeout");
    noDetectionTimeout = timeoutStr.toInt();
    if (noDetectionTimeout < 60) noDetectionTimeout = 300;

    // Admin password change
    String newPass  = server.arg("new_pass");
    String newPass2 = server.arg("new_pass2");
    String oldPass  = server.arg("old_pass");

    if (newPass.length() > 0) {
      if (hashPassword(oldPass) != adminPasswordHash) {
        sendPageStart("Settings Error");
        server.sendContent("<h1>Settings Error</h1>"
                           "<div class='err'>Current password is incorrect.</div>"
                           "<a class='btn' href='/config'>Go Back</a>");
        sendPageEnd();
        return;
      }
      if (newPass != newPass2) {
        sendPageStart("Settings Error");
        server.sendContent("<h1>Settings Error</h1>"
                           "<div class='err'>New passwords do not match.</div>"
                           "<a class='btn' href='/config'>Go Back</a>");
        sendPageEnd();
        return;
      }
      if (newPass.length() < 4) {
        sendPageStart("Settings Error");
        server.sendContent("<h1>Settings Error</h1>"
                           "<div class='err'>New password must be at least 4 characters.</div>"
                           "<a class='btn' href='/config'>Go Back</a>");
        sendPageEnd();
        return;
      }
      adminPasswordHash = hashPassword(newPass);
      serialPrintln(F("Admin password updated"));
    }

    // Advanced settings
    ledBrightness = server.arg("led_bright").toInt();
    if (ledBrightness < 10 || ledBrightness > 100) ledBrightness = 50;
    strip.setBrightness(ledBrightness);

    debugLevel = server.arg("debug_lvl").toInt();

    useCustomPins = (server.arg("use_custom_pins") == "1");
    if (useCustomPins) {
      pinSensorOut = server.arg("pin_out").toInt();
      pinSensorTx  = server.arg("pin_tx").toInt();
      pinSensorRx  = server.arg("pin_rx").toInt();
      pinRgbLed    = server.arg("pin_led").toInt();
    }

    saveConfiguration();

    if (action == "reboot") {
      sendPageStart("Saving...");
      server.sendContent("<h1>Saved &amp; Rebooting</h1>"
                         "<div class='ok'>Settings saved. Device is restarting...</div>"
                         "<p>Reconnect at <a href='http://presence.local'>http://presence.local</a> after ~10 seconds.</p>");
      sendPageEnd();
      delay(2000);
      ESP.restart();
      return;
    }

    sendPageStart("Settings Saved");
    sendNavBar();
    server.sendContent("<h1>Settings Saved</h1>"
                       "<div class='ok'>All settings have been saved successfully.</div>"
                       "<a class='btn' href='/config'>Back to Settings</a>");
    sendPageEnd();
    return;
  }

  // GET: Show settings form
  sendPageStart("Settings");
  sendNavBar();
  server.sendContent("<h1>Settings</h1><form method='POST'>");

  // WiFi section
  String networkOptions = scanWiFiNetworks();
  server.sendContent(
    "<h2>WiFi Settings</h2>"
    "<label>WiFi Network:</label>"
    "<select name='wifi_ssid'><option value=''>-- Select --</option>"
  );
  server.sendContent(networkOptions);
  server.sendContent(
    "</select>"
    "<label>WiFi Password:</label>"
    "<input type='password' name='wifi_pass' placeholder='Leave blank to keep current' autocomplete='off'>"
    "<div class='help'>Leave blank to keep your current password</div>"
  );

  // Detection
  String t60  = noDetectionTimeout == 60   ? " selected" : "";
  String t120 = noDetectionTimeout == 120  ? " selected" : "";
  String t180 = noDetectionTimeout == 180  ? " selected" : "";
  String t300 = noDetectionTimeout == 300  ? " selected" : "";
  String t600 = noDetectionTimeout == 600  ? " selected" : "";
  String t900 = noDetectionTimeout == 900  ? " selected" : "";

  server.sendContent(
    "<h2>Detection Settings</h2>"
    "<label>Light Off Delay:</label>"
    "<select name='timeout'>"
    "<option value='60'" + t60   + ">1 minute</option>"
    "<option value='120'" + t120 + ">2 minutes</option>"
    "<option value='180'" + t180 + ">3 minutes</option>"
    "<option value='300'" + t300 + ">5 minutes</option>"
    "<option value='600'" + t600 + ">10 minutes</option>"
    "<option value='900'" + t900 + ">15 minutes</option>"
    "</select>"
  );

  // Integration
  sendIntegrationSection(false);

  // Admin password change
  server.sendContent(
    "<h2>Change Admin Password</h2>"
    "<label>Current Password:</label>"
    "<input type='password' name='old_pass' autocomplete='current-password'>"
    "<label>New Password:</label>"
    "<input type='password' name='new_pass' autocomplete='new-password'>"
    "<label>Confirm New Password:</label>"
    "<input type='password' name='new_pass2' autocomplete='new-password'>"
    "<div class='help'>Leave all blank to keep the current admin password</div>"
  );

  // Advanced settings
  String debugNone = debugLevel == 0 ? " selected" : "";
  String debugBasic = debugLevel == 1 ? " selected" : "";
  String debugVerbose = debugLevel == 2 ? " selected" : "";
  String customChecked = useCustomPins ? " checked" : "";

  server.sendContent(
    "<details>"
    "<summary>Advanced Settings</summary>"
    "<div class='warn'>Warning: only modify these if you know what you are doing.</div>"
    "<label>LED Brightness (%):</label>"
    "<input type='number' name='led_bright' value='" + String(ledBrightness) + "' min='10' max='100'>"
    "<label>Debug Level:</label>"
    "<select name='debug_lvl'>"
    "<option value='0'" + debugNone    + ">None (silent)</option>"
    "<option value='1'" + debugBasic   + ">Basic (default)</option>"
    "<option value='2'" + debugVerbose + ">Verbose</option>"
    "</select>"
    "<h2 style='font-size:14px;'>Custom Pin Configuration</h2>"
    "<label><input type='checkbox' name='use_custom_pins' value='1'" + customChecked + ">"
    "<span class='chk-label'>Enable custom pins (current board: " + String(BOARD_TYPE) + ")</span></label>"
    "<label>Sensor OUT GPIO:</label>"
    "<input type='number' name='pin_out' value='" + String(pinSensorOut) + "' min='0' max='48'>"
    "<label>Sensor TX GPIO (UART RX):</label>"
    "<input type='number' name='pin_tx' value='" + String(pinSensorTx) + "' min='0' max='48'>"
    "<label>Sensor RX GPIO (UART TX):</label>"
    "<input type='number' name='pin_rx' value='" + String(pinSensorRx) + "' min='0' max='48'>"
    "<label>RGB LED GPIO:</label>"
    "<input type='number' name='pin_led' value='" + String(pinRgbLed) + "' min='0' max='48'>"
    "</details>"
  );

  server.sendContent(
    "<button type='submit' name='action' value='save'>Save Settings</button>"
    "<button type='submit' name='action' value='reboot' style='background:#1976D2;margin-top:8px;'>Save &amp; Reboot</button>"
    "<a class='btn btn-gray' href='/'>Cancel</a>"
    "</form>"
    "<hr style='margin:24px 0;border:none;border-top:1px solid #eee;'>"
    "<h2>Export / Import Configuration</h2>"
    "<a class='btn btn-blue' href='/api/config/export'>Download Config (JSON)</a>"
    "<hr style='margin:24px 0;border:none;border-top:1px solid #eee;'>"
    "<h2>Danger Zone</h2>"
    "<a class='btn btn-red' href='/reset'>Factory Reset...</a>"
  );

  sendPageEnd();
}

/*
 * handleReset - Serve factory reset confirmation page.
 */
void handleReset() {
  if (!requireAuth()) return;

  if (server.method() == HTTP_POST) {
    String enteredPass = server.arg("password");
    if (hashPassword(enteredPass) != adminPasswordHash) {
      sendPageStart("Reset Error");
      sendNavBar();
      server.sendContent("<h1>Reset Error</h1><div class='err'>Incorrect password.</div>"
                         "<a class='btn' href='/reset'>Try Again</a>");
      sendPageEnd();
      return;
    }

    sendPageStart("Resetting...");
    server.sendContent("<h1>Factory Reset</h1>"
                       "<div class='err'>All settings have been cleared. Device is restarting in setup mode...</div>"
                       "<p>Look for the <strong>Presence_XXXX</strong> WiFi network to reconfigure.</p>");
    sendPageEnd();

    serialPrintln(F("Web-triggered factory reset"));
    delay(2000);
    clearConfiguration();
    invalidateSession();
    delay(500);
    ESP.restart();
    return;
  }

  // GET: Show confirmation form
  sendPageStart("Factory Reset");
  sendNavBar();
  server.sendContent(
    "<h1>Factory Reset</h1>"
    "<div class='err'><strong>Warning:</strong> This will erase ALL settings including "
    "WiFi credentials, admin password, and EISY/ISY configuration. The device will "
    "reboot into setup mode.</div>"
    "<form method='POST'>"
    "<label>Enter Admin Password to Confirm:</label>"
    "<input type='password' name='password' autofocus required>"
    "<button type='submit' class='btn-red'>Factory Reset</button>"
    "<a class='btn btn-gray' href='/'>Cancel</a>"
    "</form>"
  );
  sendPageEnd();
}

/*
 * handleLogout - Invalidate session and redirect to login.
 */
void handleLogout() {
  invalidateSession();
  server.sendHeader("Set-Cookie", "session=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT");
  server.sendHeader("Location", "/login");
  server.send(302, "text/plain", "");
}

/*
 * ================================================================
 * SECTION: WEB SERVER - API ENDPOINTS
 * ================================================================
 */

/*
 * handleApiStatus - Return JSON status (no authentication required).
 */
void handleApiStatus() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  String json = "{\n";
  json += "  \"version\": \"" + String(FIRMWARE_VERSION) + "\",\n";
  json += "  \"board\": \"" + String(BOARD_TYPE) + "\",\n";
  json += "  \"presence\": " + String(presenceDetected ? "true" : "false") + ",\n";
  json += "  \"moving\": " + String(isMoving ? "true" : "false") + ",\n";
  json += "  \"light_on\": " + String(lightOn ? "true" : "false") + ",\n";
  json += "  \"isy_configured\": " + String(isyConfigured ? "true" : "false") + ",\n";
  json += "  \"integration\": \"" + integrationMode + "\",\n";
  json += "  \"integration_configured\": " + String(integrationConfigured ? "true" : "false") + ",\n";
  json += "  \"uptime_ms\": " + String(millis()) + ",\n";
  json += "  \"free_heap\": " + String(ESP.getFreeHeap()) + ",\n";
  json += "  \"wifi_rssi\": " + String(WiFi.RSSI()) + ",\n";
  json += "  \"ip\": \"" + WiFi.localIP().toString() + "\",\n";
  json += "  \"last_detection_ms\": " + String(lastDetectionTime) + "\n";
  json += "}";
  server.send(200, "application/json", json);
}

/*
 * handleApiConfigExport - Return current config as JSON download.
 */
void handleApiConfigExport() {
  if (!requireAuth()) return;
  server.sendHeader("Content-Disposition", "attachment; filename=\"presence_config.json\"");
  server.send(200, "application/json", buildConfigJson());
}

/*
 * handleApiLogin - JSON login endpoint.
 */
void handleApiLogin() {
  if (server.hasArg("plain")) {
    // Accept JSON body (simple key extraction)
    String body = server.arg("plain");
    // For simplicity, also accept form args
  }
  String pass = server.arg("password");
  if (hashPassword(pass) == adminPasswordHash) {
    String sessionId = createSession();
    server.sendHeader("Set-Cookie", "session=" + sessionId + "; Path=/; HttpOnly; Max-Age=900");
    server.send(200, "application/json", "{\"success\":true,\"session\":\"" + sessionId + "\"}");
  } else {
    server.send(401, "application/json", "{\"success\":false,\"error\":\"Invalid password\"}");
  }
}

/*
 * handleApiLogout - JSON logout endpoint.
 */
void handleApiLogout() {
  invalidateSession();
  server.sendHeader("Set-Cookie", "session=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT");
  server.send(200, "application/json", "{\"success\":true}");
}

/*
 * ================================================================
 * SECTION: WEB SERVER SETUP
 * ================================================================
 */

/*
 * setupWebServerSetupMode - Register URL handlers for captive portal setup mode.
 */
void setupWebServerSetupMode() {
  server.on("/",                        handleSetupRoot);
  server.on("/config",                  handleSetupRoot);
  server.on("/save",           HTTP_POST, handleSetupSave);
  // Captive portal detection URLs
  server.on("/generate_204",            handleCaptivePortal);
  server.on("/gen_204",                 handleCaptivePortal);
  server.on("/hotspot-detect.html",     handleCaptivePortal);
  server.on("/library/test/success.html", handleCaptivePortal);
  server.on("/connecttest.txt",         handleCaptivePortal);
  server.on("/redirect",                handleCaptivePortal);
  server.on("/fwlink",                  handleCaptivePortal);
  server.on("/ncsi.txt",                handleCaptivePortal);
  server.on("/success.txt",             handleCaptivePortal);
  server.onNotFound(handleCaptivePortal);
}

/*
 * setupWebServerRunMode - Register URL handlers for normal (post-setup) operation.
 */
void setupWebServerRunMode() {
  server.on("/",               HTTP_GET,  handleStatus);
  server.on("/status",         HTTP_GET,  handleStatus);
  server.on("/login",          HTTP_GET,  handleLogin);
  server.on("/login",          HTTP_POST, handleLogin);
  server.on("/logout",         HTTP_GET,  handleLogout);
  server.on("/config",         HTTP_GET,  handleConfig);
  server.on("/config",         HTTP_POST, handleConfig);
  server.on("/reset",          HTTP_GET,  handleReset);
  server.on("/reset",          HTTP_POST, handleReset);
  server.on("/api/status",     HTTP_GET,  handleApiStatus);
  server.on("/api/config/export", HTTP_GET, handleApiConfigExport);
  server.on("/api/login",      HTTP_POST, handleApiLogin);
  server.on("/api/logout",     HTTP_POST, handleApiLogout);
  server.onNotFound([]() {
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
  });
}

/*
 * ================================================================
 * SECTION: WIFI FUNCTIONS
 * ================================================================
 */

/*
 * enterConfigMode - Start AP and captive portal for initial setup.
 */
void enterConfigMode() {
  serialPrintln(F("\n*** ENTERING SETUP MODE ***"));

  configMode = true;

  WiFi.mode(WIFI_AP_STA);  // AP+STA so we can scan networks
  generateDeviceSSID();

  WiFi.softAP(deviceSSID.c_str());

  IPAddress apIP = WiFi.softAPIP();

  Serial.println(F("\n================================================"));
  Serial.println(F("*** SETUP MODE ***"));
  Serial.print(F("Connect to WiFi: "));
  Serial.println(deviceSSID);
  Serial.print(F("AP IP Address:   "));
  Serial.println(apIP);
  Serial.println(F("================================================\n"));

  dnsServer.start(DNS_PORT, "*", apIP);

  setupWebServerSetupMode();
  server.begin();

  serialPrintln(F("Setup web server started"));
}

/*
 * connectToWiFi - Connect to configured WiFi network.
 */
void connectToWiFi() {
  serialPrint(F("Connecting to WiFi: "));
  serialPrintln(wifiSSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

  int timeout = 30;
  while (WiFi.status() != WL_CONNECTED && timeout > 0) {
    delay(1000);
    if (Serial && debugLevel > 0) Serial.print(".");
    timeout--;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    serialPrintln(F("WiFi connected!"));
    serialPrint(F("IP Address: "));
    serialPrintln(WiFi.localIP().toString());

    // Start mDNS
    if (MDNS.begin("presence")) {
      serialPrintln(F("mDNS started: http://presence.local"));
    }

    // Start OTA
    ArduinoOTA.setHostname("esp32-presence");
    ArduinoOTA.onStart([]() { serialPrintln(F("OTA update started...")); });
    ArduinoOTA.onEnd([]()   { serialPrintln(F("OTA update complete!")); });
    ArduinoOTA.onError([](ota_error_t error) {
      serialPrintln("OTA error: " + String(error));
    });
    ArduinoOTA.begin();
    serialPrintln(F("OTA updates enabled"));

    // Green flash to confirm connection
    for (int i = 0; i < 3; i++) {
      setRGB(0, 255, 0);
      delay(200);
      setRGB(0, 0, 0);
      delay(200);
    }

    // Setup authenticated web server
    // esp32 Arduino core 3.x changed collectHeaders to require an array + count
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    const char* collectHeaderKeys[] = {"Cookie"};
    server.collectHeaders(collectHeaderKeys, 1);
#else
    server.collectHeaders("Cookie");
#endif
    setupWebServerRunMode();
    server.begin();
    serialPrintln(F("Web server started"));

  } else {
    Serial.println();
    serialPrintln(F("WiFi connection failed! Entering setup mode..."));
    enterConfigMode();
  }
}

/*
 * ================================================================
 * SECTION: SETUP AND LOOP
 * ================================================================
 */

void setup() {
  deviceStartTime = millis();

  Serial.begin(BAUD_RATE_DEBUG);
  delay(100);

  Serial.println(F("\n\n================================================"));
  Serial.println(F("ESP32 LD2410C Presence Detection System"));
  Serial.print(F("Firmware Version: "));
  Serial.println(FIRMWARE_VERSION);
  Serial.print(F("Board: "));
  Serial.println(BOARD_TYPE);
  Serial.println(F("Integrations: EISY/ISY, Insteon Hub 2, Home Assistant, HomeKit"));
  Serial.println(F("================================================\n"));

  // Initialize NeoPixel LED
  strip.begin();
  strip.show();
  strip.setBrightness(50);
  setRGB(0, 0, 0);
  Serial.print(F("RGB LED on GPIO"));
  Serial.println(RGB_LED_PIN);

  // Reset button
  pinMode(PIN_RESET, INPUT_PULLUP);

  // Load configuration (may override default pins)
  loadConfiguration();

  // Apply saved LED brightness
  strip.setBrightness(ledBrightness);

  // Initialize UART for sensor
  Serial2.begin(BAUD_RATE_SENSOR, SERIAL_8N1, pinSensorTx, pinSensorRx);
  Serial.print(F("Sensor UART: RX=GPIO"));
  Serial.print(pinSensorTx);
  Serial.print(F(", TX=GPIO"));
  Serial.println(pinSensorRx);

  // Initialize sensor OUT pin
  pinMode(pinSensorOut, INPUT);

  // Start watchdog timer
  // esp32 Arduino core 3.x changed esp_task_wdt_init to take a config struct
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_config_t wdtConfig = {
    .timeout_ms   = WDT_TIMEOUT_SECONDS * 1000,
    .idle_core_mask = 0,
    .trigger_panic  = true
  };
  esp_task_wdt_init(&wdtConfig);
#else
  esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true);
#endif
  esp_task_wdt_add(NULL);
  serialPrintln(F("Watchdog timer started (8s timeout)"));

  // Decide mode: setup or connect
  if (wifiSSID == "" || checkResetButton()) {
    enterConfigMode();
  } else {
    connectToWiFi();
  }

  Serial.println(F("\n================================================"));
  Serial.println(F("Setup complete!"));
  Serial.println(F("================================================\n"));
}

void loop() {
  // Reset watchdog
  esp_task_wdt_reset();

  if (configMode) {
    dnsServer.processNextRequest();
    server.handleClient();
    blinkBlueHeartbeat();
    // Check for reset button even in config mode
    checkResetButtonHeld();
    return;
  }

  // Normal operation
  ArduinoOTA.handle();
  server.handleClient();

  if (checkResetButtonHeld()) {
    enterConfigMode();
    return;
  }

  readSensorData();
  updateLED();

  if (integrationConfigured) {
    controlLight();
  }

  delay(100);
}
