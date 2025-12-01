#include "battery_circles.h"

#include <zmk/display.h>
#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/split_central_status_changed.h>
#include <zmk/event_manager.h>

#include <fonts.h>
#include "display_colors.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct battery_update_state {
    uint8_t source;
    uint8_t level;
};

struct connection_update_state {
    uint8_t source;
    bool connected;
};

static void set_battery_circle_value(lv_obj_t *widget_obj, struct battery_update_state state, bool is_initialized) {
    if (!is_initialized || state.source >= ZMK_SPLIT_BLE_PERIPHERAL_COUNT) {
        return;
    }

    lv_obj_t *arc = lv_obj_get_child(widget_obj, state.source);
    if (!arc) {
        return;
    }

    lv_arc_set_value(arc, state.level);
}

static void set_battery_circle_connected(lv_obj_t *widget_obj, struct connection_update_state state, bool is_initialized) {
    if (!is_initialized || state.source >= ZMK_SPLIT_BLE_PERIPHERAL_COUNT) {
        return;
    }

    lv_obj_t *arc = lv_obj_get_child(widget_obj, state.source);
    if (!arc) {
        return;
    }

    if (state.connected) {
        lv_obj_set_style_arc_color(arc, lv_color_hex(DISPLAY_COLOR_ARC_INDICATOR), LV_PART_INDICATOR);
    } else {
        lv_obj_set_style_arc_color(arc, lv_color_hex(DISPLAY_COLOR_ARC_BG), LV_PART_INDICATOR);
    }
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
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 108, 62);
    lv_obj_set_style_bg_color(widget->obj, lv_color_hex(DISPLAY_COLOR_BATTERY_PANEL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(widget->obj, 255, LV_PART_MAIN);
    lv_obj_set_style_radius(widget->obj, 24, LV_PART_MAIN);
    lv_obj_set_style_border_width(widget->obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(widget->obj, 0, LV_PART_MAIN);

    // Even distribution: equal spacing on left, middle, and right
    // Container: 108px, 2 arcs of 28px diameter
    // Remaining space: 108 - 56 = 52px, divided into 3 gaps = ~17px each
    int gap = (108 - (2 * 28)) / 3;
    int y_center = (62 - 28) / 2;

    for (int i = 0; i < ZMK_SPLIT_BLE_PERIPHERAL_COUNT; i++) {
        lv_obj_t *arc = lv_arc_create(widget->obj);

        lv_obj_set_size(arc, 28, 28);
        lv_obj_set_pos(arc, gap + i * (28 + gap), y_center);

        lv_arc_set_range(arc, 0, 100);
        lv_arc_set_value(arc, 0);
        lv_arc_set_bg_angles(arc, 0, 360);
        lv_arc_set_rotation(arc, 270);

        lv_obj_set_style_arc_width(arc, 5, LV_PART_MAIN);
        lv_obj_set_style_arc_color(arc, lv_color_hex(DISPLAY_COLOR_ARC_BG), LV_PART_MAIN);

        lv_obj_set_style_arc_width(arc, 5, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(arc, lv_color_hex(DISPLAY_COLOR_ARC_INDICATOR), LV_PART_INDICATOR);

        lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
        lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
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
