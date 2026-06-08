# Sender Integration: Recent Firmware Changes

Three new capabilities were added to the HTTP API on the `feature/wifi` branch.
All endpoints are on the plotter's IP, port 80. The device is reachable at
`http://plotter/` via mDNS on most networks.

---

## 1. Pen settings endpoints

Read and write the four pen parameters remotely. Changes are persisted to SD
(`/.config.cfg`) and take effect immediately — same as changing them via the
rotary encoder.

### `GET /pen`

Returns current pen settings.

```json
{
  "penDown":  10,
  "penUp":    800,
  "overwrite": 0,
  "penDelay": 0
}
```

| Field | Type | Description |
|-------|------|-------------|
| `penDown` | int | S value sent for pen-down command (0–1000) |
| `penUp` | int | S value sent for pen-up command (0–1000) |
| `overwrite` | 0 or 1 | When 1, all M3/M4/M5 S values in the gcode file are replaced with `penDown`/`penUp` |
| `penDelay` | int | Milliseconds of G4 dwell injected after each pen command (0 = disabled) |

### `POST /pen`

Updates any subset of the pen settings. Send a JSON body with only the fields
you want to change.

**Request body** (`Content-Type: application/json`):
```json
{ "penDown": 50, "overwrite": 1 }
```

**Response:** same shape as `GET /pen`, reflecting the values now in effect.

**Example:**
```sh
# Read
curl http://plotter/pen

# Set pen-down S value and enable overwrite
curl -X POST http://plotter/pen \
     -H 'Content-Type: application/json' \
     -d '{"penDown": 50, "overwrite": 1}'
```

---

## 2. Machine position in `/status`

`GET /status` now includes the real-time machine position (`x`, `y` in mm),
queried directly from GRBL via the `?` real-time command on every status poll.
This is the actual position mid-motion, not a software estimate.

**Full response shape:**
```json
{
  "state":       "running",
  "file":        "drawing.gcode",
  "progress":    0.42,
  "line":        183,
  "x":           45.123,
  "y":           112.456,
  "pauseReason": "",
  "ip":          "192.168.1.42",
  "ssid":        "MyNetwork"
}
```

| Field | Type | Description |
|-------|------|-------------|
| `state` | string | `idle`, `running`, `paused`, `completed`, `error`, `canceled`, `alarm`, `resetting` |
| `file` | string | Active filename, empty when idle |
| `progress` | float 0–1 | File byte position ratio |
| `line` | int | Gcode lines sent so far |
| `x` | float | Machine X position in mm |
| `y` | float | Machine Y position in mm |
| `pauseReason` | string | Label from `;PAUSE` marker, or `""` when not paused by a marker |
| `ip` | string | Device IP |
| `ssid` | string | Connected network |

**Recommendation:** poll `/status` every 200–500 ms during plotting to drive a
live position preview. This is the only endpoint that needs polling — it covers
progress, position, and pause state in one request.

---

## 3. Automatic pause on pen-change markers

The gcode streamer now recognises `;PAUSE <label>` comment lines as pen-change
breakpoints. No firmware configuration needed — it is purely driven by the
gcode file content.

### How it works

1. When the streamer encounters a `;PAUSE` line it:
   - Injects a pen-up command immediately
   - Sets state to `paused`
   - Stores the label as the pause reason
2. The OLED on the device shows the label in the middle of the screen instead
   of the progress bar, with "Continue" and "Cancel" buttons.
3. `/status` returns `"pauseReason": "<label>"` so the sender can show it in
   its terminal / UI.
4. The user changes the pen and resumes — via the rotary encoder on the device,
   or `POST /resume` from the sender. The pause reason is cleared on resume.

### Gcode convention

Insert a `;PAUSE` comment line between groups that require a pen change:

```gcode
; === Group 1: red layer ===
G0 X10 Y10
G1 X50 Y10
; ...more moves...

;PAUSE Red pen

; === Group 2: blue layer ===
G0 X10 Y20
; ...more moves...

;PAUSE Blue pen

; === Group 3: black layer ===
```

**Variants accepted** (all equivalent):
```
;PAUSE Red pen
; PAUSE Red pen
;pause red pen
; pause Red pen
;PAUSE              ← no label → shows "Pen change"
```

### What the sender should do

- When `state == "paused"` and `pauseReason != ""`:
  - Show the reason prominently in the terminal / status bar
  - Enable the "Resume" button (or prompt the user)
- When the user clicks Resume: `POST /resume`
- `POST /resume` returns `409` if the plotter is not currently paused

---

## Existing endpoints (unchanged, for reference)

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/` | HTML web UI |
| `GET` | `/files` | JSON array of gcode filenames on SD |
| `GET` | `/status` | Machine state (extended, see above) |
| `GET` | `/pen` | Pen settings _(new)_ |
| `POST` | `/upload` | Multipart file upload to SD |
| `POST` | `/grbl` | Send raw GRBL command, returns response |
| `POST` | `/start` | Body: filename — start plotting |
| `POST` | `/pause` | Pause current plot |
| `POST` | `/resume` | Resume paused plot |
| `POST` | `/stop` | Cancel current plot |
| `POST` | `/pen` | Update pen settings _(new)_ |

All `POST` endpoints (except `/upload`) return `200 ok`, `400 Bad Request`, or
`409 Conflict` (used when the requested action is not valid in the current
state, e.g. resuming when not paused).
