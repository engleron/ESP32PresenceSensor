# ESP32 Presence Detection System - Improvement Specification

## Project Overview
This is an ESP32-based presence detection system using the LD2410C sensor that integrates with Universal Devices EISY/ISY/Polisy controllers to automatically control Insteon lighting based on occupancy detection.

## Current State
The project has a working file `ESP32Presence/ESP32Presence.ino` (version 1.7.1) that supports basic functionality but needs significant improvements for multi-board support, better UX, and web-based configuration management.

## Repository Structure
```
/
├── ESP32Presence/
│   └── ESP32Presence.ino (current main file - version 1.7.1)
```

## Tasks Required

### Task 1: Create Comprehensive README.md
Create a professional README.md for the GitHub repository that includes:

1. **Project Title & Description**
   - Clear explanation of what the project does
   - Key features and benefits
   - Use cases (home automation, energy savings, convenience)

2. **Hardware Requirements**
   - List of supported ESP32 boards:
     - ESP32-DevKitC-32E (tested)
     - ESP32-S3 Dev Board (tested)
   - LD2410C presence sensor specifications and where to buy
   - Optional: EISY/ISY/Polisy controller for light control
   - Wiring diagrams for each supported board (use Markdown tables or ASCII art)

3. **Software Requirements**
   - Arduino IDE version requirements (1.8.19+ or 2.x)
   - Required libraries with installation instructions:
     - Adafruit NeoPixel
     - ESP32 board package (with exact URL)
   - How to install ESP32 board support

4. **Installation Guide**
   - Step-by-step flashing instructions
   - Board selection in Arduino IDE
   - Upload speed and settings
   - First-time setup process walkthrough
   - Configuration walkthrough with screenshots (describe what to show)

5. **Features Documentation**
   - LED status indicators and their meanings:
     - Blue heartbeat: Setup mode
     - Green solid: Movement detected
     - Yellow solid: Static presence
     - Red solid: No presence
     - Red/Blue blinking: Error
   - Configurable parameters (timeout, pins, EISY settings)
   - EISY/ISY/Polisy integration details
   - Web-based configuration interface

6. **Usage Guide**
   - How to perform factory reset (5 second hold)
   - How to access web configuration after setup
   - How to change settings
   - Troubleshooting common issues

7. **API/Integration Details**
   - REST API endpoints used for EISY/ISY
   - How to find Insteon device IDs
   - HTTP vs HTTPS configuration

8. **Contributing Guidelines**
   - How to report issues
   - How to submit improvements
   - Code style guidelines

9. **License Information**
   - Suggest MIT License (open source, permissive)

10. **Changelog**
    - Version history with release notes
    - Migration guide from v1.7.1 to v2.0.0

### Task 2: Refactor and Improve ESP32Presence/ESP32Presence.ino

#### 2.1 Multi-Board Support
**Requirements:**
- Support ESP32-DevKitC-32E and ESP32-S3 boards automatically
- Auto-detect board type at compile time using preprocessor directives
- Provide clear compile-time board selection mechanism if auto-detection fails
- Display detected board type in Serial output on startup

**Pin Configurations:**

**ESP32-DevKitC-32E:**
```cpp
#define SENSOR_OUT_PIN    4   // LD2410C OUT pin
#define SENSOR_TX_PIN    18   // LD2410C TX -> ESP32 RX
#define SENSOR_RX_PIN    17   // LD2410C RX -> ESP32 TX  
#define RGB_LED_PIN      16   // Built-in WS2812 RGB LED
```

**ESP32-S3:**
```cpp
#define SENSOR_OUT_PIN    4   // LD2410C OUT pin
#define SENSOR_TX_PIN    17   // LD2410C TX -> ESP32 RX
#define SENSOR_RX_PIN    16   // LD2410C RX -> ESP32 TX
#define RGB_LED_PIN      48   // Built-in WS2812 RGB LED
```

