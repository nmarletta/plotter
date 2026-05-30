// wifi_manager.cpp
// WiFi station mode + background HTTP server for the MKR WiFi 1010.
// Credentials read from /.config.cfg on SD (ssid=xxx / password=xxx).
// HTTP server runs on port 80. All HTML is served inline — no SPIFFS needed.

#include "wifi_manager.h"
#include <WiFiNINA.h>
#include <WiFiMDNSResponder.h>
#include <SdFat.h>
#include "serial_manager.h"
#include "job_control.h"

extern SdFat sd;
extern SerialManager serialMgr;

// ---- Config file parsing ----

static char _ssid[64]     = "";
static char _password[64] = "";

static void loadWifiCfg() {
  FsFile f;
  if (!f.open("/.config.cfg", O_READ)) return;
  char line[80];
  uint8_t len = 0;
  while (f.available()) {
    char c = (char)f.read();
    if (c == '\r') continue;
    bool eol = (c == '\n') || !f.available();
    if (!f.available() && c != '\n') { if (len < sizeof(line)-1) line[len++] = c; }
    if (eol) {
      line[len] = '\0';
      char *eq = strchr(line, '=');
      if (eq) {
        *eq = '\0';
        if      (strcmp(line, "ssid")     == 0) strncpy(_ssid,     eq+1, sizeof(_ssid)-1);
        else if (strcmp(line, "password") == 0) strncpy(_password, eq+1, sizeof(_password)-1);
      }
      len = 0;
    } else {
      if (len < sizeof(line)-1) line[len++] = c;
    }
  }
  f.close();
}

// ---- Server ----

static WiFiServer        _server(80);
static WiFiMDNSResponder _mdns;
static bool              _connected = false;

bool wifiBegin() {
  loadWifiCfg();
  if (_ssid[0] == '\0') {
    Serial.println("WIFI: no ssid in /.config.cfg");
    return false;
  }
  Serial.print("WIFI: connecting to "); Serial.println(_ssid);
  int status = WiFi.begin(_ssid, _password);
  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < 15000) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WIFI: failed to connect");
    return false;
  }
  _server.begin();
  _mdns.begin("plotter");
  _connected = true;
  Serial.print("WIFI: connected, IP="); Serial.println(WiFi.localIP());
  Serial.println("WIFI: mDNS started — http://plotter.local");
  return true;
}

bool wifiConnected() { return _connected && WiFi.status() == WL_CONNECTED; }
const char* wifiSSID() { return _ssid; }

