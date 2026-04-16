#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>

// ---- Pin definitions (Waveshare ESP32-S3-Touch-LCD-4.3B) ----
#define TOUCH_SDA    8
#define TOUCH_SCL    9
#define TOUCH_INT    4
// NOTE: TP_RST on the 4.3B is wired to CH422G EXIO1, NOT a direct ESP32 GPIO.
// The reset is performed via I2C in _initCH422G() below.

// CH422G IO expander I2C address
// EXIO1 = TP_RST (touch reset, active low)
// EXIO2 = DISP   (backlight enable, active high)
#define CH422G_ADDR  0x24

// GT911 touch controller — fixed at 0x5D (INT LOW at RST rising edge)
#define GT911_ADDR        0x5D
#define GT911_REG_STATUS  0x814E  // bit7=buf ready, bits[3:0]=touch count
#define GT911_REG_POINT0  0x8150  // first touch point: trackID, X(BE), Y(BE), area, rsvd

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
static lv_color_t            *_buf1;
static lv_obj_t              *_btns[NUM_BUTTONS];
static lv_obj_t              *_status_label;
static lv_obj_t              *_wifi_dot;
static int                    _active_btn = -1;

// ---- Display flush callback (LVGL v9, DIRECT mode) ----
// LVGL renders directly into _buf1 which IS the hardware framebuffer.
// Multiple dirty regions may be flushed per render cycle. We defer the
// cache writeback until lv_display_flush_is_last() so the DMA sees a
// complete frame rather than sequential partial updates.
static void _display_flush(lv_display_t *disp, const lv_area_t *area,
                            uint8_t *px_map) {
    if (lv_display_flush_is_last(disp)) {
        Cache_WriteBack_Addr((uint32_t)_buf1,
                             SCREEN_W * SCREEN_H * sizeof(uint16_t));
    }
    lv_display_flush_ready(disp);
}

// ---- GT911 direct register access ----
// bb_captouch is NOT used — it calls Wire.begin() internally which destroys
// the i2c-ng bus handle and causes INVALID_STATE on the first transaction.
// Direct Wire access works reliably without any library reinit.

static void _gt911_read(uint16_t reg, uint8_t *buf, uint8_t len) {
    Wire.beginTransmission(GT911_ADDR);
    Wire.write((uint8_t)(reg >> 8));
    Wire.write((uint8_t)(reg & 0xFF));
    Wire.endTransmission(false);  // repeated START
    Wire.requestFrom((uint8_t)GT911_ADDR, len);
    for (uint8_t i = 0; i < len; i++)
        buf[i] = Wire.available() ? Wire.read() : 0;
}

static void _gt911_write_byte(uint16_t reg, uint8_t val) {
    Wire.beginTransmission(GT911_ADDR);
    Wire.write((uint8_t)(reg >> 8));
    Wire.write((uint8_t)(reg & 0xFF));
    Wire.write(val);
    Wire.endTransmission(true);
}

// ---- Touch read callback (LVGL v9) ----
static void _touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
    uint8_t status = 0;
    _gt911_read(GT911_REG_STATUS, &status, 1);

    uint8_t count = status & 0x0F;
    if ((status & 0x80) && count > 0) {
        uint8_t pt[8] = {};
        _gt911_read(GT911_REG_POINT0, pt, 8);
        data->state   = LV_INDEV_STATE_PRESSED;
        data->point.x = (int16_t)((pt[1] << 8) | pt[2]);
        data->point.y = (int16_t)((pt[3] << 8) | pt[4]);
        // Clamp: touch sensor extends slightly beyond display glass
        if (data->point.x >= SCREEN_W) data->point.x = SCREEN_W - 1;
        if (data->point.y >= SCREEN_H) data->point.y = SCREEN_H - 1;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
    // Clear buffer status so GT911 can write the next sample
    if (status & 0x80) _gt911_write_byte(GT911_REG_STATUS, 0x00);
}

