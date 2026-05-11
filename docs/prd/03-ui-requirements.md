# User Interface Enhancement Goals

## Integration with Existing UI

The controller uses a **text-based OLED interface** (128x64 pixels, U8g2 library) with **rotary encoder navigation** (rotate to select, click to confirm). The existing UI pattern displays **3 menu items at once** with visual selection indicator, scrolling through longer lists. This 3-item vertical limit is a hard constraint validated through display testing.

**New UI elements must integrate with this established pattern:**

- **Plot Progress Screen** - Replace StatePlot's Serial.println() debug output with U8g2 display calls. Show compact status information (filename truncated if needed, percentage, status text, 2-item menu: Pause/Continue, Cancel) fitting the 3-item display paradigm.

- **Settings Edit Screen** - Existing state_settings.cpp already implements the encoder interaction pattern (list navigation → select setting → edit value → confirm). Display integration needed to replace displayList() and displayChangeSettings() with U8g2 rendering.

- **Control/Jog Screen** - New screen following existing pattern: 3-item menu (X Jog, Y Jog, Home), with sub-state for jog distance/direction selection using encoder rotation for incremental movement.

- **Error Display** - Simple pause-and-display approach: show error message with Continue/Cancel options, allowing user to manually resolve hardware issues.

**Design Consistency:** All new screens use the same U8g2 font, layout spacing, and selection indicators as existing file browser and main menu.

## Modified/New Screens and Views

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

## UI Consistency Requirements

**UC1: Display Rendering Pattern** - All screens shall use U8g2's page buffer mode with consistent font (u8g2_font_ncenB08 or equivalent), 8-pixel line spacing, and full-screen clear before redraw.

**UC2: 3-Item Display Paradigm** - List-based screens (settings, control menu) shall show maximum 3 items simultaneously with "> " indicator for selected item, scrolling to reveal additional items. This is a hard constraint based on 128x64 pixel display testing.

**UC3: Encoder Interaction Consistency** - All screens shall respond to encoder rotation (navigate/adjust) and click (select/confirm) with identical timing and debounce behavior as existing file browser.

**UC4: Text Truncation Strategy** - Long text (filenames, setting names) shall truncate with "..." suffix when exceeding display width, ensuring critical information remains visible.

**UC5: Status Text Standardization** - GRBL status shall display using consistent labels: "Idle", "Running", "Paused", "Error", "Completed", "Canceled" matching GCodeStatus enum names. Progress percentage displayed as integer (e.g., "42%" not "42.5%").

**UC6: Non-Blocking Updates** - Display updates during plotting shall not introduce perceptible latency or block G-code streaming, using efficient U8g2 draw operations and update throttling if needed.
