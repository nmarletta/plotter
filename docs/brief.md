# Project Brief: Pen Plotter Controller

## Executive Summary

The Pen Plotter Controller is a standalone Arduino-based system that enables DIY pen plotter operation without requiring a connected computer. Built on an Arduino Nano WiFi MKR with rotary encoder interface, OLED display, and SD card reader, the controller reads G-code files from an SD card and transmits plotting commands via serial communication to a GRBL-based Arduino motor controller. The system transforms the pen plotter from a tethered peripheral into an independent device, allowing makers to queue and execute plots by simply loading files onto an SD card and selecting them through an intuitive rotary encoder menu interface.

## Problem Statement

**Current State & Pain Points:**
Operating a DIY pen plotter currently requires a computer to remain connected and active throughout the entire plotting process, which can range from minutes to hours depending on design complexity. This creates several pain points:
- **Workspace constraint:** The plotter must be positioned within cable reach of a computer
- **Resource lock:** A computer is tied up for the duration of each plot, preventing other work
- **Reliability risk:** Any computer interruption (sleep mode, software crash, accidental disconnect) ruins the plot in progress
- **Friction:** Each plot requires booting a computer, connecting cables, and managing software, adding overhead that discourages spontaneous creative work

**Impact:**
This dependency transforms what should be a simple, enjoyable creative tool into a logistically complex setup. For a maker working on multiple plots per session, the overhead of computer management can equal or exceed actual plotting time. The inability to queue work and walk away means the plotter sits idle during times when the maker is unavailable but the plotter could be productive.

**Why Existing Solutions Fall Short:**
- **Commercial standalone controllers:** Cost $200-500, pricing out hobby users
- **Raspberry Pi solutions:** Require additional complexity (OS management, network configuration, web interfaces) and higher power consumption
- **OctoPrint/similar:** Designed for 3D printers, overkill for simple G-code streaming with excessive setup complexity

**Urgency:**
The plotter is currently functional but underutilized due to the connection burden. With the controller hardware already acquired (Arduino Nano WiFi MKR) and partial code complete, this is the optimal time to finish the system and unlock the plotter's full utility.

## Proposed Solution

**Core Concept:**
The Pen Plotter Controller leverages the Arduino ecosystem to create a purpose-built, standalone G-code streaming device. The system uses an Arduino Nano WiFi MKR as the control unit, interfacing with a rotary encoder for input, an OLED display (U8g2 library) for visual feedback, and an SD card reader (SdFat library) for file storage. The controller communicates via serial (RX/TX) with a separate Arduino running modified GRBL firmware that handles motor control and pen servo operations.

**Key Differentiators:**
- **Single-purpose design:** Unlike general-purpose solutions (Raspberry Pi, commercial controllers), this system does exactly one thing exceptionally well—stream G-code from SD card to GRBL
- **Minimal resource footprint:** Arduino-level power consumption and complexity versus Linux-based alternatives
- **Physical interface priority:** Rotary encoder + display provides tactile, immediate control without network configuration or web interfaces
- **Robust operation:** Displays GRBL error messages directly on screen and persists plot progress to SD card, enabling resume after power interruption
- **DIY-friendly architecture:** Uses standard maker components and libraries, making it debuggable and modifiable
- **Cost-effective:** Total BOM under $50 versus $200-500 commercial alternatives

**Why This Will Succeed:**
The solution succeeds by embracing constraints rather than fighting them. Instead of trying to replicate full computer functionality, it focuses exclusively on the plotting workflow: select file → start plot → monitor progress → handle errors gracefully. The Arduino platform provides sufficient capability for this focused task while eliminating the complexity overhead that makes alternatives frustrating.

The error display and resume capabilities address the two most common failure modes (GRBL communication issues and power interruptions), making the system genuinely workshop-ready rather than a fair-weather prototype.