String wifiIP() {
  if (!wifiConnected()) return "";
  IPAddress ip = WiFi.localIP();
  char buf[20];
  snprintf(buf, sizeof(buf), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  return String(buf);
}

// ---- HTTP helpers ----

static void sendProgmem(WiFiClient &c, const char *s) { c.print(s); }

static void send200(WiFiClient &c, const char *ct) {
  c.print(F("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: "));
  c.print(ct);
  c.print(F("\r\n\r\n"));
}

static void send400(WiFiClient &c) {
  c.print(F("HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\nBad Request"));
}

static void send409(WiFiClient &c, const char* msg) {
  c.print(F("HTTP/1.1 409 Conflict\r\nConnection: close\r\n\r\n"));
  c.print(msg);
}

static void send500(WiFiClient &c) {
  c.print(F("HTTP/1.1 500 Internal Server Error\r\nConnection: close\r\n\r\nError"));
}

// ---- GET / — HTML UI ----

static void handleRoot(WiFiClient &c) {
  send200(c, "text/html");
  c.print(F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<title>Plotter</title>"
            "<style>"
            "body{font-family:sans-serif;max-width:520px;margin:40px auto;padding:0 16px;background:#f5f5f5}"
            "h2{margin:0 0 4px}p.sub{color:#888;margin:0 0 24px;font-size:13px}"
            "section{background:#fff;border-radius:8px;padding:20px;margin-bottom:16px;box-shadow:0 1px 4px rgba(0,0,0,.08)}"
            "h3{margin:0 0 12px;font-size:15px;color:#333}"
            "input[type=file]{display:block;margin-bottom:10px}"
            "button{background:#1a73e8;color:#fff;border:none;padding:8px 18px;border-radius:5px;cursor:pointer;font-size:14px}"
            "button:hover{background:#1558b0}"
            "button.sec{background:#e8e8e8;color:#333}"
            "button.sec:hover{background:#ccc}"
            "#grbl-input{width:calc(100% - 90px);padding:6px 8px;border:1px solid #ccc;border-radius:4px;font-size:14px}"
            "#grbl-out{font-family:monospace;font-size:13px;background:#111;color:#0f0;padding:10px;border-radius:4px;"
            "min-height:60px;margin-top:10px;white-space:pre-wrap;word-break:break-all}"
            "#file-list{list-style:none;padding:0;margin:0}"
            "#file-list li{padding:5px 0;border-bottom:1px solid #f0f0f0;font-size:14px;display:flex;justify-content:space-between;align-items:center}"
            "#status{font-size:13px;color:#1a73e8;margin-top:8px}"
            "</style></head><body>"
            "<h2>Pen Plotter</h2><p class='sub'>"));
  c.print(wifiIP());
  c.print(F("</p>"

            "<section>"
            "<h3>Upload G-code</h3>"
            "<input type='file' id='f' accept='.gcode,.gco'>"
            "<button onclick='upload()'>Upload</button>"
            "<div id='status'></div>"
            "</section>"

            "<section>"
            "<h3>Files on SD</h3>"
            "<ul id='file-list'><li style='color:#aaa'>Loading...</li></ul>"
            "</section>"

            "<section>"
            "<h3>GRBL Terminal</h3>"
            "<input type='text' id='grbl-input' placeholder='e.g. $$ or M3 S500'>"
            "<button onclick='sendGrbl()'>Send</button>"
            "<div id='grbl-out'></div>"
            "</section>"

            "<script>"
            "function upload(){"
            "var f=document.getElementById('f').files[0];"
            "if(!f){alert('Pick a file first');return;}"
            "var st=document.getElementById('status');"
            "st.textContent='Uploading '+f.name+'...';"
            "var fd=new FormData();fd.append('file',f);"
            "fetch('/upload',{method:'POST',body:fd})"
            ".then(r=>r.text()).then(t=>{st.textContent=t;loadFiles();})"
            ".catch(e=>{st.textContent='Error: '+e;});}"

            "function loadFiles(){"
            "fetch('/files').then(r=>r.json()).then(files=>{"
            "var ul=document.getElementById('file-list');"
            "ul.innerHTML='';"
            "if(!files.length){ul.innerHTML='<li style=\"color:#aaa\">No files</li>';return;}"
            "files.forEach(function(n){"
            "var li=document.createElement('li');"
            "li.textContent=n;"
            "ul.appendChild(li);});}).catch(()=>{});}"

            "function sendGrbl(){"
            "var inp=document.getElementById('grbl-input');"
            "var cmd=inp.value.trim();if(!cmd)return;"
            "fetch('/grbl',{method:'POST',body:cmd,headers:{'Content-Type':'text/plain'}})"
            ".then(r=>r.text()).then(t=>{"
            "var o=document.getElementById('grbl-out');"
            "o.textContent+='> '+cmd+'\\n'+t+'\\n';inp.value='';});"
            "}"
            "document.getElementById('grbl-input').addEventListener('keydown',function(e){if(e.key==='Enter')sendGrbl();});"
            "loadFiles();"
            "</script></body></html>"));
}

// ---- GET /files — JSON list ----

