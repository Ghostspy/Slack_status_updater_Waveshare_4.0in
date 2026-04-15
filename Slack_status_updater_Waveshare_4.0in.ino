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
