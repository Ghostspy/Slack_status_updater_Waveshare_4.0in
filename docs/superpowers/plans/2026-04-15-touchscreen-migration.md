# Touchscreen Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Migrate the Slack status updater from 6 GPIO buttons on an ESP32 D1 Mini to a touchscreen UI on the Waveshare ESP32-S3 Touch LCD 4.3" (SKU 25948).

**Architecture:** Two-file split — `display.h` owns all display/touch/LVGL/UI code and exposes three public functions; the main `.ino` owns WiFi, Slack API, and defines `onButtonPressed(int)` which is forward-declared in `display.h`. LVGL v8.x handles button events, rendering, and touch input. The 2-second rotary-switch settling delay and all GPIO polling code are removed entirely.

**Tech Stack:** LVGL v8.x, Arduino_GFX_Library (Arduino_ESP32RGBPanel + Arduino_RGB_Display), bb_captouch (GT911), ArduinoSlack, ArduinoJson, ESP32 Arduino core v2.x (required for LVGL v8 compatibility)

---

## File Map

| File | Action | Responsibility |
|------|--------|----------------|
| `display.h` | **Create** | Arduino_GFX RGB panel driver, GT911 touch, LVGL init, UI layout, public API |
| `Slack_status_updater_Waveshare_4.0in.ino` | **Rewrite** | WiFi, Slack API, `onButtonPressed()`, setup/loop |
| `~/Documents/Arduino/libraries/lv_conf.h` | **Create** | LVGL v8 configuration (external to project, one level above lvgl/ folder) |

---

### Task 1: Environment Setup

**Files:**
- Create: `~/Documents/Arduino/libraries/lv_conf.h`
- No project files changed

- [ ] **Step 1: Install libraries via Arduino Library Manager**

  Open Arduino IDE → Tools → Manage Libraries. Install:
  - Search `lvgl` → install **lvgl** version **8.x** (do NOT install 9.x — incompatible with esp32 core v2)
  - Search `Arduino_GFX` → install **Arduino_GFX_Library** (latest)
  - Search `bb_captouch` → install **bb_captouch** (latest)

- [ ] **Step 2: Create lv_conf.h**

  ```bash
  cp ~/Documents/Arduino/libraries/lvgl/lv_conf_template.h \
     ~/Documents/Arduino/libraries/lv_conf.h
  ```

  On Windows: copy `Documents\Arduino\libraries\lvgl\lv_conf_template.h` to `Documents\Arduino\libraries\lv_conf.h`

- [ ] **Step 3: Edit lv_conf.h — activate it and configure for this display**

  Open `~/Documents/Arduino/libraries/lv_conf.h` and make these changes:

  ```c
  /* Near the top of the file — change 0 to 1 to activate the config */
  #if 1   /* was: #if 0 */

  /* Horizontal and vertical resolution */
  #define LV_HOR_RES_MAX          800
  #define LV_VER_RES_MAX          480

  /* Color depth: 16-bit RGB565 */
  #define LV_COLOR_DEPTH          16

  /* Use millis() as the LVGL tick source */
  #define LV_TICK_CUSTOM          1
  #define LV_TICK_CUSTOM_INCLUDE  "Arduino.h"
  #define LV_TICK_CUSTOM_SYS_TIME_EXPR    (millis())

  /* Enable Montserrat fonts (search for these lines and set to 1) */
  #define LV_FONT_MONTSERRAT_12   1
  #define LV_FONT_MONTSERRAT_14   1
  ```

  `LV_USE_GRID` and `LV_USE_FLEX` are already enabled by default in LVGL v8 — no change needed.

- [ ] **Step 4: Configure Arduino IDE board settings**

  Tools menu → set all of these:
  - Board: **ESP32S3 Dev Module**
  - Flash Size: **16MB (128Mb)**
  - PSRAM: **OPI PSRAM** ← critical, screen won't work without this
  - Partition Scheme: **Huge APP (3MB No OTA/1MB SPIFFS)**
  - Upload Speed: **921600**

