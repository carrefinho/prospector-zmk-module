#include <lvgl.h>

#include "layer_roller.h"
#include "battery_bar.h"
#include "modifier_indicator.h"
// #include "output.h"

#include <fonts.h>

static struct zmk_widget_layer_roller layer_roller_widget;
static struct zmk_widget_battery_bar battery_bar_widget;
static struct zmk_widget_modifier_indicator modifier_indicator_widget;
// static struct zmk_widget_output output_widget;

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, 255, LV_PART_MAIN);

    // zmk_widget_output_init(&output_widget, screen);
    // lv_obj_align(zmk_widget_output_obj(&output_widget), LV_ALIGN_TOP_RIGHT, -8, 8);

    zmk_widget_modifier_indicator_init(&modifier_indicator_widget, screen);
    lv_obj_align(zmk_widget_modifier_indicator_obj(&modifier_indicator_widget), LV_ALIGN_RIGHT_MID, -8, -12);

    zmk_widget_battery_bar_init(&battery_bar_widget, screen);
    lv_obj_set_size(zmk_widget_battery_bar_obj(&battery_bar_widget), lv_pct(100), 48);
    lv_obj_align(zmk_widget_battery_bar_obj(&battery_bar_widget), LV_ALIGN_BOTTOM_MID, 0, 0);

    zmk_widget_layer_roller_init(&layer_roller_widget, screen);
    lv_obj_set_size(zmk_widget_layer_roller_obj(&layer_roller_widget), 224, 140);
    lv_obj_align(zmk_widget_layer_roller_obj(&layer_roller_widget), LV_ALIGN_LEFT_MID, 0, -20);

    return screen;
}
