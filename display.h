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

// Application callback — must be defined in the sketch (.ino).
// Called by the LVGL button event handler when a status button is tapped.
// buttonIndex: 0=Offline, 1=Lurking, 2=Available, 3=Meeting, 4=Lunch, 5=Walking the dog
void onButtonPressed(int buttonIndex);

// ---- Public API ----
void initDisplay();
void setActiveButton(int idx);       // 0-5; -1 clears all
void setStatusText(const char* text);
void setWiFiConnected(bool connected);

// ---- Private state ----
static Arduino_ESP32RGBPanel *_rgbpanel;
static Arduino_RGB_Display   *_gfx;
static BBCapTouch              _touch;
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

// ---- Touch read callback ----
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

// Enable backlight via CH422G IO expander (I2C must already be initialized).
// Writing 0xFF to register 0x01 sets all open-drain outputs high; on this board
// one of those outputs drives the backlight enable pin.
// Source: https://github.com/Westcott1/Waveshare-ESP32-S3-Touch-LCD-4.3-and-Arduino
static void _enableBacklight() {
    Wire.beginTransmission(CH422G_ADDR);
    Wire.write(0x01);   // CH422G open-drain output register
    Wire.write(0xFF);   // all outputs high (enables backlight)
    Wire.endTransmission();
}

// ---- initDisplay() ----
void initDisplay() {
    Wire.begin(TOUCH_SDA, TOUCH_SCL);  // shared I2C bus for CH422G and GT911

    _rgbpanel = new Arduino_ESP32RGBPanel(
        5  /* DE */,  3  /* VSYNC */, 46 /* HSYNC */, 7  /* PCLK */,
        1  /* R0 */,  2  /* R1 */,   42 /* R2 */,    41 /* R3 */,  40 /* R4 */,
        39 /* G0 */,  0  /* G1 — also TOUCH_RST & boot pin; touch reset pulsed before RGB init */, 45 /* G2 */, 48 /* G3 */, 47 /* G4 */, 21 /* G5 */,
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
        true /* auto_flush — required for Arduino_ESP32RGBPanel continuous DMA; works with LVGL partial buffer on this board (ref: Westcott1 example) */
    );

    _enableBacklight();
    _gfx->begin();
    _gfx->fillScreen(RGB565_BLACK);

    // LVGL init
    lv_init();
    _buf1 = (lv_color_t *)ps_malloc(SCREEN_W * 50 * sizeof(lv_color_t));
    if (!_buf1) {
        Serial.println("FATAL: PSRAM alloc failed — enable OPI PSRAM in board settings");
        while (1) delay(1000);
    }
    lv_disp_draw_buf_init(&_draw_buf, _buf1, NULL, SCREEN_W * 50);

    lv_disp_drv_init(&_disp_drv);
    _disp_drv.hor_res  = SCREEN_W;
    _disp_drv.ver_res  = SCREEN_H;
    _disp_drv.flush_cb = _display_flush;
    _disp_drv.draw_buf = &_draw_buf;
    lv_disp_drv_register(&_disp_drv);

    // Touch init
    _touch.init(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_INT);
    lv_indev_drv_init(&_indev_drv);
    _indev_drv.type    = LV_INDEV_TYPE_POINTER;
    _indev_drv.read_cb = _touchpad_read;
    lv_indev_drv_register(&_indev_drv);
}

// ---- Public API stubs (replaced in Task 4) ----
void setActiveButton(int idx) { _active_btn = idx; }
void setStatusText(const char* text) {}
void setWiFiConnected(bool connected) {}
