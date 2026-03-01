/*
 * ===============================================
 * ESP32 Presence Detection System
 * Version: 2.0.0
 * Date: 2026-03-01
 * ===============================================
 *
 * CHANGELOG:
 * v2.0.0 - Multi-board support and modular design
 *   - Added board-specific pin definitions for ESP32 DevKit,
 *     ESP32-S2, ESP32-S3, ESP32-C3, and WEMOS LOLIN D32
 *   - Added ENABLE_PIR feature flag for PIR sensor support
 *   - Added ENABLE_MMWAVE feature flag for LD2410 UART protocol
 *   - Added LIGHT_TIMEOUT_MS, LIGHT_ON_LEVEL, LIGHT_OFF_LEVEL constants
 *   - Added direct LIGHT_PIN relay/LED control (fallback when ISY not set)
 *   - Fixed WiFiClientSecure memory leak in sendISYCommand()
 *   - Renamed PIN_SENSOR_OUT->RADAR_OUT_PIN, PIN_RX2->RADAR_RX_PIN,
 *     PIN_TX2->RADAR_TX_PIN; added PIR_PIN, LIGHT_PIN, RADAR_SERIAL
 *
 * v1.7.1 - Fixed HTTPS compilation error
 *   - Added WiFiClientSecure for HTTPS support
 *   - Fixed setInsecure() compilation error
 *
 * v1.7 - EISY/Polisy compatibility
 *   - Added explicit support for UD EISY
 *   - Works with ISY994, ISY994i, EISY, and Polisy
 *
 * ===============================================
 * COMPATIBLE CONTROLLERS:
 * ===============================================
 * - Universal Devices EISY
 * - Universal Devices Polisy
 * - Universal Devices ISY994i
 * - Universal Devices ISY994 (all versions)
 *
 * ===============================================
 * HARDWARE CONNECTIONS:
 * ===============================================
 * LD2410C Sensor:
 *   VCC -> 5V on ESP32
 *   GND -> GND on ESP32
 *   TX (from sensor) -> RADAR_RX_PIN (ESP32 receives)
 *   RX (from sensor) -> RADAR_TX_PIN (ESP32 transmits)
 *   OUT -> RADAR_OUT_PIN (digital presence detection)
 *
 * PIR Sensor (optional, requires ENABLE_PIR):
 *   VCC -> 3.3V or 5V
 *   GND -> GND
 *   OUT -> PIR_PIN
 *
 * Light / Relay (optional, used when ISY not configured):
 *   Signal -> LIGHT_PIN
 *
 * Built-in RGB LED / WS2812:
 *   See board-specific PIN_RGB_LED below
 *
 * Reset Button:
 *   BOOT button on PIN_RESET - Hold 3 seconds for factory reset
 *
 * ===============================================
 */

// ===============================================
// INCLUDE LIBRARIES
// ===============================================
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>  // For HTTPS support
#include <Adafruit_NeoPixel.h>

// ===============================================
// FEATURE FLAGS
// Uncomment the defines below to enable optional features.
// ===============================================

// Uncomment to enable PIR motion sensor (connect sensor OUT to PIR_PIN)
// #define ENABLE_PIR

// Uncomment to enable LD2410 mmWave radar UART protocol
// Requires the "ld2410" library by ncmreynolds (install via Library Manager)
// #define ENABLE_MMWAVE

// ===============================================
// BOARD-SPECIFIC PIN DEFINITIONS
// To add a new board, insert a new #elif block below.
// ===============================================

#if defined(CONFIG_IDF_TARGET_ESP32S3)
  // ESP32-S3 DevKitC-1
  #define BOARD_NAME      "ESP32-S3"
  #define PIN_RGB_LED     48   // Built-in WS2812 on ESP32-S3-DevKitC-1
  #define NUM_PIXELS      1
  #define PIN_RESET       0
  #define PIR_PIN         6
  #define RADAR_OUT_PIN   5
  #define RADAR_RX_PIN    18
  #define RADAR_TX_PIN    17
  #define LIGHT_PIN       10
  #define RADAR_SERIAL    Serial2

