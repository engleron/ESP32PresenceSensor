# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [2.5.1] - 2026-03-13

Patch release fixing an Arduino linker regression introduced in `2.5.0`.

### Fixed

- **HomeKit manual-test linker error** — resolved undefined reference to `triggerHomeKitTestEvent(...)` by moving its forward declaration out of the anonymous namespace so declaration/definition linkage matches.

### Changed

- **Firmware version bumped to `2.5.1`** in `PresenceConfig.h`.

### Component Version Notes

- **Firmware:** `2.5.1`
- **Backend:** `2.5.1` (integration dispatch/linkage fix)
- **Frontend:** `2.5.1` (no UI changes in this patch)
- **Database/Storage schema:** unchanged (NVS keys unchanged)

---

## [2.5.0] - 2026-03-13

Feature release focused on easier local access, manual validation tools, and reboot reliability from the web settings flow.

### Added

- **Optional web-login mode** — admin password is now optional during setup and can be disabled later from Settings when physical BOOT access is your trust boundary.
- **Manual web test actions** on the dashboard for:
  - LED test sequence
  - Presence ON/OFF integration event tests
  - Motion ON/OFF integration event tests
- **Integration test dispatcher** in firmware that routes manual tests through the active integration (EISY/ISY, Insteon Hub, Home Assistant, and HomeKit).

### Changed

- **Firmware version bumped to `2.5.0`** in `PresenceConfig.h`.
- **Auth gating updated** so web login is bypassed when no admin password is configured.
- **Navigation behavior adjusted** to hide Logout when password auth is disabled.

### Fixed

- **Save & Reboot WiFi regression** — settings updates no longer clear stored WiFi password when the WiFi password field is left blank, preventing accidental reboot into setup mode (blinking blue).

### Component Version Notes

- **Firmware:** `2.5.0`
- **Backend:** `2.5.0` (runtime/auth/integration routing changes)
- **Frontend:** `2.5.0` (setup/settings/dashboard test controls)
- **Database/Storage schema:** unchanged (NVS keys unchanged)

---

## [2.4.6] - 2026-03-13

Patch release focused on troubleshooting guidance for ESP32-S3 cases where the device remains running but becomes network-unreachable after some uptime.

### Changed

- **Firmware version bumped to `2.4.6`** in `PresenceConfig.h`.
- **Troubleshooting workflow expanded** with a dedicated WiFi/network section for "works after reset, then stops responding" symptoms, including serial-log capture steps, power integrity checks, RSSI targets, and router isolation tests.
- **README current-version badge updated** to match the firmware version and reflect this troubleshooting-focused release.

### Component Version Notes

- **Firmware:** `2.4.6`
- **Backend:** `2.4.6` (firmware runtime and API handlers)
- **Frontend:** `2.4.6` (documentation/UI guidance set)
- **Database/Storage schema:** unchanged (NVS keys unchanged)

---

## [2.3.1] - 2026-03-04

Patch release addressing review feedback on the HA sensor entity and LD2410C UART features.

### Changed

- **HA sensor entity payload** — removed invalid `device_class: motion` attribute (only valid for `binary_sensor` entities; omitting it avoids HA warnings when the entity ID is a `sensor.*`).
- **`WiFiClientSecure` allocation** — the HTTPS path in `sendHASensorState()` now uses a stack-allocated `WiFiClientSecure` instead of heap allocation with `new`/`delete`, eliminating a potential memory fragmentation source on repeated sensor-entity POSTs.
- **UART-stale fallback state vocabulary** — when `haEntitySrc = uart` but UART data is stale, the fallback now reports `presence`/`clear` (matching the fresh-UART vocabulary) instead of `detected`/`clear`, keeping the state schema consistent for HA automations.
- **Dashboard "Sensor State" badge** — badge now derives its value from the same state-selection logic used when pushing to HA (including UART freshness and target state), so it accurately reflects what HA receives rather than just showing a binary `detected`/`clear` from the OUT pin.
- **`parseLD2410CSerial` code comment** — updated to accurately describe the implementation: 64-byte linear shifting buffer (not circular), structural-marker validation only (the LD2410C basic reporting frame does not include a checksum/CRC byte).