- [ ] **Step 5: Verify a blank sketch compiles for this board**

  Create a new sketch with only `void setup() {}` and `void loop() {}`. Compile (Ctrl+R). Expected: compiles with no errors. If board not found, install **esp32 by Espressif Systems v2.x** via Boards Manager.

---

### Task 2: display.h — Arduino_GFX Display Driver + LVGL

**Files:**
- Create: `display.h`
- Modify: `Slack_status_updater_Waveshare_4.0in.ino` (add temporary test)

- [ ] **Step 1: Create display.h with the full driver and skeleton initDisplay()**

  Create `display.h` in the sketch folder:

  ```cpp
  #pragma once
  #include <Arduino.h>
  #include <Wire.h>
  #include <Arduino_GFX_Library.h>
  #include <bb_captouch.h>
  #include <lvgl.h>

  // ---- Pin definitions (Waveshare ESP32-S3-Touch-LCD-4.3) ----
  // Source: https://github.com/Westcott1/Waveshare-ESP32-S3-Touch-LCD-4.3-and-Arduino
  #define TOUCH_SDA    8
  #define TOUCH_SCL    9
  #define TOUCH_INT    4
  #define TOUCH_RST    0

  // CH422G IO expander I2C address (controls backlight and other peripherals)
  #define CH422G_ADDR  0x24

  // ---- Screen dimensions ----
  #define SCREEN_W     800
  #define SCREEN_H     480
  #define STATUS_BAR_H  48

  // ---- Colors (RGB565 via lv_color_hex) ----
  #define CLR_BG              lv_color_hex(0x111827)
  #define CLR_STATUS_BAR      lv_color_hex(0x1f2937)
  #define CLR_BTN             lv_color_hex(0x1e3a5f)
  #define CLR_BTN_ACTIVE      lv_color_hex(0x14532d)
  #define CLR_BORDER_ACTIVE   lv_color_hex(0x22c55e)
  #define CLR_TEXT            lv_color_hex(0xf9fafb)
  #define CLR_SUBTEXT         lv_color_hex(0x9ca3af)
  #define CLR_WIFI_OK         lv_color_hex(0x22c55e)
  #define CLR_WIFI_BAD        lv_color_hex(0xef4444)

  // ---- Status metadata (index matches button position in UI) ----
  static const int NUM_BUTTONS = 6;
  static const char* BTN_LABELS[NUM_BUTTONS] = {
      "Offline", "Lurking", "Available to chat",
      "In a meeting", "Lunch", "Walking the dog"
  };
  static const char* BTN_PRESENCE[NUM_BUTTONS] = {
      "AWAY", "AWAY", "AUTO", "AUTO", "AWAY", "AWAY"
  };

  // ---- Forward declaration: define this in your .ino ----
  void onButtonPressed(int buttonIndex);

  // ---- Public API ----
  void initDisplay();
  void setActiveButton(int idx);       // 0-5; call with -1 to clear all
  void setStatusText(const char* text);
  void setWiFiConnected(bool connected);

  // ---- Private state ----
  static Arduino_ESP32RGBPanel *_rgbpanel;
  static Arduino_RGB_Display   *_gfx;
  static BBCT                   _touch;
  static lv_disp_draw_buf_t     _draw_buf;
  static lv_color_t            *_buf1;
  static lv_disp_drv_t          _disp_drv;
  static lv_indev_drv_t         _indev_drv;
  static lv_obj_t              *_btns[NUM_BUTTONS];
  static lv_obj_t              *_status_label;
  static lv_obj_t              *_wifi_dot;
  static int                    _active_btn = -1;

  // ---- Display flush callback ----
  static void _display_flush(lv_disp_drv_t *disp, const lv_area_t *area,
                              lv_color_t *color_p) {
      uint32_t w = area->x2 - area->x1 + 1;
      uint32_t h = area->y2 - area->y1 + 1;
      _gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)color_p, w, h);
      lv_disp_flush_ready(disp);
  }

  // ---- Touch read callback (added in Task 3) ----
  static void _touchpad_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
      TOUCHINFO ti;
      if (_touch.getSamples(&ti) && ti.count > 0) {
          data->state   = LV_INDEV_STATE_PR;
          data->point.x = ti.x[0];
          data->point.y = ti.y[0];
      } else {
          data->state = LV_INDEV_STATE_REL;
      }
  }

  // ---- Backlight enable via CH422G IO expander ----
  static void _enableBacklight() {
      Wire.begin(TOUCH_SDA, TOUCH_SCL);
      Wire.beginTransmission(CH422G_ADDR);
      Wire.write(0x01);   // OE register
      Wire.write(0xFF);   // all outputs enabled
      Wire.endTransmission();
  }

  // ---- initDisplay() ----
  void initDisplay() {
      _rgbpanel = new Arduino_ESP32RGBPanel(
          5  /* DE */,  3  /* VSYNC */, 46 /* HSYNC */, 7  /* PCLK */,
          1  /* R0 */,  2  /* R1 */,   42 /* R2 */,    41 /* R3 */,  40 /* R4 */,
          39 /* G0 */,  0  /* G1 */,   45 /* G2 */,    48 /* G3 */,  47 /* G4 */, 21 /* G5 */,
          14 /* B0 */,  38 /* B1 */,   18 /* B2 */,    17 /* B3 */,  10 /* B4 */,
          0  /* hsync_polarity */,
          40 /* hsync_front_porch */, 48 /* hsync_pulse_width */, 88 /* hsync_back_porch */,
          0  /* vsync_polarity */,
          13 /* vsync_front_porch */, 3  /* vsync_pulse_width */, 32 /* vsync_back_porch */,
          1  /* pclk_active_neg */, 16000000 /* prefer_speed */
      );

      _gfx = new Arduino_RGB_Display(
          SCREEN_W, SCREEN_H, _rgbpanel,
          0    /* rotation */,
          true /* auto_flush */
      );

      _enableBacklight();
      _gfx->begin();
      _gfx->fillScreen(BLACK);

      // LVGL init
      lv_init();
      _buf1 = (lv_color_t *)ps_malloc(SCREEN_W * 50 * sizeof(lv_color_t));
      lv_disp_draw_buf_init(&_draw_buf, _buf1, NULL, SCREEN_W * 50);

      lv_disp_drv_init(&_disp_drv);
      _disp_drv.hor_res  = SCREEN_W;
      _disp_drv.ver_res  = SCREEN_H;
      _disp_drv.flush_cb = _display_flush;
      _disp_drv.draw_buf = &_draw_buf;
      lv_disp_drv_register(&_disp_drv);

      // Touch init (added fully in Task 3)
      _touch.init(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_INT);
      lv_indev_drv_init(&_indev_drv);
      _indev_drv.type    = LV_INDEV_TYPE_POINTER;
      _indev_drv.read_cb = _touchpad_read;
      lv_indev_drv_register(&_indev_drv);
  }

  // ---- Public API stubs (filled in Task 4) ----
  void setActiveButton(int idx) { _active_btn = idx; }
  void setStatusText(const char* text) {}
  void setWiFiConnected(bool connected) {}
  ```