#elif defined(CONFIG_IDF_TARGET_ESP32C3)
  // ESP32-C3 DevKitM-1
  #define BOARD_NAME      "ESP32-C3"
  #define PIN_RGB_LED     8    // Built-in WS2812 on ESP32-C3-DevKitM-1
  #define NUM_PIXELS      1
  #define PIN_RESET       9
  #define PIR_PIN         2
  #define RADAR_OUT_PIN   4
  #define RADAR_RX_PIN    6
  #define RADAR_TX_PIN    7
  #define LIGHT_PIN       10
  #define RADAR_SERIAL    Serial1

#elif defined(CONFIG_IDF_TARGET_ESP32S2)
  // ESP32-S2 DevKitM-1
  #define BOARD_NAME      "ESP32-S2"
  #define PIN_RGB_LED     18   // Built-in WS2812 on ESP32-S2-DevKitM-1 (some models)
  #define NUM_PIXELS      1
  #define PIN_RESET       0
  #define PIR_PIN         6
  #define RADAR_OUT_PIN   5
  #define RADAR_RX_PIN    37
  #define RADAR_TX_PIN    38
  #define LIGHT_PIN       10
  #define RADAR_SERIAL    Serial1

#elif defined(ARDUINO_LOLIN_D32)
  // WEMOS LOLIN D32
  #define BOARD_NAME      "WEMOS LOLIN D32"
  #define PIN_RGB_LED     5    // Built-in LED (active-low; attach external NeoPixel for RGB)
  #define NUM_PIXELS      1
  #define PIN_RESET       0
  #define PIR_PIN         27
  #define RADAR_OUT_PIN   4
  #define RADAR_RX_PIN    16
  #define RADAR_TX_PIN    17
  #define LIGHT_PIN       32
  #define RADAR_SERIAL    Serial2

#else
  // Generic ESP32 DevKit (default, e.g. ESP32-DevKitC-32E)
  #define BOARD_NAME      "ESP32 DevKit"
  #define PIN_RGB_LED     16   // WS2812 on GPIO16 (ESP32-DevKitC-32E built-in)
  #define NUM_PIXELS      1
  #define PIN_RESET       0
  #define PIR_PIN         23
  #define RADAR_OUT_PIN   4
  #define RADAR_RX_PIN    18
  #define RADAR_TX_PIN    17
  #define LIGHT_PIN       26
  #define RADAR_SERIAL    Serial2

#endif

// ===============================================
// GLOBAL CONSTANTS
// ===============================================
#define BAUD_RATE_DEBUG  115200
#define BAUD_RATE_SENSOR 256000

#define HEARTBEAT_INTERVAL    1000
#define SENSOR_CHECK_INTERVAL 500
#define RESET_HOLD_TIME       3000

#define DNS_PORT 53

// Light control defaults (configurable via WiFi portal at runtime)
#define LIGHT_TIMEOUT_MS  300000UL  // 5 minutes of no presence before light turns off
#define LIGHT_ON_LEVEL    HIGH      // Output level when light is ON  (HIGH for active-HIGH relay)
#define LIGHT_OFF_LEVEL   LOW       // Output level when light is OFF (LOW  for active-HIGH relay)

// ===============================================
// GLOBAL VARIABLES
// ===============================================
Adafruit_NeoPixel strip(NUM_PIXELS, PIN_RGB_LED, NEO_GRB + NEO_KHZ800);

WebServer server(80);
DNSServer dnsServer;
String wifiSSID = "";
String wifiPassword = "";
bool configMode = false;
bool wifiConfigured = false;

String isyIP = "";
String isyUsername = "";
String isyPassword = "";
String isyDeviceID = "";
bool isyConfigured = false;
bool useHTTPS = false;

int noDetectionTimeout = LIGHT_TIMEOUT_MS / 1000;

bool presenceDetected = false;
bool isMoving = false;
bool lightOn = false;
bool sensorError = false;
unsigned long lastDetectionTime = 0;
unsigned long lastHeartbeat = 0;
bool heartbeatState = false;

int currentLEDState = -1;

Preferences preferences;

// ===============================================
// UTILITY FUNCTIONS
// ===============================================

