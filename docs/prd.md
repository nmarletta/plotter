# Pen Plotter Controller - Brownfield Enhancement PRD

## Intro Project Analysis and Context

### Analysis Source

**IDE-Based Fresh Analysis** - Conducted comprehensive review of existing PlatformIO project structure, source code, and Project Brief documentation (docs/brief.md).

### Existing Project Overview

#### Current Project State

The **Pen Plotter Controller** is a partially implemented Arduino-based standalone controller for a DIY pen plotter. The system uses an Arduino MKR WiFi 1010 as the control unit, communicating with a separate Arduino running modified GRBL 1.1f firmware for motor and servo control.

**Current Implementation Status:**
- **Hardware Platform:** Arduino MKR WiFi 1010, OLED display (U8g2), SD card reader (SdFat), rotary encoder
- **State Machine:** Enum-based architecture with 5 states (MAIN, CONTROL, SETTINGS, FILES, PLOT)
- **Display System:** U8g2 library integrated, displays 3 menu items at once
- **File Handling:** SD card reading operational, file browser working
- **Rotary Encoder:** Debouncing implemented and functional
- **Core Streaming Logic:** GCodeStreamer class written with complete streaming, pause/resume, and progress save/resume functionality
- **Plot State Management:** StatePlot class written with menu and state handling

**Critical Assessment:** Substantial code exists but is **UNTESTED and UNVALIDATED**. The GCodeStreamer and StatePlot implementations represent significant development effort but have never been executed against actual GRBL hardware. Implementation correctness, architectural soundness, and integration patterns remain unverified.

### Available Documentation Analysis

#### Available Documentation

- ✅ **Project Brief** (docs/brief.md) - Comprehensive strategic document covering problem statement, solution approach, MVP scope, technical considerations, risks, and next steps
- ✅ **Tech Stack Documentation** - Captured in Project Brief and platformio.ini (Arduino framework, U8g2, SdFat)
- ✅ **Reference Guide** - Instructables tutorial for GRBL servo modification for pen up/down
- ❌ **Source Tree/Architecture** - Code structure visible but no formal documentation
- ❌ **Coding Standards** - No documented conventions (informal C++ Arduino patterns in use)
- ❌ **API Documentation** - No documentation of class interfaces or module interactions
- ❌ **Testing Documentation** - No test plans, test cases, or validation procedures
- ❌ **Integration Documentation** - StatePlot ↔ GCodeStreamer interaction patterns not documented

**Documentation Gap Assessment:** The Project Brief provides excellent strategic foundation, but technical implementation documentation is minimal. The lack of architecture documentation and testing procedures represents significant risk given the untested code base.

### Enhancement Scope Definition

#### Enhancement Type

- ✅ **New Feature Addition** - Serial communication with GRBL, plot execution, settings management, jog control
- ✅ **Major Feature Modification** - Plot state requires display integration (currently Serial debug output)
- ✅ **Bug Fix and Stability Improvements** - Validation and correction of untested streaming logic

#### Enhancement Description

Complete the pen plotter controller MVP by validating and integrating existing drafted code (GCodeStreamer, StatePlot), implementing missing features (serial communication setup, display integration, settings state, control state), and testing the complete system end-to-end against GRBL hardware. Transform the partially implemented codebase into a production-ready standalone plotting system.

#### Impact Assessment

- ✅ **Moderate Impact** (some existing code changes) - GCodeStreamer and StatePlot may require refactoring based on testing
- ✅ **Significant Impact** (substantial existing code changes) - Serial communication changes affect main.cpp and all GRBL-interacting components

**Risk Note:** The untested GCodeStreamer represents architectural uncertainty. Testing may reveal design flaws requiring substantial refactoring of both GCodeStreamer and StatePlot classes.

### Goals and Background Context

#### Goals

