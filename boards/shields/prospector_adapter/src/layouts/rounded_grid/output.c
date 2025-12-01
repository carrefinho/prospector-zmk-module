#include "output.h"
#include "modifier_indicator.h"

#include <zmk/display.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/event_manager.h>
#include <zmk/endpoints.h>
#include <zmk/ble.h>

#include <fonts.h>
#include <symbols.h>

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

#define PROFILE_DISPLAY_TIMEOUT K_SECONDS(3)

static void profile_display_timeout_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(profile_display_timeout_work, profile_display_timeout_handler);

static uint8_t active_profile_index = 0;
static enum zmk_transport active_transport = ZMK_TRANSPORT_USB;

static void profile_display_timeout_handler(struct k_work *work) {
    if (active_transport != ZMK_TRANSPORT_BLE) {
        zmk_widget_modifier_indicator_set_compact(false);
    }
}

static void fade_out_cb(lv_anim_t *anim) {
    lv_obj_t *obj = anim->var;
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static void update_profile_indicator(struct zmk_widget_output *widget, uint8_t profile_index) {
    char profile_text[4];
    snprintf(profile_text, sizeof(profile_text), "%d", profile_index);
    lv_label_set_text(widget->profile_label, profile_text);

    bool is_connected = zmk_ble_profile_is_connected(profile_index);
    bool is_open = zmk_ble_profile_is_open(profile_index);
    bool is_sending = (active_transport == ZMK_TRANSPORT_BLE);

    bool circle_was_visible = !lv_obj_has_flag(widget->profile_circle, LV_OBJ_FLAG_HIDDEN);
    bool spinner_was_visible = !lv_obj_has_flag(widget->profile_spinner, LV_OBJ_FLAG_HIDDEN);

    if (is_connected) {
        if (spinner_was_visible) {
            lv_anim_t anim_out;
            lv_anim_init(&anim_out);
            lv_anim_set_var(&anim_out, widget->profile_spinner);
            lv_anim_set_values(&anim_out, 255, 0);
            lv_anim_set_time(&anim_out, 150);
            lv_anim_set_exec_cb(&anim_out, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
            lv_anim_set_path_cb(&anim_out, lv_anim_path_ease_in);
            lv_anim_set_ready_cb(&anim_out, fade_out_cb);
            lv_anim_start(&anim_out);
        } else {
            lv_obj_add_flag(widget->profile_spinner, LV_OBJ_FLAG_HIDDEN);
        }

        lv_obj_set_style_arc_color(widget->profile_circle, lv_color_hex(0xffffff), LV_PART_MAIN);
        lv_obj_set_style_arc_color(widget->profile_circle, lv_color_hex(0xffffff), LV_PART_INDICATOR);
        if (is_sending) {
            lv_obj_set_style_bg_color(widget->profile_circle, lv_color_hex(0xffffff), LV_PART_MAIN);
        } else {
            lv_obj_set_style_bg_color(widget->profile_circle, lv_color_hex(0x000000), LV_PART_MAIN);
        }

        lv_obj_clear_flag(widget->profile_circle, LV_OBJ_FLAG_HIDDEN);

        if (spinner_was_visible) {
            lv_obj_set_style_opa(widget->profile_circle, 0, 0);
            lv_anim_t anim_in;
            lv_anim_init(&anim_in);
            lv_anim_set_var(&anim_in, widget->profile_circle);
            lv_anim_set_values(&anim_in, 0, 255);
            lv_anim_set_time(&anim_in, 150);
            lv_anim_set_delay(&anim_in, 150);
            lv_anim_set_exec_cb(&anim_in, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
            lv_anim_set_path_cb(&anim_in, lv_anim_path_ease_out);
            lv_anim_start(&anim_in);
        } else {
            lv_obj_set_style_opa(widget->profile_circle, 255, 0);
        }
    } else {
        if (circle_was_visible) {
            lv_anim_t anim_out;
            lv_anim_init(&anim_out);
            lv_anim_set_var(&anim_out, widget->profile_circle);
            lv_anim_set_values(&anim_out, 255, 0);
            lv_anim_set_time(&anim_out, 150);
            lv_anim_set_exec_cb(&anim_out, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
            lv_anim_set_path_cb(&anim_out, lv_anim_path_ease_in);
            lv_anim_set_ready_cb(&anim_out, fade_out_cb);
            lv_anim_start(&anim_out);
        } else {
            lv_obj_add_flag(widget->profile_circle, LV_OBJ_FLAG_HIDDEN);
        }

        uint32_t color = is_open ? 0xffff00 : 0xffffff;
        lv_obj_set_style_arc_color(widget->profile_spinner, lv_color_hex(color), LV_PART_MAIN);
        lv_obj_set_style_arc_color(widget->profile_spinner, lv_color_hex(color), LV_PART_INDICATOR);

        lv_obj_clear_flag(widget->profile_spinner, LV_OBJ_FLAG_HIDDEN);

        if (circle_was_visible) {
            lv_obj_set_style_opa(widget->profile_spinner, 0, 0);
            lv_anim_t anim_in;
            lv_anim_init(&anim_in);
            lv_anim_set_var(&anim_in, widget->profile_spinner);
            lv_anim_set_values(&anim_in, 0, 255);
            lv_anim_set_time(&anim_in, 150);
            lv_anim_set_delay(&anim_in, 150);
            lv_anim_set_exec_cb(&anim_in, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
            lv_anim_set_path_cb(&anim_in, lv_anim_path_ease_out);
            lv_anim_start(&anim_in);
        } else {
            lv_obj_set_style_opa(widget->profile_spinner, 255, 0);
        }
    }

    lv_obj_set_style_text_color(widget->profile_label, lv_color_hex(0xffffff), LV_PART_MAIN);
}

static int endpoint_changed_listener(const zmk_event_t *eh) {
    const struct zmk_endpoint_changed *event = as_zmk_endpoint_changed(eh);
    if (event) {
        struct zmk_endpoint_instance selected = zmk_endpoints_selected();
        active_transport = selected.transport;

        if (active_transport == ZMK_TRANSPORT_BLE) {
            k_work_cancel_delayable(&profile_display_timeout_work);
            zmk_widget_modifier_indicator_set_compact(true);
        } else {
            zmk_widget_modifier_indicator_set_compact(false);
        }

        struct zmk_widget_output *widget;
        SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
            update_profile_indicator(widget, active_profile_index);
        }
    }
    return ZMK_EV_EVENT_BUBBLE;
}

static int ble_active_profile_changed_listener(const zmk_event_t *eh) {
    const struct zmk_ble_active_profile_changed *event = as_zmk_ble_active_profile_changed(eh);
    if (event) {
        active_profile_index = event->index;

        zmk_widget_modifier_indicator_set_compact(true);
        k_work_reschedule(&profile_display_timeout_work, PROFILE_DISPLAY_TIMEOUT);

        struct zmk_widget_output *widget;
        SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
            update_profile_indicator(widget, active_profile_index);
        }
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(widget_output_endpoint, endpoint_changed_listener);
ZMK_SUBSCRIPTION(widget_output_endpoint, zmk_endpoint_changed);

ZMK_LISTENER(widget_output_profile, ble_active_profile_changed_listener);
ZMK_SUBSCRIPTION(widget_output_profile, zmk_ble_active_profile_changed);

int zmk_widget_output_init(struct zmk_widget_output *widget, lv_obj_t *parent) {
    widget->profile_label = lv_label_create(parent);
    lv_obj_set_style_text_font(widget->profile_label, &Symbols_Semibold_32, LV_PART_MAIN);
    lv_obj_align(widget->profile_label, LV_ALIGN_TOP_RIGHT, -22, -5);

    widget->profile_circle = lv_arc_create(parent);
    lv_obj_set_size(widget->profile_circle, 24, 24);
    lv_arc_set_rotation(widget->profile_circle, 0);
    lv_arc_set_bg_angles(widget->profile_circle, 0, 360);
    lv_arc_set_angles(widget->profile_circle, 0, 360);
    lv_obj_set_style_arc_width(widget->profile_circle, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_width(widget->profile_circle, 4, LV_PART_INDICATOR);
    lv_obj_set_style_radius(widget->profile_circle, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(widget->profile_circle, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(widget->profile_circle, 255, LV_PART_MAIN);
    lv_obj_remove_style(widget->profile_circle, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(widget->profile_circle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(widget->profile_circle, LV_ALIGN_TOP_RIGHT, -48, 0);

    widget->profile_spinner = lv_spinner_create(parent, 1000, 90);
    lv_obj_set_size(widget->profile_spinner, 24, 24);
    lv_obj_set_style_arc_width(widget->profile_spinner, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_width(widget->profile_spinner, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(widget->profile_spinner, 0, LV_PART_MAIN);
    lv_obj_align(widget->profile_spinner, LV_ALIGN_TOP_RIGHT, -48, 0);

    if (sys_slist_is_empty(&widgets)) {
        active_profile_index = zmk_ble_active_profile_index();
        struct zmk_endpoint_instance selected = zmk_endpoints_selected();
        active_transport = selected.transport;

        if (active_transport == ZMK_TRANSPORT_BLE) {
            zmk_widget_modifier_indicator_set_compact(true);
        }
    }

    update_profile_indicator(widget, active_profile_index);

    sys_slist_append(&widgets, &widget->node);

    return 0;
}

lv_obj_t *zmk_widget_output_obj(struct zmk_widget_output *widget) {
    return widget->profile_label;
}
