#include "PresenceRuntime.h"
#include "PresenceCore.h"
#include "PresenceIntegrations.h"
#include "PresenceWeb.h"


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

 * SECTION: FACTORY RESET
 * ================================================================
 */

/*
 * checkResetButton - Check if BOOT button is held at startup.
 * @return true if button is pressed at boot time
 */
bool checkResetButton() {
  if (digitalRead(PIN_RESET) == LOW) {
    delay(30);  // debounce BOOT button at startup
    if (digitalRead(PIN_RESET) != LOW) return false;
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
 * SECTION: WIFI FUNCTIONS
 * ================================================================
 */

/*
 * enterConfigMode - Start AP and captive portal for initial setup.
 */
void enterConfigMode() {
  serialPrintln(F("\n*** ENTERING SETUP MODE ***"));

  configMode = true;

  // Force immediate visual setup indication (steady blue, then heartbeat).
  heartbeatState = true;
  lastHeartbeat = millis();
  setRGB(0, 0, 255);

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

void presenceInit() {
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

void presenceTick() {
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

void setup() {
  presenceInit();
}

void loop() {
  presenceTick();
}
