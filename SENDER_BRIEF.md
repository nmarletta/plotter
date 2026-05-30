# Plotter Sender — Project Brief

## What this is

A pen plotter controller built on an Arduino MKR WiFi 1010. The device:

- Reads G-code files from an SD card
- Streams them line-by-line to a second Arduino running GRBL, which drives the motors and pen servo
- Exposes an HTTP API over WiFi so a sender application can control it remotely

The device has its own OLED display and rotary encoder for local control, but the sender application is the primary interface for day-to-day use: uploading files and starting/monitoring plots.

The full HTTP API is documented in `WiFi_API.md` (provided alongside this brief).

---

## What needs to be built

A **sender application** that communicates with the plotter over its local WiFi HTTP API. The user connects to the plotter by entering its IP address. All communication is plain HTTP on port 80 — no WebSocket, no authentication.

---

## User flows

### 1. Connect
- User enters the plotter's IP address
- App fetches `GET /status` to confirm connection and show current state
- App displays the plotter's SSID and IP for reference

### 2. Upload a file
- User picks a `.gcode` or `.gco` file from their computer
- App uploads it via `POST /upload` (multipart/form-data)
- On success, the file appears in the file list

### 3. Browse and start a file
- App fetches `GET /files` to list all G-code files on the SD card
- User selects a file
- App sends `POST /start` with the filename
- App transitions to the monitoring view

### 4. Monitor a plot
- App polls `GET /status` (suggested interval: 1–2 seconds)
- Shows: filename, progress bar (0–100%), current line number, machine state
- Controls available during a plot:
  - **Pause** → `POST /pause`
  - **Resume** → `POST /resume`
  - **Stop** → `POST /stop` (with confirmation prompt)
- When state becomes `completed` or `canceled`, return to the file list

### 5. GRBL terminal (advanced)
- A collapsible or secondary panel
- User types a GRBL command, app sends `POST /grbl`
- Response is displayed below the input
- Useful for manual jogging, homing, checking `$$` settings

---

## States the plotter can be in

Returned by `GET /status` as the `state` field:

| State | What to show |
|-------|--------------|
| `idle` | Ready — show file list and upload |
| `running` | Progress bar + Pause / Stop |
| `paused` | Progress bar + Resume / Stop |
| `completed` | "Finished" message, then back to file list |
| `canceled` | "Stopped" message, then back to file list |
| `error` | Error message — user must stop or the device retries |
| `alarm` | GRBL alarm — show alarm, user must unlock via GRBL terminal (`$X`) or stop |
| `resetting` | Brief transition state — show spinner |

---

## Constraints and notes

- **No WebSocket** — use polling for status. 1–2 second interval is fine; the plotter is slow.
- **Single client** — the HTTP server handles one request at a time. Don't fire concurrent requests.
- **No authentication** — the device is on a trusted local network. No login needed.
- **Hostname** — the plotter is reachable at `http://plotter.local` via mDNS on any device on the same network. A raw IP input should still be available as fallback (mDNS can be unreliable on some Windows machines).
- **Upload size** — the server streams uploads directly to SD. Large files (10+ MB) are fine but the upload timeout is 30 seconds, so avoid slow connections.
- **409 Conflict** — job control endpoints return 409 if the command doesn't apply to the current state (e.g. pause when idle). Handle gracefully.
- **File names** — returned by `/files` without a path prefix. Pass them to `/start` as-is.

---

## Nice-to-haves (not required for MVP)

- Drag-and-drop file upload
- Estimated time remaining (derive from progress + elapsed time)
- Visual indicator for connection lost (poll fails)
- Remember last used IP address (localStorage or similar)
- Mobile-friendly layout
