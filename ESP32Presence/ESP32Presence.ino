/*
 * ESP32Presence - Presence Detection & Light Control
 *
 * Supports multiple ESP32 board types:
 *   - ESP32 DevKit (generic)
 *   - ESP32-S2
 *   - ESP32-S3
 *   - ESP32-C3
 *   - WEMOS LOLIN D32 / D32 Pro
 *
 * Sensors:
 *   - PIR motion sensor (always enabled)
 *   - LD2410 mmWave radar (optional — uncomment ENABLE_MMWAVE below)
 *     Detects stationary presence that PIR may miss.
 *
 * Output:
 *   - Relay or LED for light control
 *   - Light turns ON when presence is detected
 *   - Light turns OFF after LIGHT_TIMEOUT_MS of no presence
 *
 * Libraries (install via Arduino Library Manager):
 *   - ld2410 by ncmreynolds  (only required when ENABLE_MMWAVE is enabled)
 *     https://github.com/ncmreynolds/ld2410
 */

// ============================================================
// USER CONFIGURATION
// ============================================================

// Uncomment the line below to enable the LD2410 mmWave radar sensor.
// Requires the "ld2410" library from Arduino Library Manager.
// #define ENABLE_MMWAVE

// Light auto-off delay in milliseconds after last detected presence.
// Default: 5 minutes
#define LIGHT_TIMEOUT_MS  (5UL * 60UL * 1000UL)

// Output logic level when the light should be ON.
// Use HIGH for active-HIGH relays/LEDs.
// Use LOW  for active-LOW relay modules (common on many relay boards).
#define LIGHT_ON_LEVEL   HIGH
#define LIGHT_OFF_LEVEL  LOW

// ============================================================
// BOARD-SPECIFIC PIN DEFINITIONS
// ============================================================
// Add a new #elif block here to support additional boards.
// Pin numbers refer to GPIO numbers.

#if defined(ARDUINO_LOLIN_D32) || defined(ARDUINO_LOLIN_D32_PRO)
  // WEMOS LOLIN D32 / D32 Pro
  #define BOARD_NAME      "WEMOS LOLIN D32"
  #define PIR_PIN         34  // GPIO 34 (input-only)
  #define LIGHT_PIN       26
  #define RADAR_RX_PIN    16  // ESP32 RX  <-- LD2410 TX
  #define RADAR_TX_PIN    17  // ESP32 TX  --> LD2410 RX

#elif defined(ARDUINO_ESP32S3_DEV) || defined(CONFIG_IDF_TARGET_ESP32S3)
  // ESP32-S3
  #define BOARD_NAME      "ESP32-S3"
  #define PIR_PIN         4
  #define LIGHT_PIN       5
  #define RADAR_RX_PIN    17
  #define RADAR_TX_PIN    18

#elif defined(ARDUINO_ESP32S2_DEV) || defined(CONFIG_IDF_TARGET_ESP32S2)
  // ESP32-S2
  #define BOARD_NAME      "ESP32-S2"
  #define PIR_PIN         10
  #define LIGHT_PIN       6
  #define RADAR_RX_PIN    9
  #define RADAR_TX_PIN    8

#elif defined(ARDUINO_ESP32C3_DEV) || defined(CONFIG_IDF_TARGET_ESP32C3)
  // ESP32-C3
  #define BOARD_NAME      "ESP32-C3"
  #define PIR_PIN         2
  #define LIGHT_PIN       3
  #define RADAR_RX_PIN    4
  #define RADAR_TX_PIN    5

#elif defined(ESP32)
  // Generic ESP32 / DevKit (fallback for all other ESP32 variants)
  #define BOARD_NAME      "ESP32 DevKit"
  #define PIR_PIN         15
  #define LIGHT_PIN       18
  #define RADAR_RX_PIN    32
  #define RADAR_TX_PIN    33

#else
  #error "Unsupported board. Add pin definitions for your board in the BOARD-SPECIFIC PIN DEFINITIONS section above."
#endif

// ============================================================
// MMWAVE RADAR (optional)
// ============================================================

#ifdef ENABLE_MMWAVE
  #include <ld2410.h>
  ld2410 radar;
  #define RADAR_SERIAL    Serial1
  #define RADAR_BAUD      256000
#endif

// ============================================================
// GLOBAL STATE
// ============================================================

static bool           lightOn           = false;
static unsigned long  lastPresenceMs    = 0;

// ============================================================
// HELPERS
// ============================================================

static void setLight(bool on) {
  lightOn = on;
  digitalWrite(LIGHT_PIN, on ? LIGHT_ON_LEVEL : LIGHT_OFF_LEVEL);
  Serial.print("Light ");
  Serial.println(on ? "ON" : "OFF");
}

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println();
  Serial.println("=== ESP32Presence ===");
  Serial.print("Board : ");
  Serial.println(BOARD_NAME);
  Serial.print("PIR   : GPIO ");
  Serial.println(PIR_PIN);
  Serial.print("Light : GPIO ");
  Serial.println(LIGHT_PIN);

  pinMode(PIR_PIN, INPUT);
  pinMode(LIGHT_PIN, OUTPUT);
  setLight(false);

#ifdef ENABLE_MMWAVE
  Serial.print("Radar RX: GPIO ");
  Serial.print(RADAR_RX_PIN);
  Serial.print("  TX: GPIO ");
  Serial.println(RADAR_TX_PIN);

  RADAR_SERIAL.begin(RADAR_BAUD, SERIAL_8N1, RADAR_RX_PIN, RADAR_TX_PIN);
  if (radar.begin(RADAR_SERIAL)) {
    Serial.println("LD2410 radar ready.");
  } else {
    Serial.println("WARNING: LD2410 radar not detected — check wiring.");
  }
#endif

  lastPresenceMs = millis();

  Serial.println("Monitoring for presence...");
  Serial.println();
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  bool presenceDetected = false;

  // --- PIR sensor ---
  if (digitalRead(PIR_PIN) == HIGH) {
    presenceDetected = true;
  }

#ifdef ENABLE_MMWAVE
  // --- LD2410 mmWave radar ---
  radar.read();
  if (radar.presenceDetected()) {
    presenceDetected = true;
  }
#endif

  // --- Light control with auto-off timeout ---
  if (presenceDetected) {
    lastPresenceMs = millis();
    if (!lightOn) {
      setLight(true);
    }
  } else if (lightOn && (millis() - lastPresenceMs >= LIGHT_TIMEOUT_MS)) {
    setLight(false);
  }

  delay(50);
}
