# Slack Status Updater — Waveshare ESP32-S3 Touch LCD 4.3"

A physical Slack status controller built on a Waveshare ESP32-S3 Touch LCD 4.3B". Tap one of six on-screen buttons to instantly update your Slack status and presence without touching your computer — useful for "In a meeting", "Lunch", "Walking the dog", and similar states you switch between throughout the day.

On boot it reads your current Slack status and highlights the matching button automatically.

---

## Hardware

| Component | Details |
|-----------|---------|
| [Waveshare ESP32-S3 Touch LCD 4.3B](https://www.waveshare.com/esp32-s3-touch-lcd-4.3b.htm) | ESP32-S3, 800×480 RGB LCD, GT911 capacitive touch, CH422G IO expander |

No physical buttons or external wiring required — the display is self-contained.

---

## Dependencies

Install the following libraries before compiling:

| Library | Version | Install via |
|---------|---------|-------------|
| [LVGL](https://github.com/lvgl/lvgl) | **9.5.0** (exactly, or later 9.x) | Arduino Library Manager |
| [Arduino_GFX_Library](https://github.com/moononournation/Arduino_GFX) | latest (1.6.x) | Arduino Library Manager |
| [ArduinoSlack](https://github.com/witnessmenow/arduino-slack-api) | latest | GitHub (manual install) |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | **v6.x** | Arduino Library Manager — search "Arduino Json" |

> **ArduinoJson version:** The ArduinoSlack library bundles ArduinoJson v6. Use v6.x, not v7 — `DynamicJsonDocument` is the v6 API used here.

`WiFi` and `WiFiClientSecure` are included with the ESP32 Arduino core.

### LVGL configuration

After installing LVGL, copy `lv_conf_template.h` to the Arduino libraries folder (one level **above** the `lvgl/` folder) and rename it `lv_conf.h`:

```bash
cp ~/Documents/Arduino/libraries/lvgl/lv_conf_template.h \
   ~/Documents/Arduino/libraries/lv_conf.h
```

Then edit `lv_conf.h` and set:

```c
#if 1                          // line 15 — change from #if 0 to activate
#define LV_COLOR_DEPTH 16
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
```

> `LV_HOR_RES_MAX`, `LV_VER_RES_MAX`, and `LV_TICK_CUSTOM` do **not** exist in LVGL v9 — do not add them. Resolution and tick are set in code.

---

## Board Settings (Arduino IDE)

| Setting | Value |
|---------|-------|
| Board | **ESP32S3 Dev Module** |
| Flash Size | **16MB (128Mb)** |
| PSRAM | **OPI PSRAM** (required — display will not work without this) |
| Partition Scheme | **Huge APP (3MB No OTA/1MB SPIFFS)** (required — sketch exceeds default 1.25 MB limit) |
| Upload Speed | **921600** |
| ESP32 Arduino core | **v3.x latest** |

---

## Setup

### 1. Create a Slack App and get an OAuth token

1. Go to [https://api.slack.com/apps](https://api.slack.com/apps) and click **Create New App** → **From scratch**.
2. Give it a name and select your workspace, then click **Create App**.
3. In the left sidebar, go to **OAuth & Permissions**.
4. Scroll to **User Token Scopes** and add:
   - `users.profile:write`
   - `users:write`
5. Scroll back to the top and click **Install to Workspace**, then **Allow**.
6. Copy the **User OAuth Token** — it starts with `xoxp-`.

### 2. Configure credentials

```bash
cp credentials_example.h credentials.h
```

Edit `credentials.h` and fill in:

```cpp
#define WIFI_SSID          "your-network-name"
#define WIFI_PASSWORD      "your-network-password"
#define SLACK_ACCESS_TOKEN "xoxp-your-token-here"
```

`credentials.h` is gitignored and should never be committed.

### 3. Flash the ESP32-S3

1. Open `Slack_status_updater_Waveshare_4.0in.ino` in **Arduino IDE**.
2. Under **Tools → Board**, select **ESP32S3 Dev Module**.
3. Apply the board settings listed above.
4. Select the correct COM port under **Tools → Port**.
5. Click **Upload**.

To monitor output, open the Serial Monitor at **115200 baud**.

---

## UI Layout

The screen is divided into a status bar and a 3×2 button grid.

**Status bar** (top, 48 px): WiFi indicator dot on the left (green = connected, red = disconnected), currently active status text centered.

**Button grid:** Each button shows a vector icon above its label. The active button is highlighted with a green background and border.

| Col 1 | Col 2 | Col 3 |
|-------|-------|-------|
| Offline | Lurking | Available to chat |
| In a meeting | Lunch | Walking the dog |

---

## Button → Status Reference

| Index | Status Text | Emoji | Presence |
|-------|-------------|-------|----------|
| 0 | Offline | `:no_entry:` | Away |
| 1 | Lurking | `:eyes:` | Away |
| 2 | Available to chat | `:speech_balloon:` | Auto |
| 3 | In a meeting | `:calendar:` | Auto |
| 4 | Lunch | `:fork_and_knife:` | Away |
| 5 | Walking the dog | `:dog:` | Away |

---

## Customization

Status text and presence are defined in `display.h`:

- `BTN_LABELS[]` — status text shown on each button and sent to Slack as status text
- `BTN_PRESENCE[]` — `"AWAY"` or `"AUTO"` for each button

Slack emoji codes are defined in the main `.ino`:

- `BTN_EMOJI[]` — Slack emoji string for each button (e.g. `":no_entry:"`)
- `BTN_PRESENCE_API[]` — `"away"` or `"auto"` passed to the Slack presence API

To add or change a status, update both `BTN_LABELS[]` / `BTN_PRESENCE[]` in `display.h` and `BTN_EMOJI[]` / `BTN_PRESENCE_API[]` in the `.ino`.

---

## File Structure

```
Slack_status_updater_Waveshare_4.0in.ino  — WiFi, Slack API calls, onButtonPressed(), main loop
display.h                                  — RGB panel driver, GT911 touch, LVGL v9 init, UI + icons
credentials.h                              — WiFi + Slack token (gitignored)
credentials_example.h                      — Template for credentials.h
```

---

## License

GNU General Public License v3.0 — see [LICENSE](LICENSE).