The existing codebase already demonstrates viability—the display, encoder, and SD card components work individually. The remaining work is primarily integration (serial communication with GRBL, progress tracking) and state management refinement, not fundamental technical unknowns.

**High-Level Vision:**
A plotter controller that feels like operating an appliance, not managing a computer. Turn it on, spin the dial to select a file, press to start, walk away. The screen shows progress and any errors GRBL reports. If power is lost, the controller remembers where it was and offers to resume. When it's done, load another file and go again. No boot sequences, no login prompts—just immediate, reliable plotting that survives real-world workshop conditions.

## Target Users

### Primary User Segment: DIY Maker / Creative Technologist

**Demographic Profile:**
Individual maker with electronics and programming capability, operating a home workshop environment. Comfortable with Arduino-level development and troubleshooting. Creates pen plotter art as a hobby or small-scale creative practice rather than commercial production.

**Current Behaviors & Workflows:**
- Generates plotter designs using vector software (Inkscape, Illustrator, Processing, etc.)
- Converts designs to G-code using post-processing tools
- Currently manages plotting through direct computer connection with G-code sender software
- Works on multiple plots in series, often during focused creative sessions
- May leave plots running while doing other workshop activities

**Specific Needs & Pain Points:**
- **Workspace flexibility:** Wants to position plotter optimally without computer proximity constraint
- **Computer freedom:** Needs to use computer for other work while plots run
- **Reliability:** Cannot afford multi-hour plot failures due to computer issues
- **Simplicity:** Values straightforward operation over feature richness
- **Visibility:** Wants to see what's happening (progress, errors) without connecting external devices
- **Workshop durability:** Needs system that handles power fluctuations and interruptions gracefully

**Goals They're Trying to Achieve:**
- **Primary goal:** Reduce friction in the plot execution process to encourage more frequent creative output
- **Secondary goal:** Improve workshop workflow by decoupling plotting from computer availability
- **Aspirational goal:** Achieve "just works" reliability where plotting becomes a background task rather than an attended operation

## Goals & Success Metrics

### Business Objectives
- **Complete functional controller within current development timeline:** Finish implementation to start using the plotter productively
- **Achieve reliable operation:** System handles 95%+ of plots without intervention or failure
- **Reduce plotting workflow overhead:** Cut setup/management time from 5-10 minutes per plot to under 30 seconds
- **Enable untethered operation:** 100% of plots can be executed without computer connection

### User Success Metrics
- **Plot completion rate:** Percentage of initiated plots that complete successfully
- **Resume success rate:** Percentage of interrupted plots that successfully resume from saved progress
- **Error visibility:** User can identify and understand GRBL errors from display without needing serial monitor
- **Time to plot:** Duration from "deciding to plot" to "plot starts" (measures workflow friction)
- **Actual usage increase:** Number of plots executed per week/month compared to pre-controller baseline

### Key Performance Indicators (KPIs)
- **System uptime reliability:** Target 95%+ successful plot completions without manual intervention or failure. Measured by tracking completed vs. failed plots over 50+ plot sample size.
- **Setup time reduction:** Target <30 seconds from power-on to plot start. Measured by timing the complete workflow: power on → file select → confirm → GRBL connection → first line sent.
- **Resume functionality success:** Target 90%+ successful resumes after power loss. Measured by intentionally interrupting plots at various completion percentages and verifying accurate resume.
- **Error communication clarity:** Target 100% of GRBL errors displayed on screen within 2 seconds of occurrence. Measured by triggering known GRBL error conditions and verifying display.
- **Computer independence:** Target 100% of plots executable without computer. Measured by proportion of plots that require no computer interaction after file transfer to SD card.

## MVP Scope

### Core Features (Must Have)

- **File Browser & Selection:** Navigate SD card file structure using rotary encoder, displaying 3 files at a time with clear visual indication of current selection. Support .gcode and .nc file extensions.