void serialPrint(const String& message) {
  if (Serial) {
    unsigned long timestamp = millis();
    unsigned long seconds = timestamp / 1000;
    unsigned long milliseconds = timestamp % 1000;
    
    Serial.print("[");
    Serial.print(seconds);
    Serial.print(".");
    if (milliseconds < 100) Serial.print("0");
    if (milliseconds < 10) Serial.print("0");
    Serial.print(milliseconds);
    Serial.print("s] ");
    Serial.print(message);
  }
}

void serialPrintln(const String& message) {
  if (Serial) {
    serialPrint(message);
    Serial.println();
  }
}

// ===============================================
// SETUP FUNCTION
// ===============================================
void setup() {
  Serial.begin(BAUD_RATE_DEBUG);
  delay(100);
  
  if (Serial) {
    Serial.println("\n\n===============================================");
    Serial.println("ESP32 Presence Detection System v2.0.0");
    Serial.print("Board: ");
    Serial.println(BOARD_NAME);
    Serial.println("Compatible with EISY, Polisy, ISY994");
    Serial.println("===============================================");
#ifdef ENABLE_PIR
    Serial.println("Feature: PIR sensor ENABLED (GPIO " + String(PIR_PIN) + ")");
#endif
#ifdef ENABLE_MMWAVE
    Serial.println("Feature: mmWave UART ENABLED (RX=" + String(RADAR_RX_PIN) + ", TX=" + String(RADAR_TX_PIN) + ")");
#endif
    Serial.println("===============================================\n");
  }
  
  strip.begin();
  strip.show();
  strip.setBrightness(50);
  serialPrintln("Built-in RGB LED initialized on GPIO" + String(PIN_RGB_LED));
  
  pinMode(PIN_RESET, INPUT_PULLUP);
  setRGB(0, 0, 0);
  
  // Initialize relay / direct light output pin
  pinMode(LIGHT_PIN, OUTPUT);
  digitalWrite(LIGHT_PIN, LIGHT_OFF_LEVEL);

#ifdef ENABLE_MMWAVE
  RADAR_SERIAL.begin(BAUD_RATE_SENSOR, SERIAL_8N1, RADAR_RX_PIN, RADAR_TX_PIN);
  serialPrint("Sensor UART initialized on RX=GPIO");
  if (Serial) {
    Serial.print(RADAR_RX_PIN);
    Serial.print(", TX=GPIO");
    Serial.println(RADAR_TX_PIN);
  }
#endif
  
  pinMode(RADAR_OUT_PIN, INPUT);
  
#ifdef ENABLE_PIR
  pinMode(PIR_PIN, INPUT);
  serialPrintln("PIR sensor initialized on GPIO" + String(PIR_PIN));
#endif
  
  loadConfiguration();
  
  if (wifiSSID == "" || checkResetButton()) {
    enterConfigMode();
  } else {
    connectToWiFi();
  }
  
  serialPrintln("\nSetup complete!");
  if (Serial) {
    Serial.println("===============================================\n");
  }
}

// ===============================================
// MAIN LOOP
// ===============================================
void loop() {
  if (configMode) {
    dnsServer.processNextRequest();
    server.handleClient();
    blinkBlueHeartbeat();
    return;
  }
  
  if (checkResetButtonHeld()) {
    enterConfigMode();
    return;
  }
  
  readSensorData();
  updateLED();
  controlLight();
  
  delay(100);
}

// ===============================================
// CONFIGURATION FUNCTIONS
// ===============================================