static void handleFiles(WiFiClient &c) {
  send200(c, "application/json");
  c.print('[');
  FsFile root = sd.open("/");
  if (root) {
    root.rewind();
    char nameBuf[64];
    bool first = true;
    while (true) {
      FsFile entry;
      if (!entry.openNext(&root, O_READ)) break;
      entry.getName(nameBuf, sizeof(nameBuf));
      if (!entry.isDir() && nameBuf[0] != '.' && nameBuf[0] != '_') {
        size_t len = strlen(nameBuf);
        bool isGcode = false;
        if (len >= 6) {
          const char *e = nameBuf+len-6;
          if (e[0]=='.' && (e[1]=='g'||e[1]=='G')) isGcode = true;
        }
        if (!isGcode && len >= 4) {
          const char *e = nameBuf+len-4;
          if (e[0]=='.' && (e[1]=='g'||e[1]=='G')) isGcode = true;
        }
        if (isGcode) {
          if (!first) c.print(',');
          c.print('"'); c.print(nameBuf); c.print('"');
          first = false;
        }
      }
      entry.close();
    }
    root.close();
  }
  c.print(']');
}

// ---- POST /grbl — send command, return response ----

static void handleGrbl(WiFiClient &c, const String &body) {
  send200(c, "text/plain");
  String cmd = body;
  cmd.trim();
  if (cmd.length() == 0) { c.print("empty command"); return; }
  Serial1.println(cmd);
  // Collect response lines for up to 1 second
  unsigned long deadline = millis() + 1000;
  String resp = "";
  while (millis() < deadline) {
    if (Serial1.available()) {
      char ch = (char)Serial1.read();
      resp += ch;
      if (ch == '\n') {
        // Stop early on ok or error
        String t = resp; t.trim();
        if (t.endsWith("ok") || t.indexOf("error:") >= 0 || t.indexOf("ALARM:") >= 0) break;
      }
    }
  }
  resp.trim();
  c.print(resp.length() ? resp : "ok");
}

// ---- POST /upload — multipart file upload → SD ----
// Streams the multipart body directly to SD without buffering full content.
// Finds the boundary, skips part headers, writes payload to /filename.

static void handleUpload(WiFiClient &c, WiFiClient &raw, int contentLength, const String &boundary) {
  if (boundary.length() == 0 || contentLength <= 0) { send400(c); return; }

  // We stream bytes directly from the client, looking for:
  //   --<boundary>\r\n                    (opening boundary)
  //   Content-Disposition: ...; filename="foo.gcode"\r\n
  //   (other headers)\r\n
  //   \r\n                                (blank line = start of data)
  //   <file data>
  //   --<boundary>--                      (closing boundary)

  String boundaryEnd  = "--" + boundary + "--";
  String boundaryLine = "--" + boundary;

  // Helper: read until \r\n (or timeout), return the line without CRLF
  auto readLine = [&](unsigned long timeoutMs) -> String {
    String line = "";
    unsigned long t = millis();
    while (millis() - t < timeoutMs) {
      while (raw.available()) {
        char ch = (char)raw.read();
        if (ch == '\r') continue;
        if (ch == '\n') return line;
        line += ch;
        t = millis(); // reset timeout on progress
      }
    }
    return line;
  };

  // 1. Skip to opening boundary
  for (int attempt = 0; attempt < 10; attempt++) {
    String line = readLine(3000);
    if (line == boundaryLine) break;
    if (attempt == 9) { send400(c); return; }
  }

  // 2. Parse part headers to extract filename
  char filename[64] = "upload.gcode";
  while (true) {
    String h = readLine(2000);
    if (h.length() == 0) break; // blank line = end of headers
    // Content-Disposition: form-data; name="file"; filename="foo.gcode"
    int fi = h.indexOf("filename=\"");
    if (fi >= 0) {
      int start = fi + 10;
      int end   = h.indexOf('"', start);
      if (end > start) {
        String fn = h.substring(start, end);
        fn.trim();
        // Sanitise: strip path separators
        int slash = fn.lastIndexOf('/');
        if (slash >= 0) fn = fn.substring(slash + 1);
        slash = fn.lastIndexOf('\\');
        if (slash >= 0) fn = fn.substring(slash + 1);
        fn.toCharArray(filename, sizeof(filename));
      }
    }
  }

  // 3. Open file on SD
  String path = "/" + String(filename);
  sd.remove(path.c_str());
  FsFile outFile;
  if (!outFile.open(path.c_str(), O_WRONLY | O_CREAT)) {
    send500(c);
    return;
  }

  // 4. Stream body to file, stopping at closing boundary.
  // We need to detect "\r\n--<boundary>" which marks the end of file data.
  // Strategy: use a small ring buffer to hold the last N bytes and compare.
  String endMarker = "\r\n" + boundaryLine;
  int    mLen      = endMarker.length();
  // Buffer slightly larger than the end marker
  const int RING = 80;
  char ring[RING];
  int  rHead = 0, rCount = 0;
  bool done  = false;

  unsigned long deadline = millis() + 30000UL; // 30s max upload time
  while (!done && millis() < deadline) {
    while (raw.available() && !done) {
      char ch = (char)raw.read();
      deadline = millis() + 30000UL; // reset on progress

      // Add to ring
      ring[(rHead + rCount) % RING] = ch;
      if (rCount < RING) rCount++;
      else { // ring full — emit oldest byte to file
        outFile.write((uint8_t)ring[rHead]);
        rHead = (rHead + 1) % RING;
      }

      // Check if tail of ring matches end marker
      if (rCount >= mLen) {
        bool match = true;
        for (int i = 0; i < mLen && match; i++) {
          if (ring[(rHead + rCount - mLen + i) % RING] != endMarker[i]) match = false;
        }
        if (match) {
          // Flush everything before the end marker
          int toFlush = rCount - mLen;
          for (int i = 0; i < toFlush; i++) {
            outFile.write((uint8_t)ring[(rHead + i) % RING]);
          }
          done = true;
        }
      }
    }
  }

  outFile.close();

  if (!done) { send500(c); return; }

  send200(c, "text/plain");
  c.print("Uploaded: ");
  c.print(filename);
}

