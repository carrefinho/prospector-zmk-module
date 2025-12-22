#include "output.h"

#include <zmk/display.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/event_manager.h>
#include <zmk/endpoints.h>
#include <zmk/ble.h>

#include <fonts.h>
#include "display_colors.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

static uint8_t active_profile_index = 0;
static enum zmk_transport active_transport = ZMK_TRANSPORT_USB;

static void set_usb_btn_state(lv_obj_t *btn, bool active) {
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    if (active) {
        lv_obj_set_style_bg_color(btn, lv_color_hex(DISPLAY_COLOR_USB_ACTIVE_BG), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
        if (label) {
            lv_obj_set_style_text_color(label, lv_color_hex(DISPLAY_COLOR_OUTPUT_ACTIVE_TEXT), LV_PART_MAIN);
        }
    } else {
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
        lv_obj_set_style_border_color(btn, lv_color_hex(DISPLAY_COLOR_USB_INACTIVE_BG), LV_PART_MAIN);
        if (label) {
            lv_obj_set_style_text_color(label, lv_color_hex(DISPLAY_COLOR_USB_INACTIVE_BG), LV_PART_MAIN);
        }
    }
}

static void set_ble_btn_state(lv_obj_t *btn, bool active) {
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    if (active) {
        lv_obj_set_style_bg_color(btn, lv_color_hex(DISPLAY_COLOR_BLE_ACTIVE_BG), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
        if (label) {
            lv_obj_set_style_text_color(label, lv_color_hex(DISPLAY_COLOR_OUTPUT_ACTIVE_TEXT), LV_PART_MAIN);
        }
    } else {
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
        lv_obj_set_style_border_color(btn, lv_color_hex(DISPLAY_COLOR_BLE_INACTIVE_BG), LV_PART_MAIN);
        if (label) {
            lv_obj_set_style_text_color(label, lv_color_hex(DISPLAY_COLOR_BLE_INACTIVE_BG), LV_PART_MAIN);
        }
    }
}

static void set_slot_active(lv_obj_t *slot, bool active) {
    if (active) {
        lv_obj_set_style_bg_color(slot, lv_color_hex(DISPLAY_COLOR_SLOT_ACTIVE_BG), LV_PART_MAIN);
    } else {
        lv_obj_set_style_bg_color(slot, lv_color_hex(DISPLAY_COLOR_SLOT_INACTIVE_BG), LV_PART_MAIN);
    }
}

static void update_output_widget(struct zmk_widget_output *widget) {
    bool is_usb = (active_transport == ZMK_TRANSPORT_USB);
    set_usb_btn_state(widget->usb_btn, is_usb);
    set_ble_btn_state(widget->ble_btn, !is_usb);

    for (int i = 0; i < 4; i++) {
        set_slot_active(widget->slots[i], (i == active_profile_index));
    }
}

static int endpoint_changed_listener(const zmk_event_t *eh) {
    const struct zmk_endpoint_changed *event = as_zmk_endpoint_changed(eh);
    if (event) {
        struct zmk_endpoint_instance selected = zmk_endpoints_selected();
        active_transport = selected.transport;

        struct zmk_widget_output *widget;
        SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
            update_output_widget(widget);
        }
    }
    return ZMK_EV_EVENT_BUBBLE;
}

static int ble_active_profile_changed_listener(const zmk_event_t *eh) {
    const struct zmk_ble_active_profile_changed *event = as_zmk_ble_active_profile_changed(eh);
    if (event) {
        active_profile_index = event->index;

        struct zmk_widget_output *widget;
        SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
            update_output_widget(widget);
        }
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(widget_output_endpoint, endpoint_changed_listener);
ZMK_SUBSCRIPTION(widget_output_endpoint, zmk_endpoint_changed);

ZMK_LISTENER(widget_output_profile, ble_active_profile_changed_listener);
ZMK_SUBSCRIPTION(widget_output_profile, zmk_ble_active_profile_changed);

static lv_obj_t *create_toggle_btn(lv_obj_t *parent, const char *text, int x) {
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_set_size(btn, 56, 29);
    lv_obj_set_pos(btn, x, 0);
    lv_obj_set_style_radius(btn, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &FG_Medium_20, LV_PART_MAIN);
    lv_obj_center(label);
    lv_obj_set_style_translate_y(label, 1, LV_PART_MAIN);

    return btn;
}

static lv_obj_t *create_slot_btn(lv_obj_t *parent, int index, int x) {
    lv_obj_t *slot = lv_obj_create(parent);
    lv_obj_set_size(slot, 27, 29);
    lv_obj_set_pos(slot, x, 33);
    lv_obj_set_style_radius(slot, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(slot, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(slot, 0, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(slot);
    char text[2];
    snprintf(text, sizeof(text), "%d", index + 1);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &FG_Medium_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_center(label);
    lv_obj_set_style_translate_y(label, 1, LV_PART_MAIN);

    return slot;
}

int zmk_widget_output_init(struct zmk_widget_output *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 116, 62);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(widget->obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(widget->obj, 0, LV_PART_MAIN);

    widget->usb_btn = create_toggle_btn(widget->obj, "USB", 0);
    widget->ble_btn = create_toggle_btn(widget->obj, "BLE", 58);

    for (int i = 0; i < 4; i++) {
        widget->slots[i] = create_slot_btn(widget->obj, i, i * 29);
    }

    if (sys_slist_is_empty(&widgets)) {
        active_profile_index = zmk_ble_active_profile_index();
        struct zmk_endpoint_instance selected = zmk_endpoints_selected();
        active_transport = selected.transport;
    }

    update_output_widget(widget);

    sys_slist_append(&widgets, &widget->node);

    return 0;
}

lv_obj_t *zmk_widget_output_obj(struct zmk_widget_output *widget) {
    return widget->obj;
}