**Implementation Approach:**
- Use `#if defined(CONFIG_IDF_TARGET_ESP32S3)` for ESP32-S3 detection
- Use `#elif defined(CONFIG_IDF_TARGET_ESP32)` for ESP32 detection
- Add `#warning` directives to show which board was detected during compilation
- Create a `BOARD_TYPE` string constant for display purposes
- Add clear documentation in code comments about supported boards

#### 2.2 User-Configurable Pins (Advanced Feature)
**Requirements:**
- Allow users to define custom pins via web interface (in "Advanced Settings" section)
- Store custom pin configuration in Preferences
- Validate pin assignments to prevent conflicts
- Provide "Restore Default Pins for [Board Type]" button
- Show current pin configuration on status page

**Implementation:**
- Create advanced settings page in web interface
- Add pin validation logic (check for duplicates, invalid pins)
- Store custom pins in Preferences with keys like "custom_sensor_out", "custom_sensor_tx", etc.
- Add boolean flag "use_custom_pins" in Preferences
- Display warning when using custom pins
- Add compile-time option to disable custom pins for simpler deployments

#### 2.3 Improved Initial Setup Mode
**Requirements:**
- Device ALWAYS starts in setup mode when no configuration is saved
- Blue blinking LED indicates setup mode (1 second on, 1 second off)
- WiFi AP name format: `Presence_XXXX` where XXXX = last 4 characters of MAC address
- Example: `Presence_A3F2`, `Presence_B8E1`
- Captive portal automatically opens configuration page on all devices (iOS, Android, Windows)
- No password required for initial setup AP
- Clear serial output indicating setup mode and SSID

**Implementation:**
- Check if Preferences contain "configured" flag on boot
- Generate unique SSID: `String ssid = "Presence_" + WiFi.macAddress().substring(12,16);`
- Remove colons from MAC, use last 4 hex characters
- Implement comprehensive captive portal DNS responses
- Add multiple captive portal detection URLs for different platforms
- Serial output: "*** SETUP MODE *** Connect to: Presence_XXXX"

#### 2.4 Enhanced Captive Portal with Comprehensive Help
**Requirements:**
- Configuration page opens automatically when connecting to setup AP
- Comprehensive help text explaining each setting
- Collapsible help sections or tooltips (using HTML details/summary tags)
- Visual indicators for required vs optional fields
- Step indicator showing configuration progress
- Real-time validation feedback before saving
- Mobile-responsive design

**Help Content for Each Section:**

**WiFi Settings Help:**
```
WiFi Network: Select your home WiFi network from the list. The device will 
connect to this network after setup.

WiFi Password: Enter the password for your WiFi network. This is stored 
securely on the device.

Tip: Make sure you're selecting a 2.4GHz network. ESP32 does NOT support 5GHz WiFi.
```

**Detection Settings Help:**
```
Light Off Delay: How long to wait after no presence is detected before turning 
off the light. Shorter times save energy but may turn off lights too quickly 
if you're sitting still. Recommended: 5 minutes.
```

**EISY/ISY Settings Help:**
```
These settings are OPTIONAL. Only fill them out if you have a Universal Devices 
EISY, Polisy, or ISY controller.

Controller IP Address: Find this in your EISY/ISY Admin Console under 
Configuration > System.

Use HTTPS: Check this ONLY if you have an EISY. ISY994 users should leave 
this unchecked.

Username: Your EISY/ISY admin username (usually "admin").

Password: Your EISY/ISY admin password.

Insteon Device Address: The address of the light you want to control.
To find this: Open EISY/ISY Admin Console > Right-click on your device > 
Copy > Address (e.g., 1A.2B.3C)
```

**Admin Password Help:**
```
IMPORTANT: Set a secure admin password. This protects your device's 
configuration page after setup.

You'll need this password to change settings later. Write it down!

If you forget it, you'll need to factory reset the device (hold BOOT for 5 seconds).
```

**Pin Configuration Help (Advanced):**
```
WARNING: Only change these if you know what you're doing!

These pins are auto-detected based on your board type. Changing them 
incorrectly can prevent the sensor from working.

Current Board: [ESP32-DevKitC-32E / ESP32-S3]
Default Pins: [list default pins]
```