void loadConfiguration() {
  serialPrintln("Loading configuration from flash memory...");
  
  preferences.begin("presence", true);
  
  wifiSSID = preferences.getString("wifi_ssid", "");
  wifiPassword = preferences.getString("wifi_pass", "");
  
  isyIP = preferences.getString("isy_ip", "");
  isyUsername = preferences.getString("isy_user", "");
  isyPassword = preferences.getString("isy_pass", "");
  isyDeviceID = preferences.getString("isy_device", "");
  useHTTPS = preferences.getBool("use_https", false);
  
  noDetectionTimeout = preferences.getInt("timeout", LIGHT_TIMEOUT_MS / 1000);
  
  preferences.end();
  
  wifiConfigured = (wifiSSID != "");
  isyConfigured = (isyIP != "" && isyUsername != "" && isyPassword != "" && isyDeviceID != "");
  
  serialPrintln("Configuration loaded:");
  serialPrint("  WiFi SSID: ");
  serialPrintln(wifiSSID == "" ? "(not set)" : wifiSSID);
  serialPrint("  WiFi Password: ");
  serialPrintln(wifiPassword == "" ? "(not set)" : "****");
  serialPrint("  EISY/ISY IP: ");
  serialPrintln(isyIP == "" ? "(not set - OPTIONAL)" : isyIP);
  serialPrint("  EISY/ISY Username: ");
  serialPrintln(isyUsername == "" ? "(not set - OPTIONAL)" : isyUsername);
  serialPrint("  Device ID: ");
  serialPrintln(isyDeviceID == "" ? "(not set - OPTIONAL)" : isyDeviceID);
  serialPrint("  Use HTTPS: ");
  serialPrintln(useHTTPS ? "Yes" : "No (HTTP)");
  serialPrint("  Timeout: ");
  if (Serial) {
    Serial.print(noDetectionTimeout);
    Serial.println(" seconds");
  }
  
  if (isyConfigured) {
    serialPrintln("  Light control: ENABLED");
  } else {
    serialPrintln("  Light control: DISABLED (not configured)");
  }
}

void saveConfiguration() {
  serialPrintln("Saving configuration to flash memory...");
  
  preferences.begin("presence", false);
  
  preferences.putString("wifi_ssid", wifiSSID);
  preferences.putString("wifi_pass", wifiPassword);
  preferences.putString("isy_ip", isyIP);
  preferences.putString("isy_user", isyUsername);
  preferences.putString("isy_pass", isyPassword);
  preferences.putString("isy_device", isyDeviceID);
  preferences.putBool("use_https", useHTTPS);
  preferences.putInt("timeout", noDetectionTimeout);
  
  preferences.end();
  
  serialPrintln("Configuration saved successfully!");
}

void clearConfiguration() {
  serialPrintln("Clearing all configuration...");
  
  preferences.begin("presence", false);
  preferences.clear();
  preferences.end();
  
  wifiSSID = "";
  wifiPassword = "";
  isyIP = "";
  isyUsername = "";
  isyPassword = "";
  isyDeviceID = "";
  useHTTPS = false;
  noDetectionTimeout = LIGHT_TIMEOUT_MS / 1000;
  wifiConfigured = false;
  isyConfigured = false;
  
  serialPrintln("Configuration cleared!");
}

// ===============================================
// WIFI FUNCTIONS
// ===============================================

void enterConfigMode() {
  serialPrintln("\n*** ENTERING CONFIGURATION MODE ***");
  serialPrintln("Starting WiFi Access Point...");
  
  configMode = true;
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32-Presence-Setup");
  
  IPAddress apIP = WiFi.softAPIP();
  serialPrintln("Access Point started!");
  serialPrintln("SSID: ESP32-Presence-Setup");
  serialPrint("IP Address: ");
  serialPrintln(apIP.toString());
  serialPrintln("\nConnect to this WiFi network and you will be");
  serialPrintln("automatically redirected to the configuration page.");
  serialPrint("Or manually go to: http://");
  serialPrintln(apIP.toString());
  
  dnsServer.start(DNS_PORT, "*", apIP);
  serialPrintln("Captive portal DNS server started");
  
  setupWebServer();
  
  server.begin();
  serialPrintln("Web server started!");
}

