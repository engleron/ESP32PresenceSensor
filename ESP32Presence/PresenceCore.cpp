#include "PresenceCore.h"

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