#### 2.5 Enhanced Factory Reset Functionality
**Requirements:**
- Hold BOOT/RESET button for 5 seconds (changed from 3 seconds)
- Enhanced visual feedback during reset process:
  - Seconds 0-2: LED turns solid red
  - Seconds 2-4: LED blinks red/blue slowly (500ms intervals)
  - Seconds 4-5: LED blinks red/blue rapidly (200ms intervals)
  - After release at 5+ seconds: LED confirms reset with 3 quick purple flashes
  - Then: LED returns to blue heartbeat (setup mode)
- Serial output with countdown: "Reset in 5... 4... 3... 2... 1... RESETTING!"
- All settings cleared from Preferences
- Device reboots into setup mode automatically

**Implementation:**
- Update `checkResetButtonHeld()` function
- Add visual feedback states based on hold duration
- Add `serialPrintln()` countdown
- Flash purple (255, 0, 255) three times after reset confirmed
- Add 1 second delay before ESP.restart()

#### 2.6 Web-Based Configuration Management
**Requirements:**
After initial setup, device serves configuration/status page at its assigned IP address.

**Access Methods:**
- Direct IP: `http://192.168.1.XXX` (shown in serial output)
- mDNS: `http://presence.local` (if implemented)

**Security:**
- Configuration page protected by admin password (set during initial setup)
- Session management with 15-minute timeout
- Rate limiting on login attempts (max 5 attempts per minute)
- Password hashed with SHA-256 before storage
- Login page shows device name and firmware version

**Configuration Pages:**

1. **Login Page** (`/login`)
   - Admin password entry
   - "Remember me" checkbox (optional, 24hr session)
   - Firmware version display
   - Link to GitHub project

2. **Status Dashboard** (`/` or `/status`) - Main page after login
   - Current sensor status (Presence: Yes/No, Type: Moving/Static)
   - Current LED color and meaning
   - Light control status (On/Off, if configured)
   - WiFi signal strength
   - Uptime (days, hours, minutes)
   - IP address
   - Board type detected
   - Current pin configuration
   - Free heap memory
   - Last detection time
   - Link to configuration page
   - Logout button

3. **Settings Page** (`/config`)
   - WiFi Settings (change network, rescan, change password)
   - Detection Timeout setting
   - EISY/ISY Settings (all fields editable)
   - Admin password change (requires old password)
   - Advanced Settings (collapsible):
     - Custom pin configuration
     - Debug level (None, Basic, Verbose)
     - LED brightness
   - Buttons:
     - "Save Settings" (saves and shows confirmation)
     - "Save & Reboot" (saves and restarts device)
     - "Cancel" (discards changes)

4. **Factory Reset Page** (`/reset`)
   - Warning message about data loss
   - "Are you sure?" confirmation
   - Password re-entry required
   - "Factory Reset" button (red)
   - "Cancel" button

5. **Logout** (`/logout`)
   - Clears session
   - Redirects to login page

**API Endpoints (JSON responses):**
- `GET /api/status` - Returns current status as JSON
- `POST /api/config` - Updates configuration (requires auth)
- `POST /api/login` - Authenticates user, returns session token
- `POST /api/logout` - Invalidates session

**Implementation Requirements:**
- Use cookies for session management
- Generate random session ID on login
- Store session ID and expiry in memory (not Preferences)
- Check session validity on every protected page
- Auto-logout after 15 minutes inactivity
- AJAX calls for live status updates (every 2 seconds)
- Mobile-responsive CSS (works on phones)
- Clean, modern UI design
- Form validation (client and server side)

#### 2.7 Additional Improvements

**Security Enhancements:**
- Hash admin password with SHA-256 before storing in Preferences
- Store only hash, never plain text password
- Implement rate limiting: max 5 login attempts per minute
- Lock out for 1 minute after 5 failed attempts
- Optional: HTTPS support with self-signed certificate (advanced)
- Add CSRF token to forms (optional for v2.0)

