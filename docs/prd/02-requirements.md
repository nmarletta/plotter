# Functional Requirements

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

# Non-Functional Requirements

**NFR1:** The system shall maintain responsive UI with rotary encoder input latency under 50ms during active plotting operations.

**NFR2:** The system shall achieve 95% plot completion rate without manual intervention or failure over 50+ plot sample size (per success metrics in Project Brief).

**NFR3:** The system shall successfully resume 90% of interrupted plots after power loss at various completion stages (per success metrics in Project Brief).

**NFR4:** The system shall complete power-on to plot-start workflow in under 30 seconds (per success metrics in Project Brief).

**NFR5:** The system shall handle multi-megabyte G-code files without memory exhaustion, using line-by-line streaming approach.

**NFR6:** The system shall maintain stable serial communication with GRBL over 10cm cable length at 115200 baud without data corruption.

**NFR7:** The system shall persist progress state atomically to SD card to prevent corruption during unexpected power loss.

**NFR8:** The system shall operate within Arduino MKR WiFi 1010 memory constraints (32KB SRAM, 256KB Flash).

# Compatibility Requirements

**CR1: GRBL Protocol Compatibility** - System shall maintain full compatibility with GRBL 1.1f protocol specification including status reports, error codes, $ settings commands, and flow control mechanisms.

**CR2: Existing Code Integration** - New serial communication and display integration shall work with existing FileHandler, RotaryButton, and state machine architecture without breaking current functionality.

**CR3: SD Card Format Compatibility** - System shall work with FAT32-formatted SD cards and support .gcode and .nc file extensions as currently implemented.

**CR4: Hardware Pin Assignment Consistency** - Serial communication implementation shall respect existing pin assignments for display (SPI pins 8-13), SD card (SPI CS pin 14), and encoder (pins 3-5), requiring Serial1 use on available UART pins.
