# Tests

Three phases. Each phase needs more hardware than the last.

---

## Phase 1 — No hardware at all (run any time)

```
pio test -e test_native
```

The PC compiles and runs these tests itself. A fake serial port (`StreamMock`)
replaces GRBL. A fake Arduino header replaces the Arduino SDK.

| Folder | What it tests |
|---|---|
| `test_native_filters/` | `filterLine`, `injectMissingS`, `applyPenOverwrite`, `progressPathFor` — the G-code rewrite rules |
| `test_native_serial/` | `SerialManager` — every method: `sendLine`, `waitForOk`, `readLine`, `flushRx`, `parseStatus` |

---

## Phase 2 — Uno plugged into the PC over USB (no MKR)

```
pio test -e test_native_grbl
```

Still runs on the PC (native binary), but opens the real serial port to talk to
actual GRBL. Tests auto-skip gracefully when no Uno is connected.

Set the port explicitly if auto-detect picks the wrong device:

```
GRBL_PORT=/dev/cu.usbmodemXXXX pio test -e test_native_grbl
```

| Folder | What it tests |
|---|---|
| `test_native_grbl/` | `SerialManager` against real GRBL responses. `GCodeStreamer` streaming a file from the PC filesystem to real GRBL. |

---

## Phase 3 — MKR + Uno + SD card (full hardware)

```
pio test -e test_serial_mgr   # SerialManager on MKR → Uno
pio test -e test_plot         # GCodeStreamer streaming a file from SD
```

Flashed onto the MKR. Requires the MKR connected to the PC (for flashing) and
Uno connected to MKR via Serial1 pins.

| Folder | What it tests |
|---|---|
| `test_serial_manager/` | `SerialManager` on real MKR hardware talking to GRBL |
| `test_plot/` | `GCodeStreamer` end-to-end: SD card → filter pipeline → GRBL |

---

## Support files (not test suites)

| File/Folder | Purpose |
|---|---|
| `test_native_serial/mocks/Arduino.h` | Minimal Arduino SDK stub for native builds |
| `test_native_serial/stream_mock.h` | Fake serial stream — pre-load RX bytes, capture TX bytes |
| `test_native_grbl/posix_stream.h` | Real serial port wrapped as an Arduino `Stream` |
