#include "battery_circles.h"

#include <zmk/display.h>
#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/split_central_status_changed.h>
#include <zmk/event_manager.h>

#include <fonts.h>
#include "display_colors.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

#define LOW_BATTERY_THRESHOLD 20
#define ARC_WIDTH_CONNECTED 5
#define ARC_WIDTH_DISCONNECTED 2
#define ARC_WIDTH_ANIM_DURATION 200
#define ARC_VALUE_ANIM_DURATION 300

static uint8_t peripheral_battery[ZMK_SPLIT_BLE_PERIPHERAL_COUNT] = {0};
static bool peripheral_connected[ZMK_SPLIT_BLE_PERIPHERAL_COUNT] = {false};

static lv_style_t style_arc_ring_disconnected;
static lv_style_t style_arc_ring_connected;
static lv_style_t style_arc_ring_low;
static lv_style_t style_arc_ind_disconnected;
static lv_style_t style_arc_ind_connected;
static lv_style_t style_arc_ind_low;
static lv_style_t style_label_box_disconnected;
static lv_style_t style_label_box_connected;
static lv_style_t style_label_box_low;
static lv_style_t style_label_disconnected;
static lv_style_t style_label_connected;
static bool styles_initialized = false;

static void init_styles(void) {
    if (styles_initialized) {
        return;
    }

    lv_style_init(&style_arc_ring_disconnected);
    lv_style_set_arc_color(&style_arc_ring_disconnected, lv_color_hex(DISPLAY_COLOR_BATTERY_DISCONNECTED_RING));

    lv_style_init(&style_arc_ring_connected);
    lv_style_set_arc_color(&style_arc_ring_connected, lv_color_hex(DISPLAY_COLOR_BATTERY_RING));

    lv_style_init(&style_arc_ring_low);
    lv_style_set_arc_color(&style_arc_ring_low, lv_color_hex(DISPLAY_COLOR_BATTERY_LOW_RING));

    lv_style_init(&style_arc_ind_disconnected);
    lv_style_set_arc_color(&style_arc_ind_disconnected, lv_color_hex(DISPLAY_COLOR_BATTERY_DISCONNECTED_FILL));

    lv_style_init(&style_arc_ind_connected);
    lv_style_set_arc_color(&style_arc_ind_connected, lv_color_hex(DISPLAY_COLOR_BATTERY_FILL));

    lv_style_init(&style_arc_ind_low);
    lv_style_set_arc_color(&style_arc_ind_low, lv_color_hex(DISPLAY_COLOR_BATTERY_LOW_FILL));

    lv_style_init(&style_label_box_disconnected);
    lv_style_set_bg_color(&style_label_box_disconnected, lv_color_hex(DISPLAY_COLOR_BATTERY_DISCONNECTED_FILL));

    lv_style_init(&style_label_box_connected);
    lv_style_set_bg_color(&style_label_box_connected, lv_color_hex(DISPLAY_COLOR_BATTERY_FILL));

    lv_style_init(&style_label_box_low);
    lv_style_set_bg_color(&style_label_box_low, lv_color_hex(DISPLAY_COLOR_BATTERY_LOW_FILL));

    lv_style_init(&style_label_disconnected);
    lv_style_set_text_color(&style_label_disconnected, lv_color_hex(DISPLAY_COLOR_BATTERY_DISCONNECTED_LABEL));

    lv_style_init(&style_label_connected);
    lv_style_set_text_color(&style_label_connected, lv_color_hex(0x000000));

    styles_initialized = true;
}

static float cubic_bezier_y(float t, float y1, float y2) {
    float mt = 1.0f - t;
    return 3.0f * mt * mt * t * y1 + 3.0f * mt * t * t * y2 + t * t * t;
}

static float cubic_bezier_x(float t, float x1, float x2) {
    float mt = 1.0f - t;
    return 3.0f * mt * mt * t * x1 + 3.0f * mt * t * t * x2 + t * t * t;
}

