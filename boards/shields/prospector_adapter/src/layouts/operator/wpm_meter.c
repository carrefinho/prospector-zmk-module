#include "wpm_meter.h"

#include <zmk/display.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/wpm.h>
#include <zmk/keymap.h>

#include <fonts.h>
#include "display_colors.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct wpm_meter_state {
    uint8_t wpm;
};

struct layer_state {
    uint8_t index;
};

static void wpm_meter_update_cb(struct wpm_meter_state state) {
    struct zmk_widget_wpm_meter *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        int active_bars = (state.wpm * WPM_BAR_COUNT) / WPM_MAX;
        if (active_bars > WPM_BAR_COUNT) {
            active_bars = WPM_BAR_COUNT;
        }

        for (int i = 0; i < WPM_BAR_COUNT; i++) {
            lv_color_t color = (i < active_bars)
                ? lv_color_hex(DISPLAY_COLOR_WPM_BAR_ACTIVE)
                : lv_color_hex(DISPLAY_COLOR_WPM_BAR_INACTIVE);
            lv_obj_set_style_bg_color(widget->bars[i], color, LV_PART_MAIN);
        }

        char wpm_text[4];
        snprintf(wpm_text, sizeof(wpm_text), "%d", state.wpm);
        lv_label_set_text(widget->wpm_label, wpm_text);
    }
}

static struct wpm_meter_state wpm_meter_get_state(const zmk_event_t *eh) {
    return (struct wpm_meter_state){.wpm = zmk_wpm_get_state()};
}

static void layer_update_cb(struct layer_state state) {
    struct zmk_widget_wpm_meter *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        const char *layer_name = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(state.index));
        char display_name[32];

        if (layer_name && *layer_name) {
            snprintf(display_name, sizeof(display_name), "%s", layer_name);
        } else {
            snprintf(display_name, sizeof(display_name), "Layer %d", state.index);
        }
        lv_label_set_text(widget->layer_label, display_name);
    }
}

static struct layer_state layer_get_state(const zmk_event_t *eh) {
    return (struct layer_state){.index = zmk_keymap_highest_layer_active()};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_wpm_meter, struct wpm_meter_state,
                            wpm_meter_update_cb, wpm_meter_get_state)
ZMK_SUBSCRIPTION(widget_wpm_meter, zmk_wpm_state_changed);

ZMK_DISPLAY_WIDGET_LISTENER(widget_wpm_meter_layer, struct layer_state,
                            layer_update_cb, layer_get_state)
ZMK_SUBSCRIPTION(widget_wpm_meter_layer, zmk_layer_state_changed);

int zmk_widget_wpm_meter_init(struct zmk_widget_wpm_meter *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 260, 90);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(widget->obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(widget->obj, 0, LV_PART_MAIN);

    int bar_width = 8;
    int bar_gap = 2;
    int bar_height = 90;
    int total_width = WPM_BAR_COUNT * bar_width + (WPM_BAR_COUNT - 1) * bar_gap;
    int start_x = (260 - total_width) / 2;

    for (int i = 0; i < WPM_BAR_COUNT; i++) {
        widget->bars[i] = lv_obj_create(widget->obj);
        lv_obj_set_size(widget->bars[i], bar_width, bar_height);
        lv_obj_set_pos(widget->bars[i], start_x + i * (bar_width + bar_gap), 0);
        lv_obj_set_style_bg_color(widget->bars[i], lv_color_hex(DISPLAY_COLOR_WPM_BAR_INACTIVE), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(widget->bars[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(widget->bars[i], 0, LV_PART_MAIN);
        lv_obj_set_style_radius(widget->bars[i], 1, LV_PART_MAIN);
        lv_obj_set_style_pad_all(widget->bars[i], 0, LV_PART_MAIN);
    }

    widget->wpm_label = lv_label_create(widget->obj);
    lv_label_set_text(widget->wpm_label, "0");
    lv_obj_set_style_text_font(widget->wpm_label, &FR_Regular_36, LV_PART_MAIN);
    lv_obj_set_style_text_color(widget->wpm_label, lv_color_hex(DISPLAY_COLOR_WPM_TEXT), LV_PART_MAIN);
    lv_obj_set_style_bg_color(widget->wpm_label, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(widget->wpm_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(widget->wpm_label, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(widget->wpm_label, 6, LV_PART_MAIN);
    lv_obj_align(widget->wpm_label, LV_ALIGN_TOP_LEFT, -8, -6);

    widget->layer_label = lv_label_create(widget->obj);
    lv_label_set_text(widget->layer_label, "");
    lv_obj_set_style_text_font(widget->layer_label, &FR_Regular_36, LV_PART_MAIN);
    lv_obj_set_style_text_color(widget->layer_label, lv_color_hex(DISPLAY_COLOR_LAYER_TEXT), LV_PART_MAIN);
    lv_obj_set_style_bg_color(widget->layer_label, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(widget->layer_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(widget->layer_label, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(widget->layer_label, 6, LV_PART_MAIN);
    lv_obj_align(widget->layer_label, LV_ALIGN_BOTTOM_RIGHT, 8, 6);

    sys_slist_append(&widgets, &widget->node);
    widget_wpm_meter_init();
    widget_wpm_meter_layer_init();

    return 0;
}

lv_obj_t *zmk_widget_wpm_meter_obj(struct zmk_widget_wpm_meter *widget) {
    return widget->obj;
}