- [ ] **Step 2: Add minimal main .ino that calls initDisplay() and fills screen**

  Replace the full contents of `Slack_status_updater_Waveshare_4.0in.ino` with:

  ```cpp
  #include "display.h"

  void onButtonPressed(int idx) {
      Serial.printf("Button %d tapped\n", idx);
  }

  void setup() {
      Serial.begin(115200);
      initDisplay();

      // Temp: fill screen red to confirm display is working
      lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xFF0000), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, LV_PART_MAIN);
      lv_timer_handler();
  }

  void loop() {
      lv_timer_handler();
      delay(5);
  }
  ```

- [ ] **Step 3: Compile and flash**

  Expected: screen fills solid red. If the screen stays black:
  - Confirm PSRAM is set to **OPI PSRAM** in board settings
  - Confirm `ps_malloc` isn't returning NULL (add `Serial.println(_buf1 ? "buf OK" : "buf NULL")` after the ps_malloc call)
  - If backlight doesn't turn on, check the CH422G I2C address — some boards use `0x20` instead of `0x24`. Try both.

  > **Note on GPIO0:** `TOUCH_RST` and the RGB panel `G1` both map to GPIO0, which is what the Westcott1 reference example uses. Touch reset is toggled once during init and then released, after which GPIO0 is driven by the RGB panel. This works in practice because the two uses don't overlap in time.