static float cubic_bezier_solve(float x, float x1, float x2) {
    float t = x;
    for (int i = 0; i < 8; i++) {
        float x_est = cubic_bezier_x(t, x1, x2);
        float dx = x - x_est;
        if (dx > -0.001f && dx < 0.001f) break;
        float mt = 1.0f - t;
        float slope = 3.0f * mt * mt * x1 + 6.0f * mt * t * (x2 - x1) + 3.0f * t * t * (1.0f - x2);
        if (slope < 0.001f && slope > -0.001f) break;
        t += dx / slope;
    }
    return t;
}

static int32_t lv_anim_path_bezier_battery(const lv_anim_t *a) {
    float x = (float)a->act_time / (float)a->duration;
    float t = cubic_bezier_solve(x, 0.00f, 0.30f);
    float y = cubic_bezier_y(t, 0.70f, 1.00f);
    int32_t diff = a->end_value - a->start_value;
    return a->start_value + (int32_t)(diff * y);
}

static void arc_width_anim_cb(void *var, int32_t value) {
    lv_obj_t *arc = (lv_obj_t *)var;
    lv_obj_set_style_arc_width(arc, value, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, value, LV_PART_INDICATOR);
}

static void animate_arc_width(lv_obj_t *arc, int32_t target_width) {
    int32_t current_width = lv_obj_get_style_arc_width(arc, LV_PART_MAIN);
    if (current_width == target_width) {
        return;
    }

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, arc);
    lv_anim_set_values(&anim, current_width, target_width);
    lv_anim_set_time(&anim, ARC_WIDTH_ANIM_DURATION);
    lv_anim_set_exec_cb(&anim, arc_width_anim_cb);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_start(&anim);
}

static void arc_value_anim_cb(void *var, int32_t value) {
    lv_obj_t *arc = (lv_obj_t *)var;
    lv_arc_set_value(arc, value);
}

static void animate_arc_value(lv_obj_t *arc, int32_t target_value) {
    int32_t current_value = lv_arc_get_value(arc);
    if (current_value == target_value) {
        return;
    }

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, arc);
    lv_anim_set_values(&anim, current_value, target_value);
    lv_anim_set_time(&anim, ARC_VALUE_ANIM_DURATION);
    lv_anim_set_exec_cb(&anim, arc_value_anim_cb);
    lv_anim_set_path_cb(&anim, lv_anim_path_bezier_battery);
    lv_anim_start(&anim);
}

struct battery_update_state {
    uint8_t source;
    uint8_t level;
};

struct connection_update_state {
    uint8_t source;
    bool connected;
};