void connectToWiFi() {
  serialPrint("\nConnecting to WiFi: ");
  serialPrintln(wifiSSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  
  int timeout = 30;
  while (WiFi.status() != WL_CONNECTED && timeout > 0) {
    delay(1000);
    if (Serial) Serial.print(".");
    timeout--;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    serialPrintln("\nWiFi connected!");
    serialPrint("IP Address: ");
    serialPrintln(WiFi.localIP().toString());
    
    for (int i = 0; i < 3; i++) {
      setRGB(0, 255, 0);
      delay(200);
      setRGB(0, 0, 0);
      delay(200);
    }
  } else {
    serialPrintln("\nWiFi connection failed!");
    serialPrintln("Entering configuration mode...");
    enterConfigMode();
  }
}

// ===============================================
// WEB SERVER FUNCTIONS
// ===============================================

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/config", handleRoot);
  server.on("/generate_204", handleCaptivePortal);
  server.on("/gen_204", handleCaptivePortal);
  server.on("/hotspot-detect.html", handleCaptivePortal);
  server.on("/library/test/success.html", handleCaptivePortal);
  server.on("/connecttest.txt", handleCaptivePortal);
  server.on("/redirect", handleCaptivePortal);
  server.on("/fwlink", handleCaptivePortal);
  server.on("/save", handleSave);
  server.onNotFound(handleCaptivePortal);
}

void handleCaptivePortal() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta http-equiv='refresh' content='0; url=http://192.168.4.1/config'>";
  html += "</head><body>Redirecting...</body></html>";
  server.send(200, "text/html", html);
}

String scanWiFiNetworks() {
  serialPrintln("Scanning for WiFi networks...");
  int n = WiFi.scanNetworks();
  String options = "";
  
  if (n == 0) {
    options = "<option value=''>No networks found</option>";
  } else {
    String seenSSIDs[50];
    int seenCount = 0;
    
    for (int i = 0; i < n && seenCount < 50; i++) {
      String ssid = WiFi.SSID(i);
      
      bool isDuplicate = false;
      for (int j = 0; j < seenCount; j++) {
        if (seenSSIDs[j] == ssid) {
          isDuplicate = true;
          break;
        }
      }
      
      if (!isDuplicate) {
        seenSSIDs[seenCount++] = ssid;
        
        int rssi = WiFi.RSSI(i);
        String security = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " (Open)" : "";
        String selected = (ssid == wifiSSID) ? " selected" : "";
        
        options += "<option value='" + ssid + "'" + selected + ">" + ssid + " (" + String(rssi) + " dBm)" + security + "</option>";
      }
    }
  }
  
  serialPrint("Found ");
  if (Serial) {
    Serial.print(n);
    Serial.println(" networks");
  }
  
  return options;
}