- [ ] **Step 4: Remove the red fill test, commit**

  Remove the two temp lines (`lv_obj_set_style_bg_color`, `lv_obj_set_style_bg_opa`, `lv_timer_handler()`) from setup().

  ```bash
  git add display.h Slack_status_updater_Waveshare_4.0in.ino
  git commit -m "feat: add display.h with Arduino_GFX RGB panel + LVGL display driver"
  ```

---

### Task 3: Verify Touch Input

**Files:**
- Modify: `Slack_status_updater_Waveshare_4.0in.ino` (add temporary touch debug)

Touch driver code is already wired in `display.h` from Task 2. This task verifies it works.

- [ ] **Step 1: Add touch debug logging to onButtonPressed**

  The `_touchpad_read` callback fires LVGL pointer events, but `onButtonPressed` won't fire until buttons exist (Task 4). Instead, add a raw touch poll to the loop temporarily:

  In `Slack_status_updater_Waveshare_4.0in.ino`, add to `loop()`:
  ```cpp
  // Temp: raw touch debug
  static BBCT dbgTouch;
  static bool touchInited = false;
  if (!touchInited) {
      dbgTouch.init(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_INT);
      touchInited = true;
  }
  TOUCHINFO ti;
  if (dbgTouch.getSamples(&ti) && ti.count > 0) {
      Serial.printf("Touch: x=%d y=%d\n", ti.x[0], ti.y[0]);
      delay(100);
  }
  ```

- [ ] **Step 2: Compile, flash, open Serial Monitor at 115200 baud**

  Touch the screen in several locations. Expected: `Touch: x=NNN y=NNN` lines appear in the serial monitor. Verify x ranges roughly 0–800 and y ranges 0–480.

  If no output appears: check that TOUCH_SDA=8, TOUCH_SCL=9 are correct for your board revision. I2C scan the bus to confirm GT911 is detected at address 0x5D or 0x14.

- [ ] **Step 3: Remove the debug touch code from loop(), commit**

  ```bash
  git add Slack_status_updater_Waveshare_4.0in.ino
  git commit -m "test: verify GT911 touch input on ESP32-S3"
  ```

---

### Task 4: display.h — LVGL UI (Status Bar + Button Grid)

**Files:**
- Modify: `display.h`

