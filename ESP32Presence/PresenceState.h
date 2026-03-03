#pragma once

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

#include "PresenceConfig.h"

struct Session {
  String       sessionId;
  unsigned long expiryTime;
  bool         valid;
};

// Hardware objects
extern Adafruit_NeoPixel strip;
extern WebServer server;
extern DNSServer dnsServer;
extern Preferences preferences;

// Runtime pin config (may be overridden by custom pins)
extern int pinSensorOut;
extern int pinSensorTx;
extern int pinSensorRx;
extern int pinRgbLed;

// WiFi and mode
extern String wifiSSID;
extern String wifiPassword;
extern bool   configMode;

// EISY/ISY settings
extern String isyIP;
extern String isyUsername;
extern String isyPassword;
extern String isyDeviceID;
extern bool   useHTTPS;
extern bool   isyConfigured;

// Integration selector
extern String integrationMode;
extern bool   integrationConfigured;

// Insteon Hub settings
extern String insteonHubIP;
extern String insteonHubPort;
extern String insteonHubUser;
extern String insteonHubPass;
extern String insteonHubAddr;

// Home Assistant settings
extern String haIP;
extern String haPort;
extern bool   haHTTPS;
extern String haToken;
extern String haEntityId;
extern String haMode;    // "light_control" or "sensor_entity"

#ifdef ENABLE_HOMEKIT
extern String homekitCode;
#endif

// Admin/security/session
extern String adminPasswordHash;
extern bool   adminPasswordSet;
extern Session currentSession;
extern int           loginAttempts;
extern unsigned long loginLockoutEnd;

// Detection settings/state
extern int           noDetectionTimeout;
extern bool          presenceDetected;
extern bool          isMoving;
extern bool          lightOn;
extern bool          sensorError;
extern unsigned long lastDetectionTime;
extern unsigned long lastStatusPrint;

// LED/runtime state
extern unsigned long lastHeartbeat;
extern bool          heartbeatState;
extern int           ledBrightness;
extern unsigned long deviceStartTime;
extern bool          useCustomPins;
extern int           debugLevel;
extern String        deviceSSID;