- **Serial Communication with GRBL:** Establish and maintain serial connection with GRBL Arduino at appropriate baud rate. Handle GRBL's flow control protocol (character counting or simple protocol) to prevent buffer overflow.

- **G-code Streaming:** Read selected G-code file from SD card and stream commands line-by-line to GRBL, respecting GRBL's ready state and buffer capacity.

- **Progress Display:** Show real-time plot progress on OLED display including: current line number, total lines, percentage complete, estimated time remaining (based on lines processed), and current GRBL state.

- **Error Display & Handling:** Capture and display GRBL error/alarm messages on screen in human-readable format. Pause streaming on error and provide option to abort or attempt resume (where applicable).

- **Plot Resume After Power Loss:** Persist plot state to SD card (filename, line number, timestamp) at regular intervals during plotting. On startup, detect incomplete plots and offer resume option. Resume by seeking to saved line and continuing stream.

- **Manual Axis Control (Jog Mode):** Step-based jog interface: select axis (X or Y) via menu, then rotate encoder to jog in that direction with configurable step size. Display current position feedback from GRBL status reports.

- **GRBL Settings Access:** Menu option to view and modify GRBL $ settings. Display current setting values retrieved from GRBL and allow editing via rotary encoder interface. Send setting updates as $-commands.

- **Basic Menu System:** Hierarchical menu structure with states for: Home (file list), Plot (during plotting), Jog (manual control), Settings (GRBL config), with intuitive navigation via rotary encoder click/rotate.

### Out of Scope for MVP

- WiFi file transfer (future enhancement)
- Web interface or remote control
- Multiple file queueing
- Plot preview visualization
- G-code editing or transformation
- Advanced statistics or logging
- Multi-plotter support
- Touchscreen interface
- Audio feedback or notifications
- Custom GRBL firmware modifications

### MVP Success Criteria

The MVP is considered successful and ready for production use when:
1. **Core workflow functions end-to-end:** Can select a file, start plot, see progress, complete plot without computer intervention
2. **Error handling works:** GRBL errors appear on display and system responds appropriately (pause/abort)
3. **Resume survives power loss:** At least 3 test interruptions at different plot stages (early, mid, late) successfully resume
4. **Menu navigation is intuitive:** Can navigate to any function (file select, jog, settings) within 10 seconds without reference documentation
5. **Serial communication is stable:** 10 consecutive plots complete without communication failures or hangs
6. **Code is maintainable:** Structure is clear enough that you can modify/debug it 6 months later without complete relearning

## Post-MVP Vision

### Phase 2 Features

**Adjustable Pen Servo Positions**
Modify GRBL firmware and controller to expose pen up/down servo positions as adjustable parameters (similar to GRBL $ settings). Enable real-time adjustment from the controller interface to accommodate different pen types, paper thicknesses, and mounting configurations without reflashing firmware or connecting to a computer. Store position presets for frequently used pen configurations.

**Plot Queue Management**
Support selecting multiple files for sequential plotting. Display queue status and enable "batch plotting" mode for overnight or unattended multi-plot sessions.

**Enhanced Progress Visualization**
Display estimated completion time based on actual plotting speed rather than line count. Show more detailed real-time status (current coordinates, feed rate, GRBL buffer state).

**Pause/Resume During Plot**
User-initiated pause (not just error-triggered) to allow pen changes, workspace adjustments, or inspection. Resume gracefully from paused state while maintaining position accuracy.

### Long-term Vision

Evolve the controller into a complete standalone plotter workstation that handles all configuration, monitoring, and control through the physical interface. The system becomes increasingly intelligent about plotting workflow—remembering pen configurations, detecting common issues, and optimizing for reliability over time.

Key aspirational capabilities:
- **Zero-computer workflow:** Design on computer, transfer wirelessly (WiFi file transfer), everything else happens at the plotter
- **Pen profile management:** Save/load complete configurations for different pens (servo positions, speed settings, acceleration profiles)
- **Self-diagnosing:** System detects and reports common failure modes (pen not lowering, position errors) with guided troubleshooting
- **Plot history & statistics:** Track plotting patterns, success rates, and typical durations for better time estimation