static void update_battery_circle_colors(lv_obj_t *widget_obj, uint8_t source) {
    if (source >= ZMK_SPLIT_BLE_PERIPHERAL_COUNT) {
        return;
    }

    lv_obj_t *arc = lv_obj_get_child(widget_obj, source);
    if (!arc) {
        return;
    }

    lv_obj_t *label_box = lv_obj_get_child(arc, 0);
    lv_obj_t *label = label_box ? lv_obj_get_child(label_box, 0) : NULL;

    bool connected = peripheral_connected[source];
    uint8_t level = peripheral_battery[source];
    bool low_battery = connected && level > 0 && level <= LOW_BATTERY_THRESHOLD;

    lv_obj_remove_style(arc, &style_arc_ring_disconnected, LV_PART_MAIN);
    lv_obj_remove_style(arc, &style_arc_ring_connected, LV_PART_MAIN);
    lv_obj_remove_style(arc, &style_arc_ring_low, LV_PART_MAIN);
    lv_obj_remove_style(arc, &style_arc_ind_disconnected, LV_PART_INDICATOR);
    lv_obj_remove_style(arc, &style_arc_ind_connected, LV_PART_INDICATOR);
    lv_obj_remove_style(arc, &style_arc_ind_low, LV_PART_INDICATOR);
    if (label_box) {
        lv_obj_remove_style(label_box, &style_label_box_disconnected, LV_PART_MAIN);
        lv_obj_remove_style(label_box, &style_label_box_connected, LV_PART_MAIN);
        lv_obj_remove_style(label_box, &style_label_box_low, LV_PART_MAIN);
    }
    if (label) {
        lv_obj_remove_style(label, &style_label_disconnected, LV_PART_MAIN);
        lv_obj_remove_style(label, &style_label_connected, LV_PART_MAIN);
    }

    if (label) {
        char label_text[4];
        if (connected && level > 0) {
            snprintf(label_text, sizeof(label_text), "%d", level);
        } else {
            snprintf(label_text, sizeof(label_text), "-");
        }
        lv_label_set_text(label, label_text);
    }

    if (!connected) {
        animate_arc_width(arc, ARC_WIDTH_DISCONNECTED);
        lv_obj_add_style(arc, &style_arc_ring_disconnected, LV_PART_MAIN);
        lv_obj_add_style(arc, &style_arc_ind_disconnected, LV_PART_INDICATOR);
        if (label_box) {
            lv_obj_add_style(label_box, &style_label_box_disconnected, LV_PART_MAIN);
        }
        if (label) {
            lv_obj_add_style(label, &style_label_disconnected, LV_PART_MAIN);
        }
    } else if (low_battery) {
        animate_arc_width(arc, ARC_WIDTH_CONNECTED);
        lv_obj_add_style(arc, &style_arc_ring_low, LV_PART_MAIN);
        lv_obj_add_style(arc, &style_arc_ind_low, LV_PART_INDICATOR);
        if (label_box) {
            lv_obj_add_style(label_box, &style_label_box_low, LV_PART_MAIN);
        }
        if (label) {
            lv_obj_add_style(label, &style_label_connected, LV_PART_MAIN);
        }
    } else {
        animate_arc_width(arc, ARC_WIDTH_CONNECTED);
        lv_obj_add_style(arc, &style_arc_ring_connected, LV_PART_MAIN);
        lv_obj_add_style(arc, &style_arc_ind_connected, LV_PART_INDICATOR);
        if (label_box) {
            lv_obj_add_style(label_box, &style_label_box_connected, LV_PART_MAIN);
        }
        if (label) {
            lv_obj_add_style(label, &style_label_connected, LV_PART_MAIN);
        }
    }
}

static void set_battery_circle_value(lv_obj_t *widget_obj, struct battery_update_state state, bool is_initialized) {
    if (!is_initialized || state.source >= ZMK_SPLIT_BLE_PERIPHERAL_COUNT) {
        return;
    }

    lv_obj_t *arc = lv_obj_get_child(widget_obj, state.source);
    if (!arc) {
        return;
    }

    peripheral_battery[state.source] = state.level;
    animate_arc_value(arc, state.level);
    update_battery_circle_colors(widget_obj, state.source);
}

static void set_battery_circle_connected(lv_obj_t *widget_obj, struct connection_update_state state, bool is_initialized) {
    if (!is_initialized || state.source >= ZMK_SPLIT_BLE_PERIPHERAL_COUNT) {
        return;
    }

    peripheral_connected[state.source] = state.connected;
    update_battery_circle_colors(widget_obj, state.source);
}

void battery_circles_battery_update_cb(struct battery_update_state state) {
    struct zmk_widget_battery_circles *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_battery_circle_value(widget->obj, state, widget->initialized);
    }
}

static struct battery_update_state battery_circles_get_battery_state(const zmk_event_t *eh) {
    if (eh == NULL) {
        return (struct battery_update_state){.source = 0, .level = 0};
    }

    const struct zmk_peripheral_battery_state_changed *bat_ev =
        as_zmk_peripheral_battery_state_changed(eh);
    if (bat_ev == NULL) {
        return (struct battery_update_state){.source = 0, .level = 0};
    }