// Initialize CH422G IO expander: reset GT911 touch controller and enable backlight.
//
// On the 4.3B, GT911's reset line (TP_RST) routes through CH422G EXIO1 — there is
// no direct ESP32 GPIO for touch reset. Without this sequence the GT911 never exits
// reset and NACKs all I2C transactions.
//
// The CH422G uses MULTIPLE I2C device addresses for different operations —
// it is NOT a single-address register-mapped device:
//   0x24 (CH422G_Mode)   : write 1 mode byte (IO_OE bit enables IO0-7 as outputs)
//   0x38 (CH422G_IO_OUT) : write 1 output byte (bitmask, 1=high per IO pin)
//
// IO pin mapping (4.3B):
//   IO1 (bit 0x02) = TP_RST  — touch reset, active low
//   IO2 (bit 0x04) = DISP    — backlight enable, active high
//   IO3 (bit 0x08) = LCD_RST — LCD reset, active low
//
// GT911 I2C address selection: determined by TOUCH_INT level at RST rising edge
//   TOUCH_INT LOW  → 0x5D  (official 4.3B default)
//   TOUCH_INT HIGH → 0x14
static void _initCH422G() {
    // Enable IO0-7 as outputs (CH422G_Mode = 0x24, IO_OE flag = 0x01)
    Wire.beginTransmission(0x24);
    Wire.write(0x01);
    Wire.endTransmission();

    // Set all IO outputs HIGH: backlight on, both RSTs released
    Wire.beginTransmission(0x38);
    Wire.write(0xFF);
    Wire.endTransmission();

    // Step 1: Drive INT LOW before RST to latch GT911 address 0x5D
    pinMode(TOUCH_INT, OUTPUT);
    digitalWrite(TOUCH_INT, LOW);
    delay(10);

    // Step 2: Assert TP_RST LOW (IO1=0), keep all other IOs HIGH
    Wire.beginTransmission(0x38);
    Wire.write(0xFF & ~0x02);   // 0xFD — IO1 low, everything else high
    Wire.endTransmission();
    delay(100);

    // Step 3: Release TP_RST HIGH — GT911 latches address 0x5D and boots
    Wire.beginTransmission(0x38);
    Wire.write(0xFF);
    Wire.endTransmission();
    delay(200);   // GT911 requires ~200 ms after RST release before I2C is ready

    // Step 4: Return INT to input (address is now permanently latched)
    pinMode(TOUCH_INT, INPUT);
}

// ---- Button event callback ----
static void _btn_event_cb(lv_event_t *e) {
    lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (_btns[i] == btn) {
            onButtonPressed(i);
            return;
        }
    }
}

// ---- Button icon drawing ----
// Each button gets a simple vector icon drawn in its LV_EVENT_DRAW_MAIN callback.
// Shapes are basic LVGL primitives — no image files or extra fonts required.

static void _fill_ri(lv_layer_t *layer,
                     int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                     lv_color_t color, int32_t radius) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color     = color;
    d.bg_opa       = LV_OPA_COVER;
    d.radius       = radius;
    d.border_width = 0;
    lv_area_t a    = {x1, y1, x2, y2};
    lv_draw_rect(layer, &d, &a);
}

static void _draw_icon(lv_layer_t *layer, int32_t cx, int32_t cy, int idx) {
    const lv_color_t W  = lv_color_hex(0xf9fafb);  // near-white
    const lv_color_t BL = lv_color_hex(0x60a5fa);  // sky blue
    const lv_color_t RD = lv_color_hex(0xef4444);  // red
    const lv_color_t DK = lv_color_hex(0x0f172a);  // very dark (pupils)

    switch (idx) {

        case 0: // Offline — red no-entry circle with white bar
            _fill_ri(layer, cx-46, cy-46, cx+46, cy+46, RD, LV_RADIUS_CIRCLE);
            _fill_ri(layer, cx-30, cy-10, cx+30, cy+10, W,  8);
            break;

        case 1: // Lurking — pair of eyes with pupils
            _fill_ri(layer, cx-44, cy-18, cx-8,  cy+18, W,  LV_RADIUS_CIRCLE);
            _fill_ri(layer, cx+8,  cy-18, cx+44, cy+18, W,  LV_RADIUS_CIRCLE);
            _fill_ri(layer, cx-31, cy-9,  cx-21, cy+9,  DK, LV_RADIUS_CIRCLE);
            _fill_ri(layer, cx+21, cy-9,  cx+31, cy+9,  DK, LV_RADIUS_CIRCLE);
            _fill_ri(layer, cx-27, cy-5,  cx-22, cy+0,  W,  LV_RADIUS_CIRCLE);
            _fill_ri(layer, cx+29, cy-5,  cx+34, cy+0,  W,  LV_RADIUS_CIRCLE);
            break;

        case 2: { // Available to chat — speech bubble with tail + text lines
            _fill_ri(layer, cx-44, cy-34, cx+44, cy+20, BL, 14);
            _fill_ri(layer, cx-38, cy+16, cx-18, cy+38, BL, 0);
            _fill_ri(layer, cx-32, cy-20, cx+32, cy-12, W,  4);
            _fill_ri(layer, cx-32, cy-4,  cx+22, cy+4,  W,  4);
            _fill_ri(layer, cx-32, cy+12, cx+8,  cy+20, W,  4);
            break;
        }

        case 3: { // In a meeting — calendar outline with header and date grid
            lv_draw_rect_dsc_t cd;
            lv_draw_rect_dsc_init(&cd);
            cd.bg_opa       = LV_OPA_TRANSP;
            cd.border_color = W;
            cd.border_width = 3;
            cd.border_opa   = LV_OPA_COVER;
            cd.radius       = 8;
            lv_area_t ca    = {cx-42, cy-26, cx+42, cy+46};
            lv_draw_rect(layer, &cd, &ca);
            // Header (rounded top, straight bottom)
            _fill_ri(layer, cx-42, cy-26, cx+42, cy-6,  BL, 8);
            _fill_ri(layer, cx-42, cy-16, cx+42, cy-6,  BL, 0);
            // Ring hangers above calendar
            _fill_ri(layer, cx-24, cy-42, cx-14, cy-22, W,  4);
            _fill_ri(layer, cx+14, cy-42, cx+24, cy-22, W,  4);
            // 3×3 date grid
            for (int r = 0; r < 3; r++) {
                for (int c = 0; c < 3; c++) {
                    int32_t gx = cx - 28 + c * 28;
                    int32_t gy = cy + 6  + r * 18;
                    _fill_ri(layer, gx-8, gy-6, gx+8, gy+6, W, 3);
                }
            }
            break;
        }

        case 4: // Lunch — fork (left) and knife (right)
            // Fork handle + 3 prongs + neck bar
            _fill_ri(layer, cx-25, cy-8,  cx-16, cy+46, W, 4);
            _fill_ri(layer, cx-28, cy-46, cx-24, cy-6,  W, 2);
            _fill_ri(layer, cx-22, cy-46, cx-18, cy-6,  W, 2);
            _fill_ri(layer, cx-16, cy-46, cx-12, cy-6,  W, 2);
            _fill_ri(layer, cx-30, cy-12, cx-10, cy-4,  W, 2);
            // Knife handle + blade
            _fill_ri(layer, cx+16, cy-8,  cx+25, cy+46, W, 4);
            _fill_ri(layer, cx+12, cy-46, cx+25, cy-6,  W, 4);
            break;

        case 5: // Walking the dog — paw print (large pad + 4 toe pads)
            _fill_ri(layer, cx-26, cy+4,  cx+26, cy+46, W, LV_RADIUS_CIRCLE);
            _fill_ri(layer, cx-40, cy-24, cx-16, cy+0,  W, LV_RADIUS_CIRCLE);
            _fill_ri(layer, cx-22, cy-38, cx+2,  cy-14, W, LV_RADIUS_CIRCLE);
            _fill_ri(layer, cx-2,  cy-38, cx+22, cy-14, W, LV_RADIUS_CIRCLE);
            _fill_ri(layer, cx+16, cy-24, cx+40, cy+0,  W, LV_RADIUS_CIRCLE);
            break;
    }
}