### Expansion Opportunities

**Advanced Material Handling**
Support for different paper surfaces and sizes with stored configuration profiles. Coordinate plot origin with material positioning.

**G-code Preprocessing**
Built-in transformations: scaling, rotation, origin adjustment, speed modification. Apply changes to queued files without returning to design software.

**Community Sharing (Optional)**
If interest develops, document the controller design for others building similar systems. Share the approach to pen position configuration and resume functionality as reference implementations.

**Networked Monitoring**
Simple status API or notification system to check plot progress remotely without full web interface complexity.

## Technical Considerations

### Platform Requirements

- **Target Platforms:** Embedded system (Arduino Nano WiFi MKR controller + separate Arduino running GRBL)
- **Development Environment:** PlatformIO (currently in use)
- **Display Requirements:** OLED display driven by U8g2 library, sufficient resolution for 3-line menu display plus status information
- **Storage Requirements:** SD card reader with FAT32 support for G-code file storage and plot state persistence
- **Communication Requirements:** Hardware serial (RX/TX) at standard GRBL baud rate (typically 115200), with sufficient buffer management for reliable streaming
- **Performance Requirements:** Real-time responsiveness for rotary encoder input (<50ms), stable G-code streaming without buffer starvation, SD card read performance adequate for line-by-line G-code parsing

### Technology Preferences

- **Controller Platform:** Arduino Nano WiFi MKR (WiFi capability reserved for future use, sufficient GPIO and processing power for current requirements)
- **Motor Control Platform:** Separate Arduino running modified GRBL 1.1f (20170131) firmware (based on Instructables servo modification for pen up/down control)
- **Display Library:** U8g2 (already implemented, working well for current 3-item menu display)
- **SD Card Library:** SdFat (already implemented, reliable FAT32 support)
- **Programming Language:** C++ (Arduino framework)
- **Build System:** PlatformIO (modern, library management, better than Arduino IDE)
- **Version Control:** Git (assumed, standard practice)

### Architecture Considerations

- **Repository Structure:** Single PlatformIO project for controller firmware. GRBL firmware modifications tracked separately (based on upstream GRBL with documented servo patches from Instructables guide).

- **Service Architecture:** Two-microcontroller architecture:
  - **Controller (Nano WiFi MKR):** User interface, file management, G-code streaming, state persistence
  - **GRBL (Arduino):** Motor control, servo control, position tracking, hardware interfacing
  - **Communication:** Unidirectional serial command streaming (controller → GRBL) with status report polling (GRBL → controller)

- **State Management:** Current state machine implementation covers menu states. Needs refinement for plot state (streaming, paused, error, resume). State persistence to SD card for resume capability requires careful atomic write operations to prevent corruption on power loss.

- **Integration Requirements:**
  - Serial protocol compatibility with GRBL 1.1f specification
  - G-code parser for file preprocessing and line counting
  - GRBL status report parser for position feedback and error detection
  - Rotary encoder debouncing (already implemented and working)
  - Non-blocking SD card operations to maintain UI responsiveness

- **Security/Compliance:** None (personal project, no network exposure in MVP, no sensitive data)

## Constraints & Assumptions

### Constraints

- **Budget:** Hardware already acquired (~$50 total: Arduino Nano WiFi MKR, display, SD card reader, rotary encoder, misc components). No additional budget constraints for MVP (software-only completion).

- **Timeline:** Personal project timeline—completion driven by available hobby time rather than external deadline. Priority is functional correctness and learning over speed. Expect incremental progress over weeks/months rather than sprint-based delivery.

- **Resources:** Solo developer working in spare time. Limited C++ experience means some features may require learning/research time. No external dependencies or team coordination required—complete autonomy over technical decisions and implementation pace.

