#include "PresenceState.h"

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
String isyIP         = "";
String isyUsername   = "";
String isyPassword   = "";
String isyDeviceID   = "";
bool   useHTTPS      = false;
bool   isyConfigured = false;

// Integration selector
String integrationMode       = "none";
bool   integrationConfigured = false;

// Insteon Hub settings
String insteonHubIP   = "";
String insteonHubPort = DEFAULT_INSTEON_HUB_PORT;
String insteonHubUser = "";
String insteonHubPass = "";
String insteonHubAddr = "";

// Home Assistant settings
String haIP       = "";
String haPort     = DEFAULT_HA_PORT;
bool   haHTTPS    = false;
String haToken    = "";
String haEntityId = "";
String haMode     = "light_control";  // "light_control" or "sensor_entity"

#ifdef ENABLE_HOMEKIT
String homekitCode = "11122333";
#endif

// Admin/security/session
String adminPasswordHash = "";
bool   adminPasswordSet  = false;
Session currentSession = {"", 0, false};
int           loginAttempts   = 0;
unsigned long loginLockoutEnd = 0;

// Detection settings/state
int           noDetectionTimeout = 300;
bool          presenceDetected   = false;
bool          isMoving           = false;
bool          lightOn            = false;
bool          sensorError        = false;
unsigned long lastDetectionTime  = 0;
unsigned long lastStatusPrint    = 0;

// LED/runtime state
unsigned long lastHeartbeat  = 0;
bool          heartbeatState = false;
int           ledBrightness  = 50;
unsigned long deviceStartTime = 0;
bool          useCustomPins   = false;
int           debugLevel      = 1;
String        deviceSSID      = "Presence_0000";