void handleRoot() {
  String networkOptions = scanWiFiNetworks();
  
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta charset='UTF-8'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }";
  html += ".container { max-width: 500px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
  html += "h1 { color: #333; margin-top: 0; font-size: 24px; }";
  html += "h2 { color: #666; font-size: 18px; margin-top: 25px; border-bottom: 2px solid #4CAF50; padding-bottom: 5px; }";
  html += "label { display: block; margin-top: 15px; font-weight: bold; color: #555; font-size: 14px; }";
  html += "input, select { width: 100%; padding: 10px; margin-top: 5px; box-sizing: border-box; border: 1px solid #ddd; border-radius: 4px; font-size: 16px; }";
  html += "input[type='checkbox'] { width: auto; margin-right: 8px; }";
  html += "select { background: white; }";
  html += "button { margin-top: 20px; padding: 12px 30px; background: #4CAF50; color: white; border: none; cursor: pointer; font-size: 16px; border-radius: 4px; width: 100%; }";
  html += "button:hover { background: #45a049; }";
  html += ".info { background: #e3f2fd; padding: 12px; margin: 15px 0; border-radius: 5px; border-left: 4px solid #2196F3; font-size: 14px; }";
  html += ".optional { background: #fff3e0; padding: 12px; margin: 15px 0; border-radius: 5px; border-left: 4px solid #ff9800; font-size: 14px; }";
  html += ".version { text-align: center; color: #999; font-size: 12px; margin-top: 20px; }";
  html += ".help { font-size: 12px; color: #777; margin-top: 3px; }";
  html += ".checkbox-label { display: inline; font-weight: normal; }";
  html += "</style></head><body>";
  
  html += "<div class='container'>";
  html += "<h1>ESP32 Presence Detector Setup</h1>";
  html += "<div class='info'><strong>Welcome!</strong> Configure your device to get started.</div>";
  
  html += "<form action='/save' method='POST' autocomplete='on'>";
  
  html += "<h2>WiFi Settings (Required)</h2>";
  
  html += "<label for='wifi_ssid'>WiFi Network:</label>";
  html += "<select name='wifi_ssid' id='wifi_ssid' required>";
  html += "<option value=''>-- Select Network --</option>";
  html += networkOptions;
  html += "</select>";
  
  html += "<label for='wifi_pass'>WiFi Password:</label>";
  html += "<input type='password' name='wifi_pass' id='wifi_pass' value='" + wifiPassword + "' placeholder='Enter WiFi password' autocomplete='off'>";
  html += "<div class='help'>Tip: Use your phone's WiFi password sharing feature</div>";
  
  html += "<h2>Detection Settings</h2>";
  html += "<label for='timeout'>Light Off Delay (minutes):</label>";
  html += "<select name='timeout' id='timeout'>";
  html += "<option value='60'" + String(noDetectionTimeout == 60 ? " selected" : "") + ">1 minute</option>";
  html += "<option value='120'" + String(noDetectionTimeout == 120 ? " selected" : "") + ">2 minutes</option>";
  html += "<option value='180'" + String(noDetectionTimeout == 180 ? " selected" : "") + ">3 minutes</option>";
  html += "<option value='300'" + String(noDetectionTimeout == 300 ? " selected" : "") + ">5 minutes (default)</option>";
  html += "<option value='600'" + String(noDetectionTimeout == 600 ? " selected" : "") + ">10 minutes</option>";
  html += "<option value='900'" + String(noDetectionTimeout == 900 ? " selected" : "") + ">15 minutes</option>";
  html += "</select>";
  html += "<div class='help'>Time to wait with no presence before turning off light</div>";
  
  html += "<h2>EISY / ISY / Polisy Settings (Optional)</h2>";
  html += "<div class='optional'><strong>Note:</strong> Works with EISY, Polisy, ISY994i, and ISY994. Leave blank to skip light control.</div>";
  
  html += "<label for='isy_ip'>Controller IP Address:</label>";
  html += "<input type='text' name='isy_ip' id='isy_ip' value='" + isyIP + "' placeholder='e.g., 192.168.1.100' autocomplete='off'>";
  
  html += "<label>";
  html += "<input type='checkbox' name='use_https' id='use_https' value='1'" + String(useHTTPS ? " checked" : "") + ">";
  html += "<span class='checkbox-label'>Use HTTPS (EISY only, leave unchecked for ISY994)</span>";
  html += "</label>";
  html += "<div class='help'>EISY supports HTTPS. ISY994 uses HTTP only.</div>";
  
  html += "<label for='isy_user'>Controller Username:</label>";
  html += "<input type='text' name='isy_user' id='isy_user' value='" + isyUsername + "' placeholder='Optional' autocomplete='username'>";
  
  html += "<label for='isy_pass'>Controller Password:</label>";
  html += "<input type='password' name='isy_pass' id='isy_pass' value='" + isyPassword + "' placeholder='Optional' autocomplete='off'>";
  
  html += "<label for='isy_device'>Insteon Device Address:</label>";
  html += "<input type='text' name='isy_device' id='isy_device' value='" + isyDeviceID + "' placeholder='e.g., 1A.2B.3C' autocomplete='off'>";
  html += "<div class='help'>Find this in the EISY/ISY Admin Console</div>";
  
  html += "<button type='submit'>Save Configuration</button>";
  html += "</form>";
  
  html += "<div class='version'>v2.0.0 | " + String(BOARD_NAME) + " | EISY/Polisy/ISY Compatible</div>";
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
}