- **Technical:**
  - Arduino Nano WiFi MKR memory constraints (32KB SRAM, 256KB Flash) may limit feature complexity, though not currently a concern
  - GRBL 1.1f protocol is fixed—cannot modify GRBL communication protocol without firmware changes
  - Display resolution constrains UI complexity (text-only interface, limited simultaneous information display)
  - SD card performance may impact responsiveness during file operations (must test with actual plotting loads)
  - Single hardware serial port limits ability to add diagnostic output during development (USB serial occupied by programming/debugging)

### Key Assumptions

- **Working components assumption:** Display, SD card reader, and rotary encoder implementations are solid and won't require significant rework
- **GRBL stability assumption:** Modified GRBL firmware from Instructables guide is stable and won't introduce communication or reliability issues
- **Serial reliability assumption:** Physical serial connection between controllers is noise-free and reliable at 115200 baud over short cable lengths (10cm max)—minimal risk of signal degradation
- **G-code compatibility assumption:** Plotter-generated G-code files follow standard format compatible with GRBL 1.1f (no exotic commands or extensions)
- **Power stability assumption:** Workshop power is stable enough that resume-from-power-loss handles realistic failure modes (not constant brownouts or partial power)
- **File size assumption:** G-code files are multi-megabyte in size, making line-by-line streaming approach essential (cannot buffer full files in memory)—validates architecture decision
- **Development environment assumption:** PlatformIO remains viable development platform and library dependencies (U8g2, SdFat) continue to be maintained
- **Learning curve assumption:** C++ patterns needed for state machine refinement and serial communication are learnable through documentation and examples
- **Usage pattern assumption:** Personal use means edge cases can be documented/worked around rather than requiring bulletproof handling of every scenario

## Risks & Open Questions

### Key Risks

- **Serial communication complexity:** Implementing GRBL flow control (character counting or simple protocol) correctly is critical—errors could cause buffer overflow, lost commands, or plot corruption. **Impact:** High—core functionality depends on reliable streaming. **Mitigation:** Start with simple protocol (wait for 'ok'), test extensively with short files before long plots.

- **Plot resume state management:** Saving state atomically to SD card during plotting without introducing latency or corruption risk is technically challenging. **Impact:** Medium—resume feature is must-have but not core plotting. **Mitigation:** Use proven atomic write patterns, test resume at various plot stages, accept some small data loss window.

- **State machine complexity:** Current state structure uncertainty could lead to difficult-to-debug issues as more states (plotting, paused, error, resume) are added. **Impact:** Medium—affects code maintainability and reliability. **Mitigation:** Refactor state machine early before adding complex features, study Arduino state machine patterns/libraries.

- **GRBL error interpretation:** GRBL error codes need correct mapping to user-friendly messages; incomplete handling could leave user confused during failures. **Impact:** Low—errors are rare in normal operation. **Mitigation:** Document GRBL 1.1f error codes, implement comprehensive lookup table, test error scenarios explicitly.

- **SD card performance with large files:** Multi-megabyte G-code files may cause SD read latency that starves GRBL buffer, causing motion artifacts. **Impact:** Medium—could affect plot quality. **Mitigation:** Profile SD read performance, implement read-ahead buffering if needed, test with actual large files early.

- **C++ learning curve blockers:** Lack of C++ experience could lead to memory leaks, pointer errors, or inefficient patterns that cause runtime failures. **Impact:** Low-Medium—personal project allows time for learning. **Mitigation:** Use Arduino examples as templates, leverage PlatformIO's debugging tools, ask for code reviews in Arduino community if stuck.

### Open Questions