### Documentation

- **CONFIGURATION.md** — expanded Home Assistant section with full documentation of HA Mode (`light_control` / `sensor_entity`), Sensor Data Source (`out_pin` / `uart`), state value tables, UART attribute list, and HTTPS certificate note.

---



Latency and reliability release focused on real-time behavior, setup usability, and Insteon Hub robustness.

### Added

- **BOOT short-press service mode** — toggles LAN web/OTA access on demand without entering AP setup mode.
- **Purple service-mode heartbeat LED** — distinct visual state for run-mode service access.
- **Asynchronous integration worker task** — Insteon Hub commands now execute off the main sensor loop.
- **Proactive setup scanning** — setup mode starts WiFi scanning immediately and setup page auto-refreshes while scanning.

### Changed

- **Firmware version bumped to `2.3.0`**.
- **Setup initialization logging clarified** — explicitly reports whether setup mode or run mode is active.
- **OUT-pin presence visualization updated** — `green` for presence, `yellow` for no presence while light remains on, accelerating `red` pulse in the final minute before timeout-off, and solid `red` once the light is off.
- **Run-mode web/OTA default** remains compile-time controlled and disabled by default for low-latency operation.

### Fixed

- **`Presence_0000` setup SSID issue** — SSID suffix now derives from eFuse MAC, avoiding zeroed WiFi MAC edge cases at boot.
- **Insteon Hub command reliability** — direct-send first, clear-buffer fallback retry, improved command payload (`cmd1+cmd2`), tuned timing.
- **Main loop blocking during network actions** — eliminated for Insteon by queueing desired state to worker task.

---

## [2.2.0] - 2026-03-02

Refactor release focused on maintainability, modularity, and cleaner project configuration boundaries.

### Added

- **Dedicated compile-time configuration file** — `ESP32Presence/PresenceConfig.h` now centralizes firmware version, board pin defaults, timing constants, and default ports.
- **Modular C++ source layout** with focused `.h/.cpp` pairs:
  - `PresenceState` (global state + hardware objects)
  - `PresenceCore` (utility/security/config persistence)
  - `PresenceIntegrations` (EISY/ISY, Insteon Hub, Home Assistant dispatch)
  - `PresenceWeb` (setup portal, authenticated pages, API routes)
  - `PresenceRuntime` (LED/sensor/reset/WiFi runtime loop)

### Changed

- **Arduino sketch entrypoint simplified** — `ESP32Presence.ino` now only delegates to `appSetup()` and `appLoop()`.
- **Documentation refreshed** to describe modular architecture and centralized compile-time configuration.

### Fixed

- **Refactor boundary regression** where `loop()` could be truncated in intermediate split iterations.
- **NeoPixel duplicate-source compile conflict guidance** documented and cleaned up in workflow.

---

## [2.1.0] - 2026-03-01

Multi-integration release adding direct Insteon Hub 2, Home Assistant, and optional HomeKit control paths alongside the existing EISY/ISY integration.

### Added

- **Insteon Hub 2 integration** — Direct HTTP control of Insteon devices via the Insteon Hub 2 (Model 2245) API, with Basic auth and `cmd1+cmd2` payload format.
- **Home Assistant integration** — REST API light control via Bearer token, supporting HTTP and HTTPS (`setInsecure` for self-signed certs).
- **HomeKit integration** — Compile-time optional native HomeKit accessory support (uncomment `#define ENABLE_HOMEKIT` and install arduino-homekit-esp32 library).
- **Integration mode selector** — Single NVS key `int_mode` selects the active integration (`none` | `isy` | `insteon_hub` | `ha` | `homekit`); only one integration is active at a time.
- **Auto-migration** — Existing devices with ISY configured automatically migrate to `integrationMode = "isy"` on first load; no factory reset required.
- **Web UI integration panels** — Setup and settings pages updated with integration dropdown and per-integration settings panels for all supported modes.

### Changed

- **Firmware version bumped to `2.1.0`**.
- **`controlLight()` refactored** to dispatch to the selected integration rather than always calling the ISY path.

---

## [2.0.0] - 2026-03-01

