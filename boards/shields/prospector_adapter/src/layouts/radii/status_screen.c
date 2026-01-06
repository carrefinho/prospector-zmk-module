#include <lvgl.h>

#include "layer_indicator.h"
#include "battery_circles.h"
#include "modifier_indicator.h"
#include "output.h"
#include "display_colors.h"

static struct zmk_widget_layer_indicator layer_indicator_widget;
static struct zmk_widget_battery_circles battery_circles_widget;
static struct zmk_widget_modifier_indicator modifier_indicator_widget;
static struct zmk_widget_output output_widget;

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, 255, LV_PART_MAIN);

    lv_obj_t *left_panel = lv_obj_create(screen);
    lv_obj_set_size(left_panel, 172, 240);
    lv_obj_set_pos(left_panel, 0, 0);
    lv_obj_set_style_bg_color(left_panel, lv_color_hex(DISPLAY_COLOR_LEFT_PANEL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(left_panel, 255, LV_PART_MAIN);
    lv_obj_set_style_radius(left_panel, 24, LV_PART_MAIN);
    lv_obj_set_style_border_width(left_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(left_panel, 0, LV_PART_MAIN);

    zmk_widget_layer_indicator_init(&layer_indicator_widget, left_panel);
    lv_obj_set_pos(zmk_widget_layer_indicator_obj(&layer_indicator_widget), 14, 20);

    zmk_widget_output_init(&output_widget, screen);
    lv_obj_align(zmk_widget_output_obj(&output_widget), LV_ALIGN_TOP_RIGHT, 0, 0);

    zmk_widget_modifier_indicator_init(&modifier_indicator_widget, screen);
    lv_obj_align(zmk_widget_modifier_indicator_obj(&modifier_indicator_widget), LV_ALIGN_BOTTOM_RIGHT, 0, -62);

    zmk_widget_battery_circles_init(&battery_circles_widget, screen);
    lv_obj_set_pos(zmk_widget_battery_circles_obj(&battery_circles_widget), 172, 178);

    return screen;
}