**Code Quality:**
- Add comprehensive inline documentation
- Organize code into logical sections with clear headers
- Use const for all constants
- Use descriptive variable names (avoid single letters except i, j in loops)
- Add function header comments explaining parameters and return values
- Follow Arduino style guide
- Add error handling for all network operations
- Implement watchdog timer to auto-recover from hangs

**User Experience:**
- Add mDNS support: access via `http://presence.local`
- Implement OTA (Over-The-Air) firmware updates
- Add export/import configuration feature (JSON format)
- Mobile-responsive web interface
- Optional dark mode for web interface (toggle in settings)
- Show WiFi signal strength as bars/percentage
- Add "Test Connection" button for EISY/ISY settings

**Debugging Features:**
- Add debug level setting: None / Basic / Verbose
- Conditional serial output based on debug level
- Web-based log viewer showing last 100 lines (optional)
- Option to download logs as text file
- Include timestamps in all log messages (already implemented)

**Data Logging (Optional - Low Priority):**
- Optional detection event logging (enable/disable in settings)
- Store last 100 events in memory
- Statistics dashboard:
  - Detections today/this week
  - Average detection duration
  - Peak detection times
- Export logs as CSV

**Performance:**
- Optimize web page loading (minify HTML/CSS)
- Add caching headers for static content
- Reduce serial output in production mode
- Efficient memory management (avoid String concatenation in loops)

### Task 3: Code Organization and Structure

Organize improved code with clear sections:

```cpp
/*
 * ================================================================
 * SECTION: INCLUDES AND DEFINITIONS
 * ================================================================
 */

/*
 * ================================================================
 * SECTION: BOARD-SPECIFIC CONFIGURATION
 * ================================================================
 */

/*
 * ================================================================
 * SECTION: GLOBAL VARIABLES
 * ================================================================
 */

/*
 * ================================================================
 * SECTION: UTILITY FUNCTIONS
 * ================================================================
 */

/*
 * ================================================================
 * SECTION: SETUP AND LOOP
 * ================================================================
 */

/*
 * ================================================================
 * SECTION: CONFIGURATION MANAGEMENT
 * ================================================================
 */

/*
 * ================================================================
 * SECTION: WEB SERVER AND PAGES
 * ================================================================
 */

/*
 * ================================================================
 * SECTION: SENSOR FUNCTIONS
 * ================================================================
 */

/*
 * ================================================================
 * SECTION: LED CONTROL
 * ================================================================
 */

/*
 * ================================================================
 * SECTION: LIGHT CONTROL (EISY/ISY)
 * ================================================================
 */

/*
 * ================================================================
 * SECTION: SECURITY AND SESSION MANAGEMENT
 * ================================================================
 */
```

**File Structure for Repository:**
```
/
├── README.md                          (comprehensive documentation)
├── LICENSE                            (MIT License)
├── CHANGELOG.md                       (version history)
├── IMPROVEMENT_SPEC.md               (this file)
├── docs/
│   ├── WIRING.md                     (wiring diagrams and pin tables)
│   ├── CONFIGURATION.md              (detailed setup guide)
│   ├── TROUBLESHOOTING.md            (common issues and solutions)
│   └── API.md                        (REST API documentation)
├── examples/
│   └── basic_setup/
│       └── README.md                 (simple getting started guide)
└── ESP32Presence/
    └── ESP32Presence.ino             (main firmware file v2.0.0)
```

### Task 4: Documentation Files to Create

#### README.md
Must include:
- Project badge (build status if applicable)
- Quick start guide (TL;DR section)
- Detailed installation
- Features list with checkboxes
- Hardware requirements with links
- Software requirements with versions
- Wiring diagrams (ASCII art or table)
- LED status reference table
- Factory reset instructions
- FAQ section
- Contributing section
- License
- Credits/acknowledgments

#### WIRING.md
- Detailed wiring for ESP32-DevKitC-32E
- Detailed wiring for ESP32-S3
- Pinout diagrams (text-based tables)
- Photos/diagrams description (what should be shown)
- Troubleshooting wiring issues
- Custom pin configuration notes

