#include "PresenceRuntime.h"
#include "PresenceCore.h"
#include "PresenceIntegrations.h"
#include "PresenceWeb.h"

namespace {
bool runModeServicesActive = false;
bool runModeWebHandlersConfigured = false;
bool otaCallbacksConfigured = false;
bool serviceModeActive = false;
}  // namespace

void stopRunModeServices();
bool startRunModeServices(bool asServiceMode);
void toggleServiceMode();


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
 * blinkPurpleHeartbeat - Blink purple LED at 1-second intervals (service mode).
 */
void blinkPurpleHeartbeat() {
  if (millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
    heartbeatState = !heartbeatState;
    lastHeartbeat  = millis();
    setRGB(heartbeatState ? 255 : 0, 0, heartbeatState ? 255 : 0);
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
 * pulseRedWarning - Pulse red LED during final-minute no-presence warning.
 * Pulse speeds up every 15 seconds as timeout approaches.
 */
void pulseRedWarning(unsigned long remainingMs) {
  unsigned long periodMs = 1600;  // 60s..45s remaining (slow)
  if (remainingMs <= 45000UL) periodMs = 1100;  // 45s..30s
  if (remainingMs <= 30000UL) periodMs = 700;   // 30s..15s
  if (remainingMs <= 15000UL) periodMs = 400;   // 15s..0s (fast)

  unsigned long halfPeriod = periodMs / 2;
  unsigned long phase = millis() % periodMs;
  unsigned long ramp = (phase <= halfPeriod) ? phase : (periodMs - phase);

  // Keep a visible red floor and pulse up to full red.
  int redLevel = 32 + (int)((223UL * ramp) / halfPeriod);
  setRGB(redLevel, 0, 0);
}

/*
 * updateLED - Set LED color based on current sensor state.
 */
void updateLED() {
  static int prevState = -1;
  int state = 0;

  if (sensorError) {
    blinkRedBlue();
    state = 5;  // reserved for sensor error
  } else if (presenceDetected) {
    setRGB(0, 255, 0);  // Green: presence
    state = 1;
  } else {
    unsigned long remainingMs = 0;
    bool warningState = false;
    if (lightOn && noDetectionTimeout > 0) {
      unsigned long timeoutMs = (unsigned long)noDetectionTimeout * 1000UL;
      unsigned long elapsedMs = millis() - lastDetectionTime;
      if (elapsedMs < timeoutMs) {
        remainingMs = timeoutMs - elapsedMs;
        warningState = (remainingMs <= 60000UL);
      }
    }

    if (!lightOn) {
      setRGB(255, 0, 0);  // Red: no presence, light is off
      state = 3;
    } else if (warningState) {
      pulseRedWarning(remainingMs);  // Red pulse: no presence warning (light-off imminent)
      state = 4;
    } else {
      setRGB(255, 255, 0);  // Yellow: no presence, light still on
      state = 2;
    }
  }

  if (state != prevState && state != 5) {
    prevState = state;
    const char* msgs[] = {
      "",
      "GREEN (presence)",
      "YELLOW (no presence)",
      "RED (no presence, light off)",
      "RED PULSE (no presence warning)"
    };
    if (state >= 1 && state <= 4) {
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

    // OUT pin only reports presence (HIGH/LOW). Keep this legacy flag true
    // while present for API backward compatibility.
    isMoving = true;

    if (millis() - lastStatusPrint > 2000) {
      verbosePrint(F("Presence detected: PRESENT"));
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
 * parseLD2410CSerial - Non-blocking parser for LD2410C UART data frames.
 *
 * The LD2410C sends basic reporting frames at ~10 Hz:
 *   Header (4): FD FC FB FA
 *   Length (2): 0D 00  (13 bytes of payload)
 *   Payload (13):
 *     [0]    0x02  data-type marker
 *     [1]    0xAA  frame head
 *     [2]    target state: 0=none 1=moving 2=stationary 3=both
 *     [3-4]  moving distance (LE, cm)
 *     [5]    moving energy (0-100)
 *     [6-7]  stationary distance (LE, cm)
 *     [8]    stationary energy (0-100)
 *     [9-10] detection distance (LE, cm)
 *     [11]   0x55  frame tail
 *     [12]   0x00  padding
 *   End (4): 04 03 02 01
 *
 * Uses a 64-byte linear shifting buffer. Validation is structural only
 * (frame markers and declared payload length); the LD2410C basic reporting
 * frame does not include a checksum/CRC byte so none is verified.
 * Populates the uartTarget* globals and sets lastUartUpdateMs.
 */
void parseLD2410CSerial() {
  static uint8_t buf[64];
  static int     bufLen = 0;

  // Drain whatever is in the UART FIFO without blocking.
  while (Serial2.available() > 0 && bufLen < (int)sizeof(buf)) {
    buf[bufLen++] = (uint8_t)Serial2.read();
  }

  // Process all complete frames that fit in the buffer.
  while (bufLen >= 23) {
    // Locate frame header FD FC FB FA.
    int startIdx = -1;
    for (int i = 0; i <= bufLen - 4; i++) {
      if (buf[i] == 0xFD && buf[i+1] == 0xFC &&
          buf[i+2] == 0xFB && buf[i+3] == 0xFA) {
        startIdx = i;
        break;
      }
    }
    if (startIdx < 0) {
      // No header yet — keep last 3 bytes in case it's a partial header.
      int keep = (bufLen >= 3) ? 3 : bufLen;
      memmove(buf, buf + (bufLen - keep), keep);
      bufLen = keep;
      return;
    }

    // Discard bytes before the header.
    if (startIdx > 0) {
      bufLen -= startIdx;
      memmove(buf, buf + startIdx, bufLen);
    }

    // Need a full 23-byte frame.
    if (bufLen < 23) return;

    // Validate data length, markers, and frame end.
    uint16_t dataLen = (uint16_t)buf[4] | ((uint16_t)buf[5] << 8);
    bool valid = (dataLen == 13 &&
                  buf[6]  == 0x02 && buf[7]  == 0xAA &&
                  buf[17] == 0x55 &&
                  buf[19] == 0x04 && buf[20] == 0x03 &&
                  buf[21] == 0x02 && buf[22] == 0x01);
    if (!valid) {
      // Not a basic reporting frame; skip one byte and retry.
      bufLen--;
      memmove(buf, buf + 1, bufLen);
      continue;
    }

    // Parse payload fields.
    uartTargetState        = buf[8];
    uartMovingDistance     = (int)((uint16_t)buf[9]  | ((uint16_t)buf[10] << 8));
    uartMovingEnergy       = (int)buf[11];
    uartStationaryDistance = (int)((uint16_t)buf[12] | ((uint16_t)buf[13] << 8));
    uartStationaryEnergy   = (int)buf[14];
    uartDetectionDistance  = (int)((uint16_t)buf[15] | ((uint16_t)buf[16] << 8));
    lastUartUpdateMs       = millis();

    // Consume the processed frame.
    bufLen -= 23;
    if (bufLen > 0) memmove(buf, buf + 23, bufLen);
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
 * @return true only if factory reset was performed (device restarts)
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
    unsigned long held = millis() - pressStartTime;
    wasPressed = false;
    setRGB(0, 0, 0);
    if (held >= 80 && held < RESET_HOLD_TIME) {
      if (!configMode) {
        serialPrintln(F("BOOT short press - toggling service mode"));
        toggleServiceMode();
      }
      return false;
    }
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

void stopRunModeServices() {
  if (!runModeServicesActive) {
    serviceModeActive = false;
    return;
  }

  server.stop();
  MDNS.end();
  runModeServicesActive = false;
  serviceModeActive = false;
  serialPrintln(F("Run-mode web services stopped"));
}

bool startRunModeServices(bool asServiceMode) {
  if (runModeServicesActive) {
    serviceModeActive = asServiceMode;
    if (asServiceMode) {
      heartbeatState = true;
      lastHeartbeat = millis();
      setRGB(255, 0, 255);
    }
    return true;
  }
  if (WiFi.status() != WL_CONNECTED) {
    serialPrintln(F("Cannot start run-mode web services: WiFi not connected"));
    return false;
  }

  if (!runModeWebHandlersConfigured) {
    // esp32 Arduino core 3.x changed collectHeaders to require an array + count
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    const char* collectHeaderKeys[] = {"Cookie"};
    server.collectHeaders(collectHeaderKeys, 1);
#else
    server.collectHeaders("Cookie");
#endif
    setupWebServerRunMode();
    runModeWebHandlersConfigured = true;
  }

  if (MDNS.begin("presence")) {
    serialPrintln(F("mDNS started: http://presence.local"));
  }

  if (!otaCallbacksConfigured) {
    ArduinoOTA.setHostname("esp32-presence");
    ArduinoOTA.onStart([]() { serialPrintln(F("OTA update started...")); });
    ArduinoOTA.onEnd([]()   { serialPrintln(F("OTA update complete!")); });
    ArduinoOTA.onError([](ota_error_t error) {
      serialPrintln("OTA error: " + String(error));
    });
    otaCallbacksConfigured = true;
  }
  ArduinoOTA.begin();
  server.begin();

  runModeServicesActive = true;
  serviceModeActive = asServiceMode;

  if (asServiceMode) {
    heartbeatState = true;
    lastHeartbeat = millis();
    setRGB(255, 0, 255);
    serialPrintln("Service mode enabled (LAN web): http://" + WiFi.localIP().toString());
  } else {
    serialPrintln(F("Run-mode web services enabled"));
  }
  return true;
}

void toggleServiceMode() {
  if (serviceModeActive) {
    stopRunModeServices();
    serialPrintln(F("Service mode disabled - returning to run mode"));
    return;
  }
  if (startRunModeServices(true)) {
    serialPrintln(F("Service mode active - BOOT short press again to exit"));
  }
}

/*
 * enterConfigMode - Start AP and captive portal for initial setup.
 */
void enterConfigMode() {
  serialPrintln(F("\n*** ENTERING SETUP MODE ***"));

  stopRunModeServices();
  configMode = true;

  // Force immediate visual setup indication (steady blue, then heartbeat).
  heartbeatState = true;
  lastHeartbeat = millis();
  setRGB(0, 0, 255);

  WiFi.mode(WIFI_AP_STA);  // AP+STA so we can scan networks
  generateDeviceSSID();

  WiFi.softAP(deviceSSID.c_str());
  startWiFiScanAsync();
  serialPrintln(F("Setup mode WiFi scan started"));

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

  const unsigned long connectTimeoutMs = 30000;
  const unsigned long dotIntervalMs    = 1000;
  unsigned long connectStart           = millis();
  unsigned long lastDotAt              = connectStart;

  while (WiFi.status() != WL_CONNECTED && (millis() - connectStart) < connectTimeoutMs) {
    delay(100);
    // setup() can block here for up to 30s; feed TWDT while waiting for association
    esp_task_wdt_reset();

    if (Serial && debugLevel > 0 && (millis() - lastDotAt) >= dotIntervalMs) {
      Serial.print(".");
      lastDotAt = millis();
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    serialPrintln(F("WiFi connected!"));
    serialPrint(F("IP Address: "));
    serialPrintln(WiFi.localIP().toString());

#if ENABLE_RUNMODE_WEB_OTA_DEFAULT
    startRunModeServices(false);
#else
    serialPrintln(F("Run mode active (web disabled by default)"));
    serialPrintln(F("Press BOOT briefly to toggle LAN web service mode"));
#endif

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

  // Integration worker handles potentially blocking network actions off the main loop
  initIntegrationWorker();

  // Start watchdog timer
  // esp32 Arduino core 3.x changed esp_task_wdt_init to take a config struct
  esp_err_t wdtInitResult = ESP_OK;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_config_t wdtConfig = {
    .timeout_ms   = WDT_TIMEOUT_SECONDS * 1000,
    .idle_core_mask = 0,
    .trigger_panic  = true
  };
  // On Arduino-ESP32 3.x, TWDT may already be initialized by the core.
  // Reconfigure first to avoid "already initialized" boot errors.
  wdtInitResult = esp_task_wdt_reconfigure(&wdtConfig);
  if (wdtInitResult == ESP_ERR_INVALID_STATE) {
    wdtInitResult = esp_task_wdt_init(&wdtConfig);
  }
#else
  wdtInitResult = esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true);
#endif
  if (wdtInitResult != ESP_OK) {
    serialPrintln("Watchdog init warning, err=" + String((int)wdtInitResult));
  }

  esp_err_t wdtAddResult = esp_task_wdt_add(NULL);
  if (wdtAddResult == ESP_ERR_INVALID_ARG) {
    serialPrintln(F("Watchdog task already registered"));
  } else if (wdtAddResult != ESP_OK) {
    serialPrintln("Watchdog task add warning, err=" + String((int)wdtAddResult));
  }
  serialPrintln(F("Watchdog timer started (8s timeout)"));

  // Decide mode: setup or connect
  if (wifiSSID == "" || checkResetButton()) {
    enterConfigMode();
  } else {
    connectToWiFi();
  }

  Serial.println(F("\n================================================"));
  if (configMode) {
    Serial.println(F("Initialization complete: SETUP MODE ACTIVE"));
    Serial.println(F("Open captive portal or browse http://192.168.4.1"));
  } else {
    Serial.println(F("Initialization complete: RUN MODE ACTIVE"));
  }
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
  if (runModeServicesActive) {
    ArduinoOTA.handle();
    server.handleClient();
  }
  checkResetButtonHeld();

  readSensorData();
  parseLD2410CSerial();
  if (serviceModeActive) {
    blinkPurpleHeartbeat();
  } else {
    updateLED();
  }

  controlLight();

  delay(MAIN_LOOP_DELAY_MS);
}

void setup() {
  presenceInit();
}

void loop() {
  presenceTick();
}