- **Validate existing streaming architecture** - Test GCodeStreamer and StatePlot integration against GRBL hardware to confirm correctness
- **Complete serial communication setup** - Configure hardware serial (Serial1 at 115200 baud) for GRBL communication
- **Integrate plot state with OLED display** - Replace Serial debug output with U8g2 display calls showing progress, status, and menu
- **Implement settings management** - Enable viewing and modifying GRBL $settings from controller interface
- **Implement jog control** - Provide manual X/Y axis positioning through rotary encoder interface
- **Achieve reliable untethered plotting** - Execute complete plots from SD card without computer connection, with 95%+ success rate
- **Enable plot resume after power loss** - Validate progress save/resume functionality works correctly in real conditions

#### Background Context

The plotter hardware is functional but requires a connected computer for operation, creating workflow friction and limiting utility. Partial controller firmware exists with sophisticated streaming logic written but never tested. The primary challenge is validation and completion: confirming the drafted GCodeStreamer architecture works correctly, integrating it with the display system, and filling gaps in settings and jog functionality. Success depends on testing revealing manageable issues in the untested code rather than fundamental architectural problems requiring rewrites.

This enhancement transforms an underutilized plotter into a standalone appliance while navigating the uncertainty of untested implementation code.

#### Change Log

| Change | Date | Version | Description | Author |
|--------|------|---------|-------------|--------|
| Initial PRD | 2025-11-25 | 1.0 | Created brownfield PRD from Project Brief and code analysis | PM |

## Requirements

### Functional Requirements

**FR1:** The system shall establish serial communication with GRBL Arduino at 115200 baud on hardware Serial1 port, verifying connection readiness before allowing plot operations.

**FR2:** The system shall stream G-code files line-by-line from SD card to GRBL, implementing wait-for-ok flow control to prevent buffer overflow.

**FR3:** The system shall display real-time plot progress on OLED showing: filename, percentage complete, and GRBL status (Running/Paused/Error/Completed).

**FR4:** The system shall capture and display GRBL error and alarm messages on the OLED screen in human-readable format, pausing streaming when errors occur.

**FR5:** The system shall persist plot progress (filename, line number, timestamp) to SD card at completion of each G-code line, or at 3-second intervals if per-line saving introduces unacceptable latency.

**FR6:** The system shall detect incomplete plots on startup (via .progress files) and offer user option to resume from saved line number or start fresh.

**FR7:** The system shall provide pause/resume functionality during plotting via rotary encoder menu, maintaining plot state and GRBL synchronization across pause cycles.

**FR8:** The system shall provide cancel functionality with confirmation prompt, removing progress file and returning to file browser on confirmed cancellation.

**FR9:** The system shall implement manual axis control providing: (a) X/Y axis jog via rotary encoder for direction/distance, (b) homing cycle initiation ($H command to GRBL).

**FR10:** The system shall query and display current GRBL $ settings via rotary encoder menu interface.

