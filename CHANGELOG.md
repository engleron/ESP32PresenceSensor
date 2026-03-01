# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