// ---- GET /status — machine state as JSON ----

static void handleStatus(WiFiClient &c) {
  send200(c, "application/json");
  GCodeStatus s = plotStatus();
  const char* stateStr =
    s == GCodeStatus::Running   ? "running"   :
    s == GCodeStatus::Paused    ? "paused"    :
    s == GCodeStatus::Completed ? "completed" :
    s == GCodeStatus::Error     ? "error"     :
    s == GCodeStatus::Canceled  ? "canceled"  :
    s == GCodeStatus::Alarm     ? "alarm"     :
    s == GCodeStatus::Resetting ? "resetting" :
                                  "idle";
  char buf[300];
  snprintf(buf, sizeof(buf),
    "{\"state\":\"%s\",\"file\":\"%s\",\"progress\":%.2f,\"line\":%lu,\"ip\":\"%s\",\"ssid\":\"%s\"}",
    stateStr,
    plotFilename() ? plotFilename() : "",
    plotProgress(),
    (unsigned long)plotCurrentLine(),
    wifiIP().c_str(),
    _ssid
  );
  c.print(buf);
}

// ---- POST /start — begin streaming a file ----

static void handleStart(WiFiClient &c, const String &body) {
  String fname = body;
  fname.trim();
  if (fname.length() == 0) { send400(c); return; }
  if (fname[0] != '/') fname = "/" + fname;
  if (!plotStartRemote(fname.c_str())) {
    send409(c, "Already plotting");
    return;
  }
  send200(c, "text/plain");
  c.print("ok");
}

// ---- POST /pause, /resume, /stop ----

static void handlePause(WiFiClient &c) {
  if (plotStatus() != GCodeStatus::Running) { send409(c, "Not running"); return; }
  plotPause();
  send200(c, "text/plain"); c.print("ok");
}

static void handleResume(WiFiClient &c) {
  if (plotStatus() != GCodeStatus::Paused) { send409(c, "Not paused"); return; }
  plotResume();
  send200(c, "text/plain"); c.print("ok");
}

