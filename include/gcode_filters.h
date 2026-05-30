// gcode_filters.h
// Stateless G-code line transforms used by GCodeStreamer.
// Pure C: no Arduino dependency. Include from gcode_streamer.cpp and native tests.
#pragma once

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdint.h>

extern int  g_penDownS;
extern int  g_penUpS;
extern bool g_overwriteS;

// Strip inline comments, trim, skip non-motion lines, remove Z-only moves,
// strip Z token from mixed XY+Z moves. In-place. Returns true = keep line.
inline bool filterLine(char* line) {
    char* cp = strchr(line, '(');
    if (cp) *cp = '\0';
    int len = (int)strlen(line);
    while (len > 0 && line[len-1] == ' ') line[--len] = '\0';

    if (len == 0) return false;
    char first = line[0];
    if (first == ';' || first == '(') return false;
    if (first == 'T' || first == 't') return false;

    char up[80];
    strncpy(up, line, sizeof(up) - 1); up[sizeof(up)-1] = '\0';
    for (int i = 0; up[i]; i++) up[i] = (char)toupper((unsigned char)up[i]);

    char* zp = strchr(up, 'Z');
    if (zp) {
        if (!strchr(up, 'X') && !strchr(up, 'Y')) return false; // Z-only
        int zi  = (int)(zp - up);
        int end = zi + 1;
        if (line[end] == '-' || line[end] == '+') end++;
        while (isdigit((unsigned char)line[end]) || line[end] == '.') end++;
        if (line[end] == ' ') end++;
        memmove(line + zi, line + end, strlen(line + end) + 1);
        len = (int)strlen(line);
        while (len > 0 && line[len-1] == ' ') line[--len] = '\0';
    }
    return true;
}

// Inject S value into bare M4/M5 commands that have no S or XY present.
inline void injectMissingS(char* line, uint8_t maxLen) {
    char up[80];
    strncpy(up, line, sizeof(up) - 1); up[sizeof(up)-1] = '\0';
    for (int i = 0; up[i]; i++) up[i] = (char)toupper((unsigned char)up[i]);
    if (strchr(up, 'X') || strchr(up, 'Y')) return;
    if (strchr(up, 'S')) return;
    if      (strstr(up, "M4")) snprintf(line, maxLen, "M4 S%d", g_penUpS);
    else if (strstr(up, "M5")) snprintf(line, maxLen, "M5 S%d", g_penDownS);
}

// Rewrite M3/M5 pen commands to configured S values when overwrite is on.
inline void applyPenOverwrite(char* line, uint8_t maxLen) {
    if (!g_overwriteS) return;
    char up[80];
    strncpy(up, line, sizeof(up) - 1); up[sizeof(up)-1] = '\0';
    for (int i = 0; up[i]; i++) up[i] = (char)toupper((unsigned char)up[i]);
    if (strchr(up, 'X') || strchr(up, 'Y')) return;
    if      (strstr(up, "M3")) snprintf(line, maxLen, "M5 S%d", g_penDownS);
    else if (strstr(up, "M4")) snprintf(line, maxLen, "M4 S%d", g_penUpS);
    else if (strstr(up, "M5")) snprintf(line, maxLen, "M5 S%d", g_penDownS);
}

// Returns true if the line is a standalone pen command (M3/M4/M5 with no XY motion).
// Used to decide where to inject a G4 dwell.
inline bool isPenCommand(const char* line) {
    char up[80];
    strncpy(up, line, sizeof(up) - 1); up[sizeof(up)-1] = '\0';
    for (int i = 0; up[i]; i++) up[i] = (char)toupper((unsigned char)up[i]);
    if (strchr(up, 'X') || strchr(up, 'Y')) return false;
    return strstr(up, "M3") || strstr(up, "M4") || strstr(up, "M5");
}

// Build the progress file path for a given gcode filepath.
inline void progressPathFor(const char* gcodePath, char* out, uint8_t maxLen) {
    const char* slash = strrchr(gcodePath, '/');
    const char* base  = slash ? slash + 1 : gcodePath;
    snprintf(out, maxLen, "/%s.progress", base);
}