- [ ] **Step 1: Add _btn_event_cb and _createUI() to display.h**

  Add these two functions to `display.h`, after `_enableBacklight()` and before `initDisplay()`:

  ```cpp
  static void _btn_event_cb(lv_event_t *e) {
      int idx = (int)(intptr_t)lv_event_get_user_data(e);
      onButtonPressed(idx);
  }

  static void _createUI() {
      // Screen background
      lv_obj_set_style_bg_color(lv_scr_act(), CLR_BG, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, LV_PART_MAIN);

      // ---- Status bar ----
      lv_obj_t *bar = lv_obj_create(lv_scr_act());
      lv_obj_set_size(bar, SCREEN_W, STATUS_BAR_H);
      lv_obj_set_pos(bar, 0, 0);
      lv_obj_set_style_bg_color(bar, CLR_STATUS_BAR, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
      lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
      lv_obj_set_style_pad_all(bar, 0, LV_PART_MAIN);
      lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

      _status_label = lv_label_create(bar);
      lv_label_set_text(_status_label, "No status set");
      lv_obj_set_style_text_color(_status_label, CLR_TEXT, LV_PART_MAIN);
      lv_obj_set_style_text_font(_status_label, &lv_font_montserrat_14, LV_PART_MAIN);
      lv_obj_align(_status_label, LV_ALIGN_LEFT_MID, 12, 0);

      _wifi_dot = lv_obj_create(bar);
      lv_obj_set_size(_wifi_dot, 14, 14);
      lv_obj_align(_wifi_dot, LV_ALIGN_RIGHT_MID, -12, 0);
      lv_obj_set_style_radius(_wifi_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
      lv_obj_set_style_bg_color(_wifi_dot, CLR_WIFI_BAD, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(_wifi_dot, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_border_width(_wifi_dot, 0, LV_PART_MAIN);

      // ---- Button grid ----
      lv_obj_t *grid = lv_obj_create(lv_scr_act());
      lv_obj_set_size(grid, SCREEN_W, SCREEN_H - STATUS_BAR_H);
      lv_obj_set_pos(grid, 0, STATUS_BAR_H);
      lv_obj_set_style_bg_color(grid, CLR_BG, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(grid, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_border_width(grid, 0, LV_PART_MAIN);
      lv_obj_set_style_radius(grid, 0, LV_PART_MAIN);
      lv_obj_set_style_pad_all(grid, 8, LV_PART_MAIN);
      lv_obj_set_style_pad_gap(grid, 8, LV_PART_MAIN);
      lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

      static lv_coord_t col_dsc[] = {
          LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST
      };
      static lv_coord_t row_dsc[] = {
          LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST
      };
      lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
      lv_obj_set_layout(grid, LV_LAYOUT_GRID);

      for (int i = 0; i < NUM_BUTTONS; i++) {
          lv_obj_t *btn = lv_btn_create(grid);
          lv_obj_set_grid_cell(btn,
              LV_GRID_ALIGN_STRETCH, i % 3, 1,   // col = i % 3
              LV_GRID_ALIGN_STRETCH, i / 3, 1);   // row = i / 3
          lv_obj_set_style_bg_color(btn, CLR_BTN,
              LV_PART_MAIN | LV_STATE_DEFAULT);
          lv_obj_set_style_bg_opa(btn, LV_OPA_COVER,
              LV_PART_MAIN | LV_STATE_DEFAULT);
          lv_obj_set_style_border_width(btn, 2,
              LV_PART_MAIN | LV_STATE_DEFAULT);
          lv_obj_set_style_border_color(btn, CLR_BTN,
              LV_PART_MAIN | LV_STATE_DEFAULT);
          lv_obj_set_style_radius(btn, 10, LV_PART_MAIN);
          lv_obj_add_event_cb(btn, _btn_event_cb, LV_EVENT_CLICKED,
                              (void *)(intptr_t)i);
          lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
          lv_obj_set_flex_align(btn,
              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

          lv_obj_t *name = lv_label_create(btn);
          lv_label_set_text(name, BTN_LABELS[i]);
          lv_obj_set_style_text_color(name, CLR_TEXT, LV_PART_MAIN);
          lv_obj_set_style_text_font(name, &lv_font_montserrat_14, LV_PART_MAIN);

          lv_obj_t *presence = lv_label_create(btn);
          lv_label_set_text(presence, BTN_PRESENCE[i]);
          lv_obj_set_style_text_color(presence, CLR_SUBTEXT, LV_PART_MAIN);
          lv_obj_set_style_text_font(presence, &lv_font_montserrat_12, LV_PART_MAIN);

          _btns[i] = btn;
      }
  }
  ```

