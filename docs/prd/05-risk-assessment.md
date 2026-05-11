# Risk Assessment and Mitigation

## Technical Risks

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

## Integration Risks

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
