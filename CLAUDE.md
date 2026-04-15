# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Arduino sketch for an ESP32-based physical Slack status updater. Six momentary buttons connected to an ESP32 D1 Mini trigger Slack API calls to set status text, emoji, and presence (AWAY/AUTO).

## Build & Flash

This is an Arduino project — there are no CLI build commands. Compilation and flashing are done through **Arduino IDE**:

1. Copy `credentials_example.h` → `credentials.h` and fill in WiFi credentials and Slack OAuth token
2. Open `Slack_status_updater_Waveshare_4.0in.ino` in Arduino IDE
3. Select board: **ESP32 D1 Mini** and the appropriate COM port
4. Click **Upload**

Serial monitor baud rate: **115200**

## Required Libraries

Install via Arduino Library Manager or manually:
- `ArduinoSlack` — https://github.com/witnessmenow/arduino-slack-api
- `ArduinoJson` — available in Arduino Library Manager

Built-in: `WiFi`, `WiFiClientSecure`

## Architecture

Everything lives in a single `.ino` sketch. Key sections:

- **`setup()`** — Configures 6 button GPIO pins as `INPUT_PULLUP`, connects to WiFi, sets up SSL cert for Slack API
- **`checkWiFi()`** — Auto-reconnect logic (up to 20 retries), called each loop iteration
- **`loop()`** — Polls all 6 buttons, applies 20ms debounce delay, waits 2 seconds after first press before triggering API call, enforces 15-second cooldown between requests

## Button → Status Mapping

| GPIO | Status Text | Emoji | Presence |
|------|-------------|-------|----------|
| 13 | Walking the dog | `:dog2:` | AWAY |
| 25 | Lunch | `:hamburger:` | AWAY |
| 33 | In a meeting | `:calendar:` | AUTO |
| 18 | Available to chat | `:speech_balloon:` | AUTO |
| 17 | Lurking | `:crystal_ball:` | AWAY |
| 16 | Offline | `:no_entry:` | AWAY |

To add or modify statuses, edit the button-handling `if/else` block in `loop()`.

## Credentials

`credentials.h` is gitignored. It must define:
- WiFi SSID and password
- Slack OAuth token (`xoxp-...`) with scopes `users.profile:write` and `users:write`

See `credentials_example.h` for the required structure.