- [ ] **Step 2: Replace the public API stubs in display.h with real implementations**

  Find and replace the three stub functions at the bottom of `display.h`:

  ```cpp
  void setActiveButton(int idx) {
      for (int i = 0; i < NUM_BUTTONS; i++) {
          if (i == idx) {
              lv_obj_set_style_bg_color(_btns[i], CLR_BTN_ACTIVE,
                  LV_PART_MAIN | LV_STATE_DEFAULT);
              lv_obj_set_style_border_color(_btns[i], CLR_BORDER_ACTIVE,
                  LV_PART_MAIN | LV_STATE_DEFAULT);
          } else {
              lv_obj_set_style_bg_color(_btns[i], CLR_BTN,
                  LV_PART_MAIN | LV_STATE_DEFAULT);
              lv_obj_set_style_border_color(_btns[i], CLR_BTN,
                  LV_PART_MAIN | LV_STATE_DEFAULT);
          }
      }
      _active_btn = idx;
  }

  void setStatusText(const char* text) {
      lv_label_set_text(_status_label, text);
  }

  void setWiFiConnected(bool connected) {
      lv_obj_set_style_bg_color(_wifi_dot,
          connected ? CLR_WIFI_OK : CLR_WIFI_BAD,
          LV_PART_MAIN);
  }
  ```

- [ ] **Step 3: Call _createUI() at the end of initDisplay()**

  Append to the very end of `initDisplay()`:
  ```cpp
      _createUI();
  ```

- [ ] **Step 4: Compile and flash — verify UI renders and buttons fire**

  Expected:
  - Status bar at top with "No status set" text (left) and a red dot (right)
  - 3×2 grid of dark blue buttons: Offline, Lurking, Available to chat (top row); In a meeting, Lunch, Walking the dog (bottom row)
  - Tapping any button prints `Button N tapped` in Serial Monitor at 115200 baud

  If button labels are invisible: confirm `LV_FONT_MONTSERRAT_12` and `LV_FONT_MONTSERRAT_14` are set to `1` in `lv_conf.h`.

- [ ] **Step 5: Commit**

  ```bash
  git add display.h
  git commit -m "feat: add LVGL UI to display.h — status bar, 3x2 button grid, helper functions"
  ```

---

### Task 5: Rewrite Main .ino — Slack API Integration

**Files:**
- Rewrite: `Slack_status_updater_Waveshare_4.0in.ino`

- [ ] **Step 1: Replace the full .ino with the final version**

  ```cpp
  #include <WiFi.h>
  #include <WiFiClientSecure.h>
  #include "credentials.h"
  #include "display.h"

  #include <ArduinoSlack.h>
  #include <ArduinoSlackCert.h>
  #include <ArduinoJson.h>

  char ssid[]     = WIFI_SSID;
  char password[] = WIFI_PASSWORD;

  WiFiClientSecure client;
  ArduinoSlack slack(client, SLACK_ACCESS_TOKEN);

  // Status definitions — index must match button order in display.h
  // 0=Offline  1=Lurking  2=Available to chat
  // 3=Meeting  4=Lunch    5=Walking the dog
  struct StatusDef {
      const char *text;
      const char *emoji;
      bool        away;   // true = SLACK_PRESENCE_AWAY, false = SLACK_PRESENCE_AUTO
  };

  static const StatusDef STATUSES[6] = {
      { "Offline",            ":no_entry:",       true  },
      { "Lurking",            ":crystal_ball:",   true  },
      { "Available to chat",  ":speech_balloon:", false },
      { "In a meeting",       ":calendar:",       false },
      { "Lunch",              ":hamburger:",      true  },
      { "Walking the dog",    ":dog2:",           true  },
  };

  // Called by display.h LVGL event callback on every button tap
  void onButtonPressed(int idx) {
      if (idx < 0 || idx >= 6) return;
      Serial.printf("Button %d: %s\n", idx, STATUSES[idx].text);

      slack.setPresence(STATUSES[idx].away
          ? SLACK_PRESENCE_AWAY
          : SLACK_PRESENCE_AUTO);

      SlackProfile profile = slack.setCustomStatus(
          STATUSES[idx].text,
          STATUSES[idx].emoji
      );

      if (!profile.error) {
          setActiveButton(idx);
          setStatusText(STATUSES[idx].text);
          Serial.printf("Status updated: %s\n", STATUSES[idx].text);
      } else {
          Serial.println("Slack API error — status not updated");
      }
  }

  void checkWiFi() {
      bool connected = (WiFi.status() == WL_CONNECTED);
      setWiFiConnected(connected);

      if (!connected) {
          Serial.println("WiFi lost, reconnecting...");
          WiFi.disconnect();
          WiFi.begin(ssid, password);
          int retries = 0;
          while (WiFi.status() != WL_CONNECTED && retries < 20) {
              delay(500);
              lv_timer_handler();
              Serial.print(".");
              retries++;
          }
          bool reconnected = (WiFi.status() == WL_CONNECTED);
          setWiFiConnected(reconnected);
          if (reconnected) {
              Serial.printf("\nReconnected. IP: %s\n",
                  WiFi.localIP().toString().c_str());
          } else {
              Serial.println("\nReconnect failed, will retry next loop.");
          }
      }
  }

  void setup() {
      Serial.begin(115200);

      initDisplay();

      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid, password);
      Serial.print("Connecting to WiFi");
      while (WiFi.status() != WL_CONNECTED) {
          delay(500);
          lv_timer_handler();   // keep display responsive during WiFi connect
          Serial.print(".");
      }
      Serial.printf("\nConnected. IP: %s\n", WiFi.localIP().toString().c_str());
      setWiFiConnected(true);

      client.setCACert(slack_server_cert);
  }

  void loop() {
      lv_timer_handler();
      delay(5);
      checkWiFi();
  }
  ```

