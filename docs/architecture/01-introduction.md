# Introduction

This document outlines the architectural approach for enhancing **Pen Plotter Controller** with **complete serial GRBL communication, display integration, plot state management, settings management, and jog control functionality**. Its primary goal is to serve as the guiding architectural blueprint for AI-driven development of new features while ensuring seamless integration with the existing system.

**Relationship to Existing Architecture:**
This document defines the architecture for completing a partially implemented Arduino-based plotter controller. The existing codebase includes working display, rotary encoder, SD card file browsing, and drafted (but untested) GCodeStreamer and StatePlot classes. This architecture establishes how to validate, integrate, and complete these components into a production-ready system.

## Existing Project Analysis

### Current Project State
- **Primary Purpose:** Standalone Arduino-based G-code streaming controller for DIY pen plotter
- **Current Tech Stack:** Arduino (MKR WiFi 1010), C++, PlatformIO, U8g2 (display), SdFat (SD card)
- **Architecture Style:** State machine architecture with enum-based state management
- **Deployment Method:** PlatformIO build and upload to Arduino hardware

### Available Documentation
- ✅ Project Brief (`docs/brief.md`) - Comprehensive problem statement and solution approach
- ✅ Brownfield PRD (`docs/prd.md`) - Detailed requirements for enhancement
- ❌ Architecture documentation - Does not exist (this document will create it)
- ❌ API documentation - No formal documentation of class interfaces
- ❌ Testing documentation - No test plans or validation procedures

### Identified Constraints
- **Hardware:** Arduino MKR WiFi 1010 (32KB SRAM, 256KB Flash) - memory constraints
- **Communication:** Hardware Serial1 for GRBL at 115200 baud - single serial channel
- **Display:** 128x64 OLED via U8g2 - limited screen real estate, text-only interface
- **Storage:** SD card via SdFat - file system operations must be efficient
- **Untested Code:** GCodeStreamer and StatePlot classes have never been validated against hardware
- **Power:** Standalone operation - must handle power interruptions gracefully
- **User Interface:** Rotary encoder only - no keyboard/touchscreen for complex input

## Change Log
| Change | Date | Version | Description | Author |
|--------|------|---------|-------------|--------|
| Initial Architecture | 2025-11-25 | 1.0 | Created brownfield architecture for plotter controller MVP | Winston (Architect) |