**FR11:** The system shall allow modification of GRBL settings values through rotary encoder interface with min/max/step validation per setting definition, sending $X=value commands to persist changes. Settings shall include:
- **GRBL Core Settings** ($0-$27): Step pulse, idle delay, port inversion, limits, homing parameters
- **Axis Configuration** ($100-$131): X/Y steps/mm, max rates, acceleration, max travel
- **Custom Pen Settings** (#PEN_DOWN, #PEN_UP): Servo position values (0-31 range)

**FR12:** The system shall filter blank lines and comment lines (starting with ';' or '(') during G-code streaming, skipping them without sending to GRBL.

**FR13:** The system shall calculate total line count for selected G-code file at plot initiation to enable percentage progress calculation.

**FR14:** The system shall maintain existing file browser, display, and encoder functionality without regression during integration of new features.

**FR15:** The system shall query current GRBL settings on entering settings state via $$ command, parsing responses to populate the 27 settings defined in state_settings.cpp with current values.

### Non-Functional Requirements

**NFR1:** The system shall maintain responsive UI with rotary encoder input latency under 50ms during active plotting operations.

**NFR2:** The system shall achieve 95% plot completion rate without manual intervention or failure over 50+ plot sample size (per success metrics in Project Brief).

**NFR3:** The system shall successfully resume 90% of interrupted plots after power loss at various completion stages (per success metrics in Project Brief).

**NFR4:** The system shall complete power-on to plot-start workflow in under 30 seconds (per success metrics in Project Brief).

**NFR5:** The system shall handle multi-megabyte G-code files without memory exhaustion, using line-by-line streaming approach.

**NFR6:** The system shall maintain stable serial communication with GRBL over 10cm cable length at 115200 baud without data corruption.

**NFR7:** The system shall persist progress state atomically to SD card to prevent corruption during unexpected power loss.

**NFR8:** The system shall operate within Arduino MKR WiFi 1010 memory constraints (32KB SRAM, 256KB Flash).

### Compatibility Requirements

**CR1: GRBL Protocol Compatibility** - System shall maintain full compatibility with GRBL 1.1f protocol specification including status reports, error codes, $ settings commands, and flow control mechanisms.

**CR2: Existing Code Integration** - New serial communication and display integration shall work with existing FileHandler, RotaryButton, and state machine architecture without breaking current functionality.

**CR3: SD Card Format Compatibility** - System shall work with FAT32-formatted SD cards and support .gcode and .nc file extensions as currently implemented.

**CR4: Hardware Pin Assignment Consistency** - Serial communication implementation shall respect existing pin assignments for display (SPI pins 8-13), SD card (SPI CS pin 14), and encoder (pins 3-5), requiring Serial1 use on available UART pins.

## User Interface Enhancement Goals

### Integration with Existing UI

The controller uses a **text-based OLED interface** (128x64 pixels, U8g2 library) with **rotary encoder navigation** (rotate to select, click to confirm). The existing UI pattern displays **3 menu items at once** with visual selection indicator, scrolling through longer lists. This 3-item vertical limit is a hard constraint validated through display testing.

**New UI elements must integrate with this established pattern:**

- **Plot Progress Screen** - Replace StatePlot's Serial.println() debug output with U8g2 display calls. Show compact status information (filename truncated if needed, percentage, status text, 2-item menu: Pause/Continue, Cancel) fitting the 3-item display paradigm.

- **Settings Edit Screen** - Existing state_settings.cpp already implements the encoder interaction pattern (list navigation → select setting → edit value → confirm). Display integration needed to replace displayList() and displayChangeSettings() with U8g2 rendering.

- **Control/Jog Screen** - New screen following existing pattern: 3-item menu (X Jog, Y Jog, Home), with sub-state for jog distance/direction selection using encoder rotation for incremental movement.

- **Error Display** - Simple pause-and-display approach: show error message with Continue/Cancel options, allowing user to manually resolve hardware issues.

**Design Consistency:** All new screens use the same U8g2 font, layout spacing, and selection indicators as existing file browser and main menu.

### Modified/New Screens and Views

**1. Plot Progress Screen** (StatePlot - **Modified**)
   - **Current:** Serial debug output only
   - **New:** OLED display showing:
     - Line 1: Filename (truncated with "..." if >21 chars)
     - Line 2: "[XX%] - [Status]" (integer percentage, e.g., "42% - Running")
     - Line 3-4: Menu items (Pause/Continue, Cancel) with selection indicator
     - Optional: Cancel confirmation overlay

**2. Settings List Screen** (state_settings - **Modified**)
   - **Current:** Has UI logic but incomplete display integration
   - **New:** OLED showing:
     - 3 settings at once from 27-item list
     - Selection indicator on current setting
     - "<- Back" as first item

**3. Settings Edit Screen** (state_settings - **Modified**)
   - **Current:** Logic exists, display integration needed
   - **New:** OLED showing:
     - Setting name
     - Current value with unit
     - Min/max range indicator
     - Instructions (rotate to change, click to confirm)

**4. Control/Jog Screen** (state_control - **New**)
   - **New:** OLED showing:
     - Menu: X Jog, Y Jog, Home (3 items)
     - In jog mode: Axis name, position feedback, increment indicator

**5. Error/Alarm Display** (**New - Simplified for MVP**)
   - **New:** OLED overlay/modal showing:
     - "Error: [code/message]"
     - Menu: "Continue" (attempt resume after user fixes issue), "Cancel"
     - User manually resolves hardware issues before pressing Continue
     - **Phase 2 Enhancement:** Error type differentiation, automatic recovery commands ($X for alarms), error logging to SD card

### UI Consistency Requirements

**UC1: Display Rendering Pattern** - All screens shall use U8g2's page buffer mode with consistent font (u8g2_font_ncenB08 or equivalent), 8-pixel line spacing, and full-screen clear before redraw.

**UC2: 3-Item Display Paradigm** - List-based screens (settings, control menu) shall show maximum 3 items simultaneously with "> " indicator for selected item, scrolling to reveal additional items. This is a hard constraint based on 128x64 pixel display testing.

**UC3: Encoder Interaction Consistency** - All screens shall respond to encoder rotation (navigate/adjust) and click (select/confirm) with identical timing and debounce behavior as existing file browser.

**UC4: Text Truncation Strategy** - Long text (filenames, setting names) shall truncate with "..." suffix when exceeding display width, ensuring critical information remains visible.

**UC5: Status Text Standardization** - GRBL status shall display using consistent labels: "Idle", "Running", "Paused", "Error", "Completed", "Canceled" matching GCodeStatus enum names. Progress percentage displayed as integer (e.g., "42%" not "42.5%").

**UC6: Non-Blocking Updates** - Display updates during plotting shall not introduce perceptible latency or block G-code streaming, using efficient U8g2 draw operations and update throttling if needed.

## Technical Constraints and Integration Requirements

### Existing Technology Stack

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

### Integration Approach

#### Serial Communication Integration Strategy

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

#### Display Integration Strategy

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

#### Settings State Integration

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

### Code Organization and Standards

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

### Deployment and Operations

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

### Risk Assessment and Mitigation

**Technical Risks:**

**RISK 1: Pin Definition Mismatch (HIGH - IMMEDIATE)**
- **Issue:** Code has incorrect display pin definitions (don't match hardware)
- **Impact:** Display may not work, potential hardware damage if pins conflict
- **Mitigation:** Correct pin definitions in main.cpp as first task before any testing

**RISK 2: GCodeStreamer Untested Architecture (MEDIUM-HIGH)**
- **Issue:** Sophisticated streaming logic never executed, correctness unknown
- **Impact:** May discover fundamental design flaws requiring significant refactoring
- **Mitigation:**
  - Incremental testing: start with 10-line G-code file
  - Test pause/resume before progress save/resume
  - Mock Serial offline to unit test response parsing
  - Budget refactoring time in story estimates

**RISK 3: Memory Exhaustion (MEDIUM)**
- **Issue:** 32KB SRAM must hold display buffer (~1KB), SD buffers, String objects, stack
- **Impact:** Crashes, corrupted state, unpredictable behavior
- **Mitigation:**
  - Profile memory usage early and often
  - Minimize String usage (prefer const char*, F() macro)
  - Close SD files promptly
  - Test with large G-code files

**RISK 4: Progress Save Latency (MEDIUM)**
- **Issue:** Per-line SD writes may delay streaming
- **Impact:** GRBL buffer starvation, motion artifacts
- **Mitigation:**
  - Measure SD write time (target <10ms)
  - Fall back to 3-second intervals if excessive
  - Consider write buffering

**RISK 5: GRBL Response Parsing Robustness (LOW-MEDIUM)**
- **Issue:** handleGrblResponse() uses substring matching ("ok", "error")
- **Impact:** False positives if unexpected messages contain substrings
- **Mitigation:**
  - Test with actual GRBL 1.1f responses
  - Harden parsing (exact line matching)
  - Handle status reports separately

**Integration Risks:**

**RISK 6: StatePlot ↔ GCodeStreamer State Sync (MEDIUM)**
- **Issue:** Independent state machines; synchronization bugs possible
- **Impact:** UI shows incorrect status, actions don't match streamer state
- **Mitigation:**
  - Review state transition logic carefully
  - Add debug assertions for state mismatches
  - Test all state transitions explicitly

**RISK 7: Settings Load Timeout (LOW)**
- **Issue:** GRBL may not respond to $$ command
- **Impact:** Settings screen hangs
- **Mitigation:**
  - 5-second timeout in loadSettings()
  - Display "Connection Error" and return to main menu
  - Allow manual retry