    return (struct battery_update_state){
        .source = bat_ev->source,
        .level = bat_ev->state_of_charge,
    };
}

void battery_circles_connection_update_cb(struct connection_update_state state) {
    struct zmk_widget_battery_circles *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_battery_circle_connected(widget->obj, state, widget->initialized);
    }
}

static struct connection_update_state battery_circles_get_connection_state(const zmk_event_t *eh) {
    if (eh == NULL) {
        return (struct connection_update_state){.source = 0, .connected = false};
    }

    const struct zmk_split_central_status_changed *conn_ev =
        as_zmk_split_central_status_changed(eh);
    if (conn_ev == NULL) {
        return (struct connection_update_state){.source = 0, .connected = false};
    }

    return (struct connection_update_state){
        .source = conn_ev->slot,
        .connected = conn_ev->connected,
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_circles_battery, struct battery_update_state,
                            battery_circles_battery_update_cb, battery_circles_get_battery_state);
ZMK_SUBSCRIPTION(widget_battery_circles_battery, zmk_peripheral_battery_state_changed);

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_circles_connection, struct connection_update_state,
                            battery_circles_connection_update_cb, battery_circles_get_connection_state);
ZMK_SUBSCRIPTION(widget_battery_circles_connection, zmk_split_central_status_changed);

int zmk_widget_battery_circles_init(struct zmk_widget_battery_circles *widget, lv_obj_t *parent) {
    init_styles();

    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 132, 62);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(widget->obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(widget->obj, 0, LV_PART_MAIN);

    int arc_size = 58;
    int spacing = 66;
    int y_center = (62 - arc_size) / 2;

    for (int i = 0; i < ZMK_SPLIT_BLE_PERIPHERAL_COUNT; i++) {
        lv_obj_t *arc = lv_arc_create(widget->obj);

        lv_obj_set_size(arc, arc_size, arc_size);
        lv_obj_set_pos(arc, i * spacing, y_center);

        lv_arc_set_range(arc, 0, 100);
        lv_arc_set_value(arc, 0);
        lv_arc_set_bg_angles(arc, 270, 180);
        lv_arc_set_rotation(arc, 0);

        lv_obj_set_style_arc_width(arc, ARC_WIDTH_DISCONNECTED, LV_PART_MAIN);
        lv_obj_set_style_arc_width(arc, ARC_WIDTH_DISCONNECTED, LV_PART_INDICATOR);
        lv_obj_add_style(arc, &style_arc_ring_disconnected, LV_PART_MAIN);
        lv_obj_add_style(arc, &style_arc_ind_disconnected, LV_PART_INDICATOR);

        lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
        lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *label_box = lv_obj_create(arc);
        lv_obj_set_size(label_box, 25, 25);
        lv_obj_set_pos(label_box, 0, 0);
        lv_obj_set_style_bg_opa(label_box, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(label_box, 2, LV_PART_MAIN);
        lv_obj_set_style_border_width(label_box, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(label_box, 0, LV_PART_MAIN);
        lv_obj_add_style(label_box, &style_label_box_disconnected, LV_PART_MAIN);

        lv_obj_t *label = lv_label_create(label_box);
        lv_label_set_text(label, "-");
        lv_obj_set_style_text_font(label, &DINish_Medium_24, LV_PART_MAIN);
        lv_obj_set_style_text_letter_space(label, -1, LV_PART_MAIN);
        lv_obj_add_style(label, &style_label_disconnected, LV_PART_MAIN);
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    }

    widget_battery_circles_battery_init();
    widget_battery_circles_connection_init();

    widget->initialized = true;
    sys_slist_append(&widgets, &widget->node);

    return 0;
}

lv_obj_t *zmk_widget_battery_circles_obj(struct zmk_widget_battery_circles *widget) {
    return widget->obj;
}
