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

#ifndef PERIPHERAL_COUNT
#define PERIPHERAL_COUNT ZMK_SPLIT_BLE_PERIPHERAL_COUNT
#endif

static lv_obj_t *peripheral_arcs[PERIPHERAL_COUNT];
static lv_obj_t *peripheral_label;

struct battery_update_state {
    uint8_t source;
    uint8_t level;
};

struct connection_update_state {
    uint8_t source;
    bool connected;
};

static void update_peripheral_display(uint8_t source, uint8_t level, bool connected) {
    int max_displayed = PERIPHERAL_COUNT > 3 ? 3 : PERIPHERAL_COUNT;
    if (source >= max_displayed) {
        return;
    }

    lv_obj_t *arc = peripheral_arcs[source];
    if (!arc) {
        return;
    }

    lv_arc_set_value(arc, connected ? level : 0);
    lv_obj_set_style_arc_color(arc,
        lv_color_hex(connected ? DISPLAY_COLOR_ARC_INDICATOR : DISPLAY_COLOR_ARC_BG),
        LV_PART_INDICATOR);

    if (PERIPHERAL_COUNT == 1 && peripheral_label) {
        if (connected && level > 0) {
            lv_label_set_text_fmt(peripheral_label, "%d", level);
        } else {
            lv_label_set_text(peripheral_label, "");
        }
    }
}

static uint8_t peripheral_battery[PERIPHERAL_COUNT];
static bool peripheral_connected[PERIPHERAL_COUNT];

static void set_battery_level(uint8_t source, uint8_t level) {
    int max_displayed = PERIPHERAL_COUNT > 3 ? 3 : PERIPHERAL_COUNT;
    if (source >= max_displayed) {
        return;
    }
    peripheral_battery[source] = level;
    update_peripheral_display(source, level, peripheral_connected[source]);
}

static void set_connection_status(uint8_t source, bool connected) {
    int max_displayed = PERIPHERAL_COUNT > 3 ? 3 : PERIPHERAL_COUNT;
    if (source >= max_displayed) {
        return;
    }
    peripheral_connected[source] = connected;
    update_peripheral_display(source, peripheral_battery[source], connected);
}

void battery_circles_battery_update_cb(struct battery_update_state state) {
    struct zmk_widget_battery_circles *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        if (widget->initialized) {
            set_battery_level(state.source, state.level);
        }
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
        if (widget->initialized) {
            set_connection_status(state.source, state.connected);
        }
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

static lv_obj_t *create_arc(lv_obj_t *parent, int size, int x, int y, int width) {
    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_set_size(arc, size, size);
    lv_obj_set_pos(arc, x, y);

    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, 0);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_rotation(arc, 270);

    lv_obj_set_style_arc_width(arc, width, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(DISPLAY_COLOR_ARC_BG), LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);

    lv_obj_set_style_arc_width(arc, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(DISPLAY_COLOR_ARC_BG), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);

    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);

    return arc;
}

int zmk_widget_battery_circles_init(struct zmk_widget_battery_circles *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 108, 62);
    lv_obj_set_style_bg_color(widget->obj, lv_color_hex(DISPLAY_COLOR_BATTERY_PANEL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(widget->obj, 255, LV_PART_MAIN);
    lv_obj_set_style_radius(widget->obj, 24, LV_PART_MAIN);
    lv_obj_set_style_border_width(widget->obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(widget->obj, 0, LV_PART_MAIN);

    if (PERIPHERAL_COUNT == 1) {
        int arc_size = 32;
        peripheral_arcs[0] = create_arc(widget->obj, arc_size, 17, (62 - arc_size) / 2, 6);

        peripheral_label = lv_label_create(widget->obj);
        lv_label_set_text(peripheral_label, "");
        lv_obj_set_style_text_font(peripheral_label, &Symbols_Medium_28, LV_PART_MAIN);
        lv_obj_set_style_text_color(peripheral_label, lv_color_hex(DISPLAY_COLOR_ARC_INDICATOR), LV_PART_MAIN);
        lv_obj_set_style_text_align(peripheral_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
        lv_obj_align(peripheral_label, LV_ALIGN_RIGHT_MID, -19, -1);

    } else if (PERIPHERAL_COUNT == 2) {
        int arc_size = 30;
        int left_pad = 19;
        int arc_gap = 10;
        int y_center = (62 - arc_size) / 2;

        for (int i = 0; i < 2; i++) {
            peripheral_arcs[i] = create_arc(widget->obj, arc_size, left_pad + i * (arc_size + arc_gap), y_center, 6);
        }

    } else {
        int arc_size = 24;
        int left_pad = 12;
        int arc_gap = 5;
        int y_center = (62 - arc_size) / 2;
        int max_displayed = PERIPHERAL_COUNT > 3 ? 3 : PERIPHERAL_COUNT;

        for (int i = 0; i < max_displayed; i++) {
            peripheral_arcs[i] = create_arc(widget->obj, arc_size, left_pad + i * (arc_size + arc_gap), y_center, 4);
        }
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