- [ ] **Step 2: Compile**

  Expected: no errors. Common issues:
  - `onButtonPressed` redefinition: remove any leftover stub from earlier tasks
  - `SLACK_PRESENCE_AWAY` / `SLACK_PRESENCE_AUTO` undefined: confirm `ArduinoSlack.h` is included

- [ ] **Step 3: Flash and test end-to-end**

  - Screen shows status bar with red WiFi dot while connecting, turns green on connect
  - Tap **Offline** (top-left): serial shows `Button 0: Offline`, Slack status updates to ⛔ Offline, button highlights green, status bar shows "Offline"
  - Tap **Lurking**: previous button returns to blue, Lurking button highlights, Slack updates
  - Verify all 6 buttons update Slack correctly and presence (AWAY/AUTO) matches the table in the spec

- [ ] **Step 4: Commit**

  ```bash
  git add Slack_status_updater_Waveshare_4.0in.ino
  git commit -m "feat: rewrite main .ino for touchscreen — Slack callbacks, WiFi, LVGL loop"
  ```

---

### Task 6: Documentation + Cleanup

**Files:**
- Modify: `README.md`
- Modify: `CLAUDE.md`
- Modify: `.gitignore`

- [ ] **Step 1: Update README.md Hardware section**

  Replace the Hardware section with:

  ```markdown
  ## Hardware

  | Component | Link |
  |-----------|------|
  | Waveshare ESP32-S3 Touch LCD 4.3" (SKU 25948) | [Waveshare](https://www.waveshare.com/esp32-s3-touch-lcd-4.3.htm) |

  **Arduino IDE board settings:**
  - Board: ESP32S3 Dev Module
  - Flash Size: 16MB
  - PSRAM: OPI PSRAM (required)
  - esp32 Arduino core: v2.x (required for LVGL v8 compatibility — do not use v3.x)
  ```

- [ ] **Step 2: Update README.md Dependencies section**

  Replace the Dependencies table with:

  ```markdown
  | Library | Version | Install via |
  |---------|---------|-------------|
  | [lvgl](https://github.com/lvgl/lvgl) | v8.x (NOT v9) | Arduino Library Manager |
  | [Arduino_GFX_Library](https://github.com/moononournation/Arduino_GFX) | latest | Arduino Library Manager |
  | [bb_captouch](https://github.com/bitbank2/bb_captouch) | latest | Arduino Library Manager |
  | [ArduinoSlack](https://github.com/witnessmenow/arduino-slack-api) | — | GitHub (manual install) |
  | [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | — | Arduino Library Manager |
  ```