- How frequently should plot state be persisted to SD card? (Every N lines, time interval, or triggered by buffer state?)
- What's the optimal buffer size for G-code streaming? (How many lines to read ahead from SD card?)
- Should GRBL status reports be polled continuously or only when needed? (Affects controller CPU usage and serial bandwidth)
- How to handle G-code comments and blank lines efficiently? (Filter during streaming or rely on GRBL to ignore?)
- What's the right granularity for progress updates on display? (Update every line is too fast, every 100 lines? Percentage thresholds?)
- Should resume prompt on startup be automatic or require user confirmation? (UX vs. safety trade-off)
- How to detect GRBL connection at startup? (Send test command, wait for response, timeout handling?)
- What's the recovery strategy if GRBL stops responding mid-plot? (Retry, abort, user intervention?)

### Areas Needing Further Research

- **GRBL 1.1f protocol documentation:** Deep dive into flow control mechanisms, status report format, real-time command handling, and error/alarm differences
- **Arduino serial communication patterns:** Best practices for non-blocking serial read/write, buffer management, timeout handling in Arduino environment
- **SD card atomic write patterns:** Research methods for safe state persistence on FAT32 filesystem with power loss protection
- **State machine implementation patterns:** Study Arduino state machine libraries or patterns (e.g., enum-based, function pointer arrays, lightweight FSM frameworks)
- **G-code parsing strategies:** Efficient line parsing and validation approaches for Arduino environment (string handling, memory allocation)
- **Display update optimization:** U8g2 best practices for minimizing display updates to maintain responsiveness during plotting
- **Power loss detection:** Investigate if Arduino can detect imminent power loss to trigger emergency state save (voltage monitoring, capacitor holdover time)

## Next Steps

### Immediate Actions

1. **Review and finalize this Project Brief** - Ensure all sections accurately capture the project vision and requirements. Make any necessary revisions before proceeding.

2. **Study GRBL 1.1f protocol documentation** - Deep dive into the communication protocol, focusing on flow control mechanisms (simple protocol vs. character counting), status report format, and error/alarm codes. Reference: [GRBL 1.1 Documentation](https://github.com/gnea/grbl/wiki)

3. **Set up basic serial communication test** - Create minimal sketch to establish serial connection with GRBL Arduino, send simple commands (e.g., `$$` to query settings), and verify response parsing. Validate communication reliability before building complex features.

4. **Implement G-code file line counter** - Write utility function to count total lines in G-code file from SD card (needed for progress calculation). Test with multi-megabyte files to measure performance.

5. **Refactor state machine structure** - Review current state implementation and refactor if needed before adding plot states. Consider using enum-based state pattern or exploring lightweight Arduino FSM libraries for clarity.

6. **Create plot state skeleton** - Implement basic plot state structure with placeholder serial communication. Focus on state transitions (idle → plotting → complete/error) and integration with existing menu system.

7. **Implement simple protocol G-code streaming** - Start with basic "wait for ok" flow control. Send one line, wait for GRBL "ok" response, send next line. Test with short G-code file (100-200 lines).

8. **Add progress display** - Integrate line counting with display updates to show current line / total lines and percentage. Test display update frequency for responsiveness.

9. **Study and implement state persistence** - Research Arduino SD card atomic write patterns, implement progress save mechanism (file name + line number), and test resume logic.

10. **Create comprehensive test plan** - Document test scenarios for each core feature (streaming, progress, error handling, resume) before expanding functionality further.

### PM Handoff

This Project Brief provides comprehensive context for the Pen Plotter Controller development.

**For structured development using BMAD workflows:**
- The brief can be used as input for PRD generation (use `/bmad:bmm:workflows:prd` workflow)
- Alternatively, for a focused implementation approach, use the tech-spec workflow (`/bmad:bmm:workflows:tech-spec`) to create detailed technical specifications and generate implementation stories

**For independent development:**
- Use the MVP Scope section as your feature checklist
- Reference Technical Considerations for implementation details
- Address Open Questions through experimentation as you build
- Track progress against the Success Metrics defined in this brief

The current codebase provides a solid foundation (working display, encoder, SD card). The core challenge ahead is serial communication with GRBL and robust state management for plotting operations.
