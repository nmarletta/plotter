# Technical Constraints and Integration Requirements

## Existing Technology Stack

**Languages:** C++ (Arduino framework)

**Frameworks/Libraries:**
- **Arduino Core** - ATmega SAMD boards (MKR WiFi 1010 support)
- **U8g2** v2.36.15 - Monochrome OLED graphics library
- **SdFat - Adafruit Fork** v2.3.54 - FAT filesystem for SD card access
- **HardwareSerial** - Native UART for GRBL communication (Serial1 on pins 13/14)

**Hardware Platform:**
- **Controller:** Arduino MKR WiFi 1010 (ATSAMD21 Cortex-M0+ @ 48MHz, 32KB SRAM, 256KB Flash)
- **Display:** SSD1306 128x64 OLED (Software SPI: SCK=2, MOSI=6, RES=7, DC=8, CS=9)
- **Storage:** SD card reader (SPI, SdFat library, FAT32 format)
- **Input:** Rotary encoder with push button (custom debouncing implementation)
- **Motor Controller:** Separate Arduino running GRBL 1.1f (20170131) with servo modifications

**Communication:** Hardware UART Serial1 at 115200 baud over 10cm cable (TX=pin 14, RX=pin 13)

**Development Environment:** PlatformIO (VSCode extension), atmelsam platform

**Build System:** PlatformIO build system, Arduino framework

**Version Control:** Git (assumed)

**Key Constraints:**
- **Memory:** 32KB SRAM total (must accommodate display buffers, SD file handles, serial buffers, state machines)
- **Flash:** 256KB program storage
- **Processing:** Single-core 48MHz ARM Cortex-M0+ (no multithreading, cooperative task scheduling only)
- **GRBL Protocol:** Fixed GRBL 1.1f specification (cannot modify without reflashing motor controller Arduino)
- **Display Update Rate:** U8g2 full-screen refresh takes ~50-80ms, must not block critical operations

## Integration Approach

### Serial Communication Integration Strategy

**Current State:**
- `Serial` (USB UART) initialized at 9600 baud for debug output in main.cpp:44
- No Serial1 (hardware UART) initialization
- GCodeStreamer class written expecting HardwareSerial reference

**Integration Requirements:**
1. **Initialize Serial1 for GRBL:**
   ```cpp
   Serial1.begin(115200); // TX=pin 14, RX=pin 13 (available)
   ```
2. **Update main.cpp setup():**
   - Keep Serial for debug (optional, can disable to save RAM)
   - Add Serial1.begin(115200)
   - Pass Serial1 reference to GCodeStreamer constructor
3. **GCodeStreamer Instantiation:**
   ```cpp
   GCodeStreamer streamer(Serial1, PIN_SDCARD_CS);
   ```

**No Pin Conflicts:** Serial1 pins (13, 14) are available and not used by display or SD card.

### Display Integration Strategy

**CRITICAL: Pin Definition Corrections Required**

Current main.cpp has **incorrect pin definitions** that don't match actual hardware:

**Code (WRONG):**
```cpp
#define PIN_DISPLAY_SDA 11  // MOSI
#define PIN_DISPLAY_SCL 13  // SCK
#define PIN_DISPLAY_DC 9
#define PIN_DISPLAY_CS 8
#define PIN_DISPLAY_RES 10  // RESET
```

**Hardware (CORRECT):**
```cpp
#define PIN_DISPLAY_SDA 6   // MOSI
#define PIN_DISPLAY_SCL 2   // SCK
#define PIN_DISPLAY_DC 8
#define PIN_DISPLAY_CS 9
#define PIN_DISPLAY_RES 7   // RESET
```

**These pin definitions must be corrected before any display integration work.**

**StatePlot Display Integration:**
- Remove all Serial.println() calls from StatePlot::refreshDisplay()
- Implement U8g2 drawing sequence (clearBuffer → drawing commands → sendBuffer)
- Reuse existing display functions from display.h/cpp for consistency
- Ensure U8g2 instance accessible (extern or reference parameter)
- Throttle updates to max 10Hz during plotting to minimize overhead

### Settings State Integration

**Current State:**
- state_settings.cpp has complete UI logic
- loadSettings() function is empty stub (line 100-101)
- No GRBL communication implemented

**Integration Requirements:**
1. **Implement loadSettings():**
   - Send `$$\n` to Serial1
   - Parse GRBL response lines: `$X=value`
   - Match to settings[] array by command field
   - Update value field for each setting
   - Timeout handling (5 second max wait)
   - Display "Loading..." or progress indicator during query

2. **Implement saveSetting():**
   - On setting confirmation, send `$X=value\n` to Serial1
   - Wait for `ok` response
   - Handle error responses
   - Update local settings[] array on success

**Note:** Custom pen settings (#PEN_DOWN, #PEN_UP) deferred to Phase 2.

## Code Organization and Standards

**File Structure:**
```
include/          # Header files
src/              # Implementation files
  main.cpp        # Entry point, hardware init, state machine
  state_*.cpp     # State handlers (main, files, plot, settings, control)
  globals.cpp     # Global state variables
  FileHandler.cpp # SD card abstraction
  RotaryButton.cpp# Encoder input handling
  display.cpp     # Display functions
  gcode_streamer.cpp # G-code streaming engine
```

**Naming Conventions:**
- Classes: PascalCase (FileHandler, GCodeStreamer, StatePlot)
- Functions: camelCase (state_main, loadSettings, sendNextLine)
- Files: snake_case for states, PascalCase for classes
- Globals: camelCase (currentState, encoder, u8g2)
- Enums: PascalCase (State, GCodeStatus)

**Coding Standards:**
- Arduino C++ patterns (String class, Serial API)
- Minimize dynamic allocation (good for embedded)
- State machines via switch/case on enum
- Class-based organization for complex subsystems
- Comment complex logic (especially GRBL protocol handling)

**Integration Standard:**
- Follow existing naming/structure patterns
- Maintain enum-based state machine paradigm
- Minimize RAM usage (prefer stack allocation, const char* over String where feasible)
- Profile memory usage regularly (32KB SRAM constraint)

## Deployment and Operations

**Build Process:**
- PlatformIO compilation: `pio run`
- Upload via USB: `pio run --target upload`
- Serial monitor: `pio device monitor -b 9600`

**Testing Workflow:**
1. Fix pin definitions in main.cpp
2. Upload firmware to MKR WiFi 1010
3. Connect Serial1 (pins 13/14) to GRBL Arduino (RX/TX crossover, common ground)
4. Power both Arduinos
5. Monitor Serial (USB) for debug output (optional)
6. Test with rotary encoder navigation

**Configuration Management:**
- All settings compile-time constants
- GRBL settings stored in GRBL EEPROM (persistent)
- SD card: G-code files + .progress files (runtime state)

**Monitoring:**
- Serial (USB) debug output optional
- No telemetry or remote logging
- Error visibility via OLED only