void handleSave() {
  wifiSSID = server.arg("wifi_ssid");
  wifiPassword = server.arg("wifi_pass");
  isyIP = server.arg("isy_ip");
  isyUsername = server.arg("isy_user");
  isyPassword = server.arg("isy_pass");
  isyDeviceID = server.arg("isy_device");
  useHTTPS = (server.arg("use_https") == "1");
  
  String timeoutStr = server.arg("timeout");
  noDetectionTimeout = timeoutStr.toInt();
  if (noDetectionTimeout < 60) noDetectionTimeout = LIGHT_TIMEOUT_MS / 1000;
  
  wifiSSID.trim();
  wifiPassword.trim();
  isyIP.trim();
  isyUsername.trim();
  isyPassword.trim();
  isyDeviceID.trim();
  
  saveConfiguration();
  
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta charset='UTF-8'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; text-align: center; }";
  html += ".container { max-width: 500px; margin: 50px auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
  html += "h1 { color: #4CAF50; }";
  html += ".checkmark { font-size: 60px; color: #4CAF50; }";
  html += "p { color: #666; line-height: 1.6; }";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<div class='checkmark'>&#10004;</div>";
  html += "<h1>Configuration Saved!</h1>";
  html += "<p>Device will restart and connect to your WiFi network...</p>";
  
  if (isyConfigured) {
    html += "<p><strong>Light control enabled</strong> (" + String(useHTTPS ? "HTTPS" : "HTTP") + ")</p>";
  } else {
    html += "<p><strong>Light control disabled</strong></p>";
  }
  
  html += "<p>Light off delay: " + String(noDetectionTimeout / 60) + " minutes</p>";
  html += "<p>The LED will flash green when connected.</p>";
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
  
  serialPrintln("Configuration saved! Restarting...");
  delay(2000);
  
  ESP.restart();
}

// ===============================================
// SENSOR FUNCTIONS
// ===============================================

void readSensorData() {
  bool radarDetected = (digitalRead(RADAR_OUT_PIN) == HIGH);

#ifdef ENABLE_PIR
  bool pirDetected = (digitalRead(PIR_PIN) == HIGH);
  bool anyDetected = radarDetected || pirDetected;
#else
  bool anyDetected = radarDetected;
#endif

  if (anyDetected) {
    presenceDetected = true;
    lastDetectionTime = millis();
    sensorError = false;
    
    isMoving = ((millis() / 30000) % 2 == 0);
    
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 2000) {
      serialPrint("Presence detected: ");
      serialPrintln(isMoving ? "MOVING" : "STATIC");
      lastPrint = millis();
    }
  } else {
    presenceDetected = false;
    isMoving = false;
    
    if (lightOn && (millis() - lastDetectionTime > (unsigned long)(noDetectionTimeout * 1000))) {
      serialPrint("No detection for ");
      if (Serial) {
        Serial.print(noDetectionTimeout / 60);
        Serial.println(" minutes");
      }
    }
  }
  
  sensorError = false;
}

// ===============================================
// LED CONTROL FUNCTIONS
// ===============================================

void updateLED() {
  int newState = -1;
  
  if (sensorError) {
    blinkRedBlue();
    newState = 4;
  } else if (presenceDetected) {
    if (isMoving) {
      setRGB(0, 255, 0);
      newState = 1;
    } else {
      setRGB(255, 255, 0);
      newState = 2;
    }
  } else {
    setRGB(255, 0, 0);
    newState = 3;
  }
  
  if (newState != currentLEDState && newState != 0 && newState != 4) {
    currentLEDState = newState;
    if (newState == 1) {
      serialPrintln("LED: GREEN (movement detected)");
    } else if (newState == 2) {
      serialPrintln("LED: YELLOW (static presence)");
    } else if (newState == 3) {
      serialPrintln("LED: RED (no presence)");
    }
  }
}

void blinkBlueHeartbeat() {
  if (millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
    heartbeatState = !heartbeatState;
    lastHeartbeat = millis();
    
    if (heartbeatState) {
      setRGB(0, 0, 255);
      if (currentLEDState != 0) {
        currentLEDState = 0;
        serialPrintln("LED: BLUE heartbeat (config mode)");
      }
    } else {
      setRGB(0, 0, 0);
    }
  }
}

void blinkRedBlue() {
  static unsigned long lastBlink = 0;
  static bool isRed = true;
  
  if (millis() - lastBlink > 500) {
    isRed = !isRed;
    lastBlink = millis();
    
    if (isRed) {
      setRGB(255, 0, 0);
    } else {
      setRGB(0, 0, 255);
    }
  }
}

void setRGB(int r, int g, int b) {
  strip.setPixelColor(0, strip.Color(r, g, b));
  strip.show();
}

