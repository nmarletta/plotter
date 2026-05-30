# Plotter WiFi HTTP API

The plotter runs an HTTP server on **port 80** when connected to WiFi.  
Credentials are read from `/.config.cfg` on the SD card at boot.

## Setup

Copy `config.cfg` from the project root to the SD card and rename it to `.config.cfg`.  
Fill in your network credentials:

```
ssid=YourNetworkName
password=YourPassword
```

Once connected, the plotter is reachable at **http://plotter.local** on any device on the same network.  
The raw IP is printed to Serial on boot. On the device, go to **WiFi** in the main menu to see connection status and reconnect if needed.

---

## Endpoints

### `GET /`

Returns the web UI — an HTML page with:
- File upload form (accepts `.gcode` / `.gco`)
- SD card file browser
- GRBL terminal with send/receive

No parameters.

---

### `GET /status`

Returns the current machine state as JSON.

**Response** `200 application/json`

```json
{
  "state":    "idle",
  "file":     "drawing.gcode",
  "progress": 0.45,
  "line":     1234,
  "ip":       "192.168.1.100",
  "ssid":     "MyNetwork"
}
```

**`state` values**

| Value | Meaning |
|-------|---------|
| `idle` | No job loaded |
| `running` | Actively streaming |
| `paused` | Feed hold active |
| `completed` | Last job finished |
| `canceled` | Last job was stopped |
| `error` | GRBL error, retry or cancel |
| `alarm` | GRBL alarm triggered |
| `resetting` | Soft reset in progress |

---

### `GET /files`

Returns a JSON array of G-code filenames present in the SD card root.

**Response** `200 application/json`

```json
["part1.gcode", "logo.gco"]
```

Hidden files (`.` prefix) and macOS resource forks (`_` prefix) are excluded.

---

### `POST /start`

Begin streaming a file from the SD card.

**Request**

```
Content-Type: text/plain

Body: filename.gcode
```

The filename must exist in the SD card root. A leading `/` is optional.  
Returns `409 Conflict` if a plot is already running or paused.

**Response** `200 text/plain`

```
ok
```

---

### `POST /pause`

Pause the current plot (sends GRBL feed hold `!`).

Returns `409 Conflict` if the machine is not currently running.

**Response** `200 text/plain`

```
ok
```

---

### `POST /resume`

Resume a paused plot (sends GRBL cycle start `~`).

Returns `409 Conflict` if the machine is not currently paused.

**Response** `200 text/plain`

```
ok
```

---

### `POST /stop`

Cancel the current plot. Safe to call at any time.

**Response** `200 text/plain`

```
ok
```

---

### `POST /upload`

Upload a G-code file to the SD card root via `multipart/form-data`.

**Request**

```
Content-Type: multipart/form-data; boundary=<boundary>

form field name: file
```

The filename is taken from the `Content-Disposition` header.  
If a file with the same name already exists it is overwritten.  
Upload timeout: 30 seconds.

**Response** `200 text/plain`

```
Uploaded: filename.gcode
```

**Errors**

| Code | Reason |
|------|--------|
| 400 | Missing or malformed boundary / content-length |
| 409 | Already plotting (for job control endpoints) |
| 500 | SD card write failed |

---

### `POST /grbl`

Send a raw GRBL command and return the response.

**Request**

```
Content-Type: text/plain

Body: <GRBL command>
```

Example body: `G0 X10 Y20` or `$$` or `$X`

The command is forwarded to GRBL over Serial1. The response is collected for up to **1 second**, or until `ok`, `error:`, or `ALARM:` is received.

**Response** `200 text/plain`

```
ok
```

---

## Not yet implemented

| Feature | Notes |
|---------|-------|
| AP / fallback mode | If the network is unavailable WiFi fails at boot; use the WiFi menu to reconnect |
| Auto-reconnect | A dropped connection is not recovered automatically — use the WiFi menu |
| Authentication | No login — anyone on the network has full access |