#### CONFIGURATION.md
- First-time setup walkthrough
- WiFi configuration details
- EISY/ISY integration setup
- Finding Insteon device IDs
- Web interface usage guide
- Changing settings after setup
- Security best practices

#### TROUBLESHOOTING.md
- LED not working
- Sensor not detecting
- Can't connect to WiFi
- Can't access web interface
- Forgot admin password
- EISY/ISY not responding
- Compile errors
- Upload errors
- Board not detected

#### CHANGELOG.md
Format:
```markdown
# Changelog

## [2.0.0] - 2026-XX-XX

### Added
- Multi-board support (ESP32-DevKitC-32E and ESP32-S3)
- Unique WiFi AP name using MAC address (Presence_XXXX)
- Enhanced captive portal with comprehensive help
- Web-based configuration management
- Admin password protection
- Session management with timeout
- Factory reset extended to 5 seconds
- mDNS support (presence.local)
- Status dashboard
- Export/import configuration
- OTA updates
- Custom pin configuration (advanced)

### Changed
- Factory reset time increased from 3 to 5 seconds
- Improved visual feedback during factory reset
- Reorganized code into logical sections
- Enhanced security with password hashing

### Fixed
- HTTPS compilation error for EISY support
- Captive portal detection on various platforms

### Migration from v1.7.1
- Settings will be automatically migrated
- Admin password must be set on first access after upgrade
- Review pin configuration (auto-detected for board type)

## [1.7.1] - 2026-02-15
(previous changelog...)
```

## Version Numbering
- New version: **v2.0.0** (major refactor)
- Follow semantic versioning (MAJOR.MINOR.PATCH)
- Update version in code, README, and all documentation

## Testing Requirements
Before release, ensure:

1. **Compilation:**
   - ✅ Compiles without errors for ESP32-DevKitC-32E
   - ✅ Compiles without errors for ESP32-S3
   - ✅ Board detection works correctly
   - ✅ Appropriate warnings shown during compilation

2. **First Boot (No Config):**
   - ✅ Blue LED heartbeat visible
   - ✅ Unique SSID created (Presence_XXXX)
   - ✅ Can connect to AP from phone
   - ✅ Captive portal opens automatically on iOS
   - ✅ Captive portal opens automatically on Android
   - ✅ Captive portal opens in browser on Windows

3. **Configuration:**
   - ✅ WiFi networks list populates correctly
   - ✅ Help text displays correctly
   - ✅ Form validation works
   - ✅ Settings save successfully
   - ✅ Device connects to configured WiFi

4. **Web Interface:**
   - ✅ Login page loads
   - ✅ Correct password grants access
   - ✅ Incorrect password denied (with rate limiting)
   - ✅ Status page shows current data
   - ✅ Settings can be changed
   - ✅ Session timeout works (15 min)
   - ✅ Logout works correctly

5. **Factory Reset:**
   - ✅ Visual feedback at 0-2 seconds (solid red)
   - ✅ Visual feedback at 2-4 seconds (slow blink)
   - ✅ Visual feedback at 4-5 seconds (rapid blink)
   - ✅ Purple confirmation flash after reset
   - ✅ Returns to setup mode (blue heartbeat)
   - ✅ All settings cleared
   - ✅ Unique SSID recreated

6. **Core Features:**
   - ✅ Sensor detection works
   - ✅ LED colors correct for each state
   - ✅ EISY/ISY integration works (if configured)
   - ✅ Timeout works correctly
   - ✅ Serial logging works (timestamps present)

7. **Advanced Features:**
   - ✅ Custom pins can be configured
   - ✅ Pin validation prevents conflicts
   - ✅ mDNS works (presence.local accessible)
   - ✅ Export/import config works

## Backwards Compatibility
- **Settings Migration:** Attempt to read v1.7.1 settings if present
- **Migration Path:** If old settings detected:
  1. Load old WiFi, EISY, and timeout settings
  2. Flag as "needs_password_setup"
  3. Force admin password creation on first web access
  4. Show migration notice in serial output