// ===============================================
// LIGHT CONTROL FUNCTIONS
// ===============================================

void controlLight() {
  bool shouldBeOn = presenceDetected || (millis() - lastDetectionTime < (unsigned long)(noDetectionTimeout * 1000));
  
  if (shouldBeOn != lightOn) {
    if (shouldBeOn) {
      turnLightOn();
    } else {
      turnLightOff();
    }
  }
}

void turnLightOn() {
  serialPrintln("Turning light ON...");
  
  bool success = false;

  if (isyConfigured) {
    String deviceID = isyDeviceID;
    deviceID.replace(" ", "%20");
    String protocol = useHTTPS ? "https" : "http";
    String url = protocol + "://" + isyIP + "/rest/nodes/" + deviceID + "/cmd/DON";
    success = sendISYCommand(url);
  } else {
    // Direct relay / LED control via LIGHT_PIN
    digitalWrite(LIGHT_PIN, LIGHT_ON_LEVEL);
    success = true;
  }

  if (success) {
    lightOn = true;
    serialPrintln("Light turned ON successfully");
  } else {
    serialPrintln("Failed to turn light ON");
  }
}

void turnLightOff() {
  serialPrintln("Turning light OFF...");
  
  bool success = false;

  if (isyConfigured) {
    String deviceID = isyDeviceID;
    deviceID.replace(" ", "%20");
    String protocol = useHTTPS ? "https" : "http";
    String url = protocol + "://" + isyIP + "/rest/nodes/" + deviceID + "/cmd/DOF";
    success = sendISYCommand(url);
  } else {
    // Direct relay / LED control via LIGHT_PIN
    digitalWrite(LIGHT_PIN, LIGHT_OFF_LEVEL);
    success = true;
  }

  if (success) {
    lightOn = false;
    serialPrintln("Light turned OFF successfully");
  } else {
    serialPrintln("Failed to turn light OFF");
  }
}

bool sendISYCommand(String url) {
  if (WiFi.status() != WL_CONNECTED) {
    serialPrintln("WiFi not connected - cannot send command");
    return false;
  }
  
  HTTPClient http;
  WiFiClientSecure secureClient;
  
  if (useHTTPS) {
    secureClient.setInsecure();  // Skip certificate validation for self-signed certs
    http.begin(secureClient, url);
  } else {
    http.begin(url);
  }
  
  http.setAuthorization(isyUsername.c_str(), isyPassword.c_str());
  
  int httpCode = http.GET();
  bool success = (httpCode == 200);
  
  if (success) {
    serialPrint("Command sent successfully (HTTP ");
    if (Serial) {
      Serial.print(httpCode);
      Serial.println(")");
    }
  } else {
    serialPrint("Command failed (HTTP ");
    if (Serial) {
      Serial.print(httpCode);
      Serial.println(")");
    }
  }
  
  http.end();
  return success;
}

// ===============================================
// RESET BUTTON FUNCTIONS
// ===============================================

bool checkResetButton() {
  if (digitalRead(PIN_RESET) == LOW) {
    serialPrintln("BOOT button pressed at startup!");
    delay(100);
    return true;
  }
  return false;
}

bool checkResetButtonHeld() {
  static unsigned long pressStartTime = 0;
  static bool wasPressed = false;
  
  bool isPressed = (digitalRead(PIN_RESET) == LOW);
  
  if (isPressed && !wasPressed) {
    pressStartTime = millis();
    wasPressed = true;
    serialPrintln("BOOT button pressed - hold for 3 seconds to factory reset");
  } else if (!isPressed && wasPressed) {
    wasPressed = false;
    serialPrintln("BOOT button released");
  } else if (isPressed && wasPressed) {
    if (millis() - pressStartTime > RESET_HOLD_TIME) {
      serialPrintln("Factory reset triggered!");
      
      for (int i = 0; i < 3; i++) {
        setRGB(255, 0, 0);
        delay(200);
        setRGB(0, 0, 255);
        delay(200);
      }
      setRGB(0, 0, 0);
      delay(300);
      
      clearConfiguration();
      delay(1000);
      ESP.restart();
      
      return true;
    }
  }
  
  return false;
}