- [ ] **Step 3: Update README.md Setup section**

  Add a new **Step 1: Configure lv_conf.h** before the existing Slack App step, and renumber:

  ```markdown
  ### 1. Configure lv_conf.h

  Copy the LVGL config template one level above the lvgl library folder and activate it:

  ```bash
  cp ~/Documents/Arduino/libraries/lvgl/lv_conf_template.h \
     ~/Documents/Arduino/libraries/lv_conf.h
  ```

  Open `lv_conf.h` and change the first `#if 0` near the top to `#if 1`.
  ```

  Remove the old hardware wiring paragraph and GPIO pin table from the Setup section (they no longer apply).

- [ ] **Step 4: Update CLAUDE.md**

  Replace the full contents of `CLAUDE.md` with:

  ````markdown
  # CLAUDE.md

  This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

  ## Project Overview

  Arduino sketch for an ESP32-S3-based physical Slack status updater with touchscreen UI. Tapping one of six on-screen buttons triggers Slack API calls to set status text, emoji, and presence (AWAY/AUTO).

  ## Build & Flash

  This is an Arduino project — compilation and flashing are done through **Arduino IDE**:

  1. Create `credentials.h` from `credentials_example.h` and fill in WiFi + Slack token
  2. Create `lv_conf.h` (see below)
  3. Open `Slack_status_updater_Waveshare_4.0in.ino` in Arduino IDE
  4. Select board **ESP32S3 Dev Module**, Flash **16MB**, PSRAM **OPI PSRAM**
  5. Click **Upload**

  Serial monitor baud rate: **115200**

  ## lv_conf.h Setup (required, one-time)

  ```bash
  cp ~/Documents/Arduino/libraries/lvgl/lv_conf_template.h \
     ~/Documents/Arduino/libraries/lv_conf.h
  ```

  In `lv_conf.h`: change `#if 0` near the top to `#if 1`.

  ## Required Libraries

  Install via Arduino Library Manager:
  - `lvgl` — **v8.x only** (v9.x is incompatible with esp32 core v2)
  - `Arduino_GFX_Library`
  - `bb_captouch`

  Install manually (GitHub):
  - `ArduinoSlack` — https://github.com/witnessmenow/arduino-slack-api

  esp32 Arduino core must be **v2.x** (not v3.x).

  ## Architecture

  Two-file structure:

  - **`display.h`** — owns everything visual: Arduino_ESP32RGBPanel + Arduino_RGB_Display for the ST7701 RGB display, bb_captouch for GT911 touch, LVGL v8.x init, status bar, 3×2 button grid. Exposes `initDisplay()`, `setActiveButton(int)`, `setStatusText(const char*)`, `setWiFiConnected(bool)`. Forward-declares `onButtonPressed(int)` which must be defined in the `.ino`.
  - **`Slack_status_updater_Waveshare_4.0in.ino`** — owns WiFi, ArduinoSlack, and `onButtonPressed()`. Loop calls `lv_timer_handler()` every 5ms to drive LVGL, plus `checkWiFi()`.

  ## Button → Status Mapping

  | Index | Status Text | Slack Emoji | Presence |
  |-------|-------------|-------------|----------|
  | 0 | Offline | `:no_entry:` | AWAY |
  | 1 | Lurking | `:crystal_ball:` | AWAY |
  | 2 | Available to chat | `:speech_balloon:` | AUTO |
  | 3 | In a meeting | `:calendar:` | AUTO |
  | 4 | Lunch | `:hamburger:` | AWAY |
  | 5 | Walking the dog | `:dog2:` | AWAY |

  Index order matches left-to-right, top-to-bottom in the UI (Offline = top-left).

  ## Credentials

  `credentials.h` is gitignored. Requires Slack OAuth token with scopes `users.profile:write` and `users:write`. See `credentials_example.h`.
  ````

- [ ] **Step 5: Add .superpowers/ to .gitignore**

  ```bash
  echo "" >> .gitignore
  echo "# Superpowers brainstorm session files" >> .gitignore
  echo ".superpowers/" >> .gitignore
  ```

- [ ] **Step 6: Final commit**

  ```bash
  git add README.md CLAUDE.md .gitignore
  git commit -m "docs: update README and CLAUDE.md for touchscreen hardware"
  ```