// Fires after the button's background/border draw; paints the icon on top.
// Children (text label) draw after this, so the label appears over the icon.
static void _icon_draw_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_DRAW_MAIN) return;
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_obj_t   *obj   = (lv_obj_t *)lv_event_get_target(e);
    int         idx   = (int)(intptr_t)lv_event_get_user_data(e);

    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    int32_t cx = (a.x1 + a.x2) / 2;
    int32_t cy = a.y1 + (a.y2 - a.y1) * 45 / 100;  // 45% down from top
    _draw_icon(layer, cx, cy, idx);
}

// ---- Build the LVGL UI (status bar + 3x2 button grid) ----
static void _createUI() {
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, CLR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // ---- Status bar ----
    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_set_size(bar, SCREEN_W, STATUS_BAR_H);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, CLR_STATUS_BAR, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar, 0, LV_PART_MAIN);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    // WiFi indicator dot (left side)
    _wifi_dot = lv_obj_create(bar);
    lv_obj_set_size(_wifi_dot, 14, 14);
    lv_obj_align(_wifi_dot, LV_ALIGN_LEFT_MID, 12, 0);
    lv_obj_set_style_bg_color(_wifi_dot, CLR_WIFI_BAD, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_wifi_dot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_wifi_dot, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(_wifi_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);

    // Current status text (centered in bar)
    _status_label = lv_label_create(bar);
    lv_obj_set_width(_status_label, SCREEN_W - 60);
    lv_obj_set_style_text_align(_status_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(_status_label, CLR_TEXT, LV_PART_MAIN);
    lv_obj_set_style_text_font(_status_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_label_set_text(_status_label, "---");
    lv_obj_align(_status_label, LV_ALIGN_CENTER, 0, 0);

    // ---- Button grid (3 cols x 2 rows) ----
    const int BTN_W = SCREEN_W / 3;                    // 266 px per column
    const int BTN_H = (SCREEN_H - STATUS_BAR_H) / 2;  // 216 px per row
    const int PAD   = 2;                               // gap between buttons

    for (int i = 0; i < NUM_BUTTONS; i++) {
        int col = i % 3;
        int row = i / 3;

        _btns[i] = lv_obj_create(scr);
        lv_obj_set_size(_btns[i], BTN_W - PAD * 2, BTN_H - PAD * 2);
        lv_obj_set_pos(_btns[i],
                       col * BTN_W + PAD,
                       STATUS_BAR_H + row * BTN_H + PAD);
        lv_obj_set_style_bg_color(_btns[i], CLR_BTN, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(_btns[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(_btns[i], 0, LV_PART_MAIN);
        lv_obj_set_style_radius(_btns[i], 8, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_btns[i], 8, LV_PART_MAIN);
        lv_obj_remove_flag(_btns[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(_btns[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(_btns[i], _btn_event_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *lbl = lv_label_create(_btns[i]);
        lv_label_set_text(lbl, BTN_LABELS[i]);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(lbl, lv_pct(100));
        lv_obj_set_style_text_color(lbl, CLR_TEXT, LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -12);

        lv_obj_add_event_cb(_btns[i], _icon_draw_cb, LV_EVENT_DRAW_MAIN,
                            (void*)(intptr_t)i);
    }
}

// ---- initDisplay() ----
void initDisplay() {
    Wire.begin(TOUCH_SDA, TOUCH_SCL);

    // Reset GT911 via CH422G and enable backlight.
    _initCH422G();

    _rgbpanel = new Arduino_ESP32RGBPanel(
        5  /* DE */,  3  /* VSYNC */, 46 /* HSYNC */, 7  /* PCLK */,
        1  /* R0 */,  2  /* R1 */,   42 /* R2 */,    41 /* R3 */,  40 /* R4 */,
        39 /* G0 */,  0  /* G1 */,   45 /* G2 */,    48 /* G3 */,  47 /* G4 */, 21 /* G5 */,
        14 /* B0 */,  38 /* B1 */,   18 /* B2 */,    17 /* B3 */,  10 /* B4 */,
        0  /* hsync_polarity */,
        40 /* hsync_front_porch */, 48 /* hsync_pulse_width */, 88 /* hsync_back_porch */,
        0  /* vsync_polarity */,
        13 /* vsync_front_porch */, 3  /* vsync_pulse_width */, 32 /* vsync_back_porch */,
        1  /* pclk_active_neg */, 16000000 /* prefer_speed */,
        false /* useBigEndian */, 0 /* de_idle_high */, 0 /* pclk_idle_high */,
        SCREEN_W * 10 /* bounce_buffer_size_px — DMA reads from SRAM copy, not PSRAM directly */
    );

    _gfx = new Arduino_RGB_Display(
        SCREEN_W, SCREEN_H, _rgbpanel,
        0    /* rotation */,
        true /* auto_flush */
    );

    _gfx->begin();
    _gfx->fillScreen(RGB565_BLACK);

    // LVGL v9 init
    lv_init();
    lv_tick_set_cb([]() -> uint32_t { return (uint32_t)millis(); });

    // Use the hardware framebuffer that Arduino_RGB_Display already allocated.
    // _gfx->getFramebuffer() (lowercase b) is a plain getter — safe to call,
    // does NOT recreate the panel (unlike _rgbpanel->getFrameBuffer() which does).
    // DIRECT mode: LVGL renders straight into this buffer; flush just writebacks cache.
    _buf1 = (lv_color_t *)_gfx->getFramebuffer();
    if (!_buf1) {
        ets_printf("FATAL: getFramebuffer() returned NULL\n");
        while (1) delay(1000);
    }

    lv_display_t *disp = lv_display_create(SCREEN_W, SCREEN_H);
    lv_display_set_flush_cb(disp, _display_flush);
    lv_display_set_buffers(disp, _buf1, NULL,
                           SCREEN_W * SCREEN_H * sizeof(lv_color_t),
                           LV_DISPLAY_RENDER_MODE_DIRECT);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, _touchpad_read);

    _createUI();
}

// ---- Public API ----
void setActiveButton(int idx) {
    _active_btn = idx;
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (i == idx) {
            lv_obj_set_style_bg_color(_btns[i], CLR_BTN_ACTIVE, LV_PART_MAIN);
            lv_obj_set_style_border_color(_btns[i], CLR_BORDER_ACTIVE, LV_PART_MAIN);
            lv_obj_set_style_border_width(_btns[i], 3, LV_PART_MAIN);
            lv_obj_set_style_border_opa(_btns[i], LV_OPA_COVER, LV_PART_MAIN);
        } else {
            lv_obj_set_style_bg_color(_btns[i], CLR_BTN, LV_PART_MAIN);
            lv_obj_set_style_border_width(_btns[i], 0, LV_PART_MAIN);
        }
    }
}

void setStatusText(const char* text) {
    lv_label_set_text(_status_label, text);
}

void setWiFiConnected(bool connected) {
    lv_obj_set_style_bg_color(_wifi_dot,
        connected ? CLR_WIFI_OK : CLR_WIFI_BAD, LV_PART_MAIN);
}