- **Clean Install:** If no old settings, proceed as new device

## Code Style Guidelines
- **Indentation:** 2 spaces (Arduino standard)
- **Braces:** Opening brace on same line
- **Naming:**
  - Constants: UPPER_SNAKE_CASE
  - Variables: camelCase
  - Functions: camelCase
  - Classes: PascalCase (if used)
- **Comments:**
  - Section headers: `/* === SECTION === */`
  - Function headers: Multi-line comment explaining purpose, params, returns
  - Inline: `// Explain why, not what`
- **Line Length:** Max 100 characters where reasonable
- **Strings:** Use F() macro for string literals in Serial.print to save RAM

## Priority Order for Implementation
1. ✅ Multi-board support with auto-detection
2. ✅ Unique SSID generation (Presence_XXXX)
3. ✅ Enhanced captive portal with help text
4. ✅ Factory reset extended to 5 seconds with feedback
5. ✅ Admin password system
6. ✅ Web-based configuration pages
7. ✅ Session management
8. ✅ Status dashboard
9. 🔄 mDNS support
10. 🔄 Custom pin configuration (advanced)
11. 🔄 Export/import config
12. 🔄 OTA updates
13. 📝 Complete README.md
14. 📝 WIRING.md
15. 📝 CONFIGURATION.md
16. 📝 TROUBLESHOOTING.md
17. 📝 CHANGELOG.md

## Important Notes for Implementation

### Password Hashing
Use SHA-256 for admin password:
```cpp
// Pseudo-code example
#include <mbedtls/md.h>

String hashPassword(String password) {
  byte hash[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
  
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char*)password.c_str(), password.length());
  mbedtls_md_finish(&ctx, hash);
  mbedtls_md_free(&ctx);
  
  String hashString = "";
  for(int i = 0; i < 32; i++) {
    char str[3];
    sprintf(str, "%02x", (int)hash[i]);
    hashString += str;
  }
  return hashString;
}
```

### Session Management
```cpp
// Store active sessions
struct Session {
  String sessionId;
  unsigned long expiryTime;
};

Session currentSession;

String generateSessionId() {
  // Generate random session ID
  return String(random(0xFFFFFFFF), HEX);
}
```

### Board Detection
```cpp
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  #define BOARD_TYPE "ESP32-S3"
  #define SENSOR_OUT_PIN 4
  #define SENSOR_TX_PIN 17
  #define SENSOR_RX_PIN 16
  #define RGB_LED_PIN 48
  #warning "Compiling for ESP32-S3"
#elif defined(CONFIG_IDF_TARGET_ESP32)
  #define BOARD_TYPE "ESP32-DevKitC-32E"
  #define SENSOR_OUT_PIN 4
  #define SENSOR_TX_PIN 18
  #define SENSOR_RX_PIN 17
  #define RGB_LED_PIN 16
  #warning "Compiling for ESP32"
#else
  #error "Unsupported board! Please use ESP32 or ESP32-S3"
#endif
```

## Questions to Consider
- Should we support ESP32-C3 or other variants?
- Should we add MQTT support for Home Assistant integration?
- Should we support multiple sensors simultaneously?
- Should we add Telegram/email notifications?
- Should we implement a full REST API for external control?

## Success Criteria
The v2.0.0 release is successful when:
1. ✅ Code compiles for both supported boards
2. ✅ All documentation is complete and accurate
3. ✅ All testing requirements are met
4. ✅ Users can upgrade from v1.7.1 without losing settings
5. ✅ First-time setup is intuitive and well-documented
6. ✅ Web interface is secure and functional
7. ✅ GitHub repository is well-organized and professional

## Timeline Estimate
- Board support & basic improvements: 2-3 hours
- Web interface & security: 3-4 hours
- Testing & debugging: 2-3 hours
- Documentation: 2-3 hours
- Total: 9-13 hours of development time

## License
Recommend MIT License for maximum compatibility and adoption.