Major release with complete refactor, multi-board support, enhanced security, and comprehensive web interface.

### Added

- **Multi-board support** — ESP32-DevKitC-32E and ESP32-S3 auto-detected at compile time using preprocessor directives
- **Unique WiFi AP name** — Setup network uses `Presence_XXXX` format (last 4 chars of MAC address) instead of generic name
- **Enhanced captive portal** — Comprehensive help text for every setting, collapsible sections, mobile-responsive design
- **Admin password protection** — Web interface secured with password, hashed using SHA-256 before storage
- **Session management** — 15-minute inactivity timeout, session cookie, automatic logout
- **Rate limiting** — Maximum 5 login attempts per minute; 1-minute lockout after limit exceeded
- **Status dashboard** (`/`) — Live sensor data, uptime, WiFi signal, memory, board info, updated every 2 seconds
- **Settings page** (`/config`) — All configuration editable via browser, no re-flashing required
- **Factory reset web page** (`/reset`) — Password-protected web-based reset option
- **API endpoints** — JSON API for `/api/status`, `/api/config`, `/api/login`, `/api/logout`
- **mDNS support** — Access device via `http://presence.local`
- **OTA firmware updates** — Update firmware over WiFi via web interface
- **Export/import configuration** — JSON format for backup and restore
- **Custom pin configuration** — Advanced setting for non-standard wiring (web interface)
- **LED brightness control** — Adjustable via web interface
- **Debug level setting** — None / Basic / Verbose serial output
- **Watchdog timer** — Auto-recovery from hangs (8-second timeout)
- **Visual factory reset feedback** — Staged LED colors during 5-second hold
- **Board type display** — Shown in Serial output on startup and in Status dashboard
- **Comprehensive documentation** — README, WIRING, CONFIGURATION, TROUBLESHOOTING guides

### Changed

- **Factory reset time increased from 3 to 5 seconds** — Reduces accidental resets
- **Factory reset visual feedback improved** — Three stages: solid red → slow blink → rapid blink → purple confirm
- **Setup SSID changed** from `ESP32-Presence-Setup` to `Presence_XXXX` (unique per device)
- **Code reorganized** into labeled sections: Includes, Board Config, Global Variables, Utilities, Setup/Loop, Configuration, Web Server, Sensor, LED, Light Control, Security/Session
- **Improved Serial output** with consistent timestamps, board info on startup
- **Web interface redesigned** — Cleaner layout, mobile-responsive, help text throughout
- **Password storage** — Now hashed with SHA-256; plain-text never stored

### Fixed

- **HTTPS compilation error** — Improved WiFiClientSecure handling for EISY HTTPS support
- **Captive portal detection** — Added more detection URLs for better compatibility across iOS, Android, and Windows
- **Memory leaks** — WiFiClientSecure objects properly managed
- **String concatenation in loops** — Replaced with more efficient patterns

### Migration from v1.7.1

Settings will be automatically detected and migrated on first boot:
1. Existing WiFi, EISY, and timeout settings are loaded from v1.7.1 preferences
2. Device flags as "needs_password_setup" — prompts for admin password creation on first web access
3. Migration notice shown in Serial output
4. Re-save settings through web interface to complete migration

**Breaking changes:**
- Admin password is now required (must be set on first access after upgrade)
- Session management is new — you'll need to log in each time you access the web interface
- Pin assignments auto-detected — verify your board is selected correctly in Arduino IDE

---

## [1.7.1] - 2026-02-15

### Fixed

- HTTPS compilation error — Added `WiFiClientSecure` for HTTPS support
- Fixed `setInsecure()` compilation error with newer ESP32 Arduino core versions

---

## [1.7.0] - 2026-02-10

### Added

- Explicit support for Universal Devices EISY
- HTTPS support for EISY connection
- Compatibility with ISY994, ISY994i, EISY, and Polisy controllers

---

## [1.6.x] - 2026-01-xx

Initial public releases with basic functionality:
- WiFi captive portal setup
- LD2410C presence detection via OUT pin
- EISY/ISY HTTP light control
- RGB LED status indicators
- UART communication with sensor
- Preferences-based configuration storage
- Factory reset (3-second BOOT button hold)