static void handleStop(WiFiClient &c) {
  plotCancel();
  send200(c, "text/plain"); c.print("ok");
}

// ---- Request dispatcher ----

struct HttpRequest {
  char method[8];
  char path[64];
  char contentType[80];
  char boundary[60];
  int  contentLength;
};

static bool parseHeaders(WiFiClient &client, HttpRequest &req) {
  memset(&req, 0, sizeof(req));
  req.contentLength = -1;

  // Read request line
  String line = "";
  unsigned long t = millis();
  while (millis() - t < 3000) {
    while (client.available()) {
      char c = (char)client.read();
      if (c == '\r') continue;
      if (c == '\n') goto gotRequestLine;
      line += c;
    }
  }
  return false;

gotRequestLine:
  // METHOD /path HTTP/1.1
  {
    int s1 = line.indexOf(' ');
    int s2 = line.indexOf(' ', s1+1);
    if (s1 < 0 || s2 < 0) return false;
    line.substring(0, s1).toCharArray(req.method, sizeof(req.method));
    line.substring(s1+1, s2).toCharArray(req.path, sizeof(req.path));
  }

  // Read headers
  while (true) {
    String h = "";
    t = millis();
    while (millis() - t < 2000) {
      while (client.available()) {
        char c = (char)client.read();
        if (c == '\r') continue;
        if (c == '\n') goto gotHeader;
        h += c;
      }
    }
    break;
gotHeader:
    if (h.length() == 0) break; // blank line = end of headers
    String hl = h; hl.toLowerCase();
    if (hl.startsWith("content-length:")) {
      req.contentLength = h.substring(15).toInt();
    } else if (hl.startsWith("content-type:")) {
      String ct = h.substring(13); ct.trim();
      ct.toCharArray(req.contentType, sizeof(req.contentType));
      // Extract boundary for multipart
      int bi = hl.indexOf("boundary=");
      if (bi >= 0) {
        String bv = h.substring(bi + 9); bv.trim();
        bv.toCharArray(req.boundary, sizeof(req.boundary));
      }
    }
  }
  return true;
}

// ---- wifiTick ----

void wifiTick() {
  if (!wifiConnected()) return;

  _mdns.poll();

  WiFiClient client = _server.available();
  if (!client) return;

  HttpRequest req;
  if (!parseHeaders(client, req)) { client.stop(); return; }

  Serial.print("HTTP "); Serial.print(req.method); Serial.print(' '); Serial.println(req.path);

  if (strcmp(req.method, "GET") == 0) {
    if (strcmp(req.path, "/") == 0) {
      handleRoot(client);
    } else if (strcmp(req.path, "/files") == 0) {
      handleFiles(client);
    } else if (strcmp(req.path, "/status") == 0) {
      handleStatus(client);
    } else {
      client.print(F("HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\nNot Found"));
    }
  } else if (strcmp(req.method, "POST") == 0) {
    if (strcmp(req.path, "/upload") == 0) {
      // Upload streams directly — must NOT pre-buffer the body
      handleUpload(client, client, req.contentLength, String(req.boundary));
    } else {
      // All other POST endpoints use a small text body
      String body = "";
      if (req.contentLength > 0) {
        unsigned long t = millis();
        while ((int)body.length() < req.contentLength && millis()-t < 2000) {
          while (client.available()) body += (char)client.read();
        }
      }
      if      (strcmp(req.path, "/grbl")   == 0) handleGrbl(client, body);
      else if (strcmp(req.path, "/start")  == 0) handleStart(client, body);
      else if (strcmp(req.path, "/pause")  == 0) handlePause(client);
      else if (strcmp(req.path, "/resume") == 0) handleResume(client);
      else if (strcmp(req.path, "/stop")   == 0) handleStop(client);
      else client.print(F("HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\nNot Found"));
    }
  } else {
    client.print(F("HTTP/1.1 405 Method Not Allowed\r\nConnection: close\r\n\r\n"));
  }

  delay(2); // give client time to receive data before we close
  client.stop();
}
