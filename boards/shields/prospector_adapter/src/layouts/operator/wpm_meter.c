#include "wpm_meter.h"

#include <zephyr/kernel.h>
#include <ctype.h>
#include <zmk/display.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/wpm.h>
#include <zmk/keymap.h>

#include <fonts.h>
#include "display_colors.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
static struct k_work_delayable wpm_smooth_work;

static float displayed_wpm = 0.0f;
static float target_wpm = 0.0f;
static int prev_active_bars = 0;
static int peak_position = 0;
static int peak_hold_counter = 0;
static int peak_decay_counter = 0;
static const float smoothing_factor_up = 0.3f;
static const float smoothing_factor_down = 0.05f;

struct wpm_meter_state {
    uint8_t wpm;
};

struct layer_state {
    uint8_t index;
};

static void wpm_meter_render(int active_bars) {
    struct zmk_widget_wpm_meter *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        if (active_bars != prev_active_bars) {
            int min_bar = (active_bars < prev_active_bars) ? active_bars : prev_active_bars;
            int max_bar = (active_bars > prev_active_bars) ? active_bars : prev_active_bars;
            for (int i = min_bar; i < max_bar; i++) {
                lv_color_t color = (i < active_bars)
                    ? lv_color_hex(DISPLAY_COLOR_WPM_BAR_ACTIVE)
                    : lv_color_hex(DISPLAY_COLOR_WPM_BAR_INACTIVE);
                lv_obj_set_style_bg_color(widget->bars[i], color, LV_PART_MAIN);
            }
            prev_active_bars = active_bars;
        }

        if (peak_position > active_bars && peak_position > 0) {
            int bar_width = 8;
            int bar_gap = 2;
            int total_width = WPM_BAR_COUNT * bar_width + (WPM_BAR_COUNT - 1) * bar_gap;
            int start_x = (260 - total_width) / 2;
            int peak_slot = (peak_position > active_bars + 1) ? (peak_position - 1) : active_bars;
            if (peak_slot >= WPM_BAR_COUNT) peak_slot = WPM_BAR_COUNT - 1;
            int peak_x = start_x + peak_slot * (bar_width + bar_gap) + 2;
            lv_obj_set_pos(widget->peak_indicator, peak_x, 0);
            lv_obj_clear_flag(widget->peak_indicator, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(widget->peak_indicator, LV_OBJ_FLAG_HIDDEN);
        }

        char wpm_text[4];
        snprintf(wpm_text, sizeof(wpm_text), "%d", (int)(displayed_wpm + 0.5f));
        lv_label_set_text(widget->wpm_label, wpm_text);
    }
}

static void wpm_smooth_work_handler(struct k_work *work) {
    float diff = target_wpm - displayed_wpm;
    bool at_target = (diff > -0.5f && diff < 0.5f);

    int old_int = (int)(displayed_wpm + 0.5f);

    if (at_target) {
        displayed_wpm = target_wpm;
    } else {
        float factor = (diff > 0) ? smoothing_factor_up : smoothing_factor_down;
        displayed_wpm += diff * factor;
    }

    int new_int = (int)(displayed_wpm + 0.5f);
    int active_bars = (new_int * WPM_BAR_COUNT) / WPM_MAX;
    if (active_bars > WPM_BAR_COUNT) active_bars = WPM_BAR_COUNT;

    bool peak_changed = false;
    if (active_bars > peak_position) {
        peak_position = active_bars;
        peak_hold_counter = 0;
        peak_decay_counter = 0;
        peak_changed = true;
    } else if (peak_position > active_bars) {
        if (peak_hold_counter < 90) {
            peak_hold_counter++;
        } else {
            peak_decay_counter++;
            if (peak_decay_counter >= 18) {
                peak_position--;
                peak_decay_counter = 0;
                peak_changed = true;
            }
        }
    }

    if (old_int != new_int || peak_changed) {
        wpm_meter_render(active_bars);
    }

    if (!at_target || peak_position > active_bars) {
        k_work_schedule(&wpm_smooth_work, K_MSEC(33));
    }
}

static void wpm_meter_update_cb(struct wpm_meter_state state) {
    target_wpm = (float)state.wpm;
    k_work_schedule(&wpm_smooth_work, K_NO_WAIT);
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

#if IS_ENABLED(CONFIG_PROSPECTOR_LAYER_NAME_UPPERCASE)
        for (int i = 0; display_name[i]; i++) {
            display_name[i] = toupper((unsigned char)display_name[i]);
        }
#endif

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

    widget->peak_indicator = lv_obj_create(widget->obj);
    lv_obj_set_size(widget->peak_indicator, 4, bar_height);
    lv_obj_set_style_bg_color(widget->peak_indicator, lv_color_hex(0x505050), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(widget->peak_indicator, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(widget->peak_indicator, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(widget->peak_indicator, 1, LV_PART_MAIN);
    lv_obj_add_flag(widget->peak_indicator, LV_OBJ_FLAG_HIDDEN);

    widget->wpm_label = lv_label_create(widget->obj);
    lv_label_set_text(widget->wpm_label, "0");
    lv_obj_set_style_text_font(widget->wpm_label, &FR_Medium_32, LV_PART_MAIN);
    lv_obj_set_style_text_color(widget->wpm_label, lv_color_hex(DISPLAY_COLOR_WPM_TEXT), LV_PART_MAIN);
    lv_obj_set_style_bg_color(widget->wpm_label, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(widget->wpm_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(widget->wpm_label, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(widget->wpm_label, 4, LV_PART_MAIN);
    lv_obj_align(widget->wpm_label, LV_ALIGN_TOP_LEFT, -7, -9);

    widget->layer_label = lv_label_create(widget->obj);
    lv_label_set_text(widget->layer_label, "");
    lv_obj_set_style_text_font(widget->layer_label, &DINishExpanded_Light_36, LV_PART_MAIN);
    lv_obj_set_style_text_color(widget->layer_label, lv_color_hex(DISPLAY_COLOR_LAYER_TEXT), LV_PART_MAIN);
    lv_obj_set_style_bg_color(widget->layer_label, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(widget->layer_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(widget->layer_label, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_top(widget->layer_label, 7, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(widget->layer_label, 3, LV_PART_MAIN);
    lv_obj_align(widget->layer_label, LV_ALIGN_BOTTOM_RIGHT, 9, 7);

    sys_slist_append(&widgets, &widget->node);
    widget_wpm_meter_init();
    widget_wpm_meter_layer_init();

    k_work_init_delayable(&wpm_smooth_work, wpm_smooth_work_handler);

    return 0;
}

lv_obj_t *zmk_widget_wpm_meter_obj(struct zmk_widget_wpm_meter *widget) {
    return widget->obj;
}
