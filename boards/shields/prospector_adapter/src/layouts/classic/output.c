#include "output.h"
#include "battery_bar.h"

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
#define BATTERY_BAR_DELAY K_MSEC(50)
#define FADE_DURATION K_MSEC(200)

#define NUM_ACTIVE       0xffffff
#define NUM_INACTIVE     0xa0a0a0
#define SYM_SENDING      0x00ffff
#define SYM_CONNECTED    0x63c0c0
#define SYM_SEARCHING    0xd0d0d0
#define SYM_UNPAIRED     0x454545

static void profile_display_timeout_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(profile_display_timeout_work, profile_display_timeout_handler);

static void battery_bar_uncompact_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(battery_bar_uncompact_work, battery_bar_uncompact_handler);

static void output_fade_in_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(output_fade_in_work, output_fade_in_handler);

static void profile_refresh_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(profile_refresh_work, profile_refresh_handler);

static uint8_t active_profile_index = 0;
static enum zmk_transport active_transport = ZMK_TRANSPORT_USB;
static bool output_visible = false;

static void set_output_visible(bool visible) {
    output_visible = visible;

    struct zmk_widget_output *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        if (visible) {
            lv_obj_fade_in(widget->container, 200, 0);
        } else {
            lv_obj_fade_out(widget->container, 200, 0);
        }
    }
}

static void profile_display_timeout_handler(struct k_work *work) {
    if (active_transport != ZMK_TRANSPORT_BLE) {
        set_output_visible(false);
        k_work_reschedule(&battery_bar_uncompact_work, BATTERY_BAR_DELAY);
    }
}

static void battery_bar_uncompact_handler(struct k_work *work) {
    zmk_widget_battery_bar_set_compact(false);
}

static void output_fade_in_handler(struct k_work *work) {
    set_output_visible(true);
}

static void set_symbol_opa(void *obj, int32_t opa) {
    lv_obj_set_style_opa(obj, opa, LV_PART_MAIN);
}

static void start_breathing_anim(lv_obj_t *obj) {
    lv_anim_delete(obj, NULL);

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_values(&anim, 155, 255);
    lv_anim_set_duration(&anim, 500);
    lv_anim_set_exec_cb(&anim, set_symbol_opa);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_in_out);
    lv_anim_set_playback_duration(&anim, 500);
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&anim);
}

static void stop_breathing_anim(lv_obj_t *obj) {
    lv_anim_delete(obj, NULL);
    lv_obj_remove_local_style_prop(obj, LV_STYLE_OPA, LV_PART_MAIN);
}

static void update_output_widget(struct zmk_widget_output *widget, uint8_t profile_index) {
    char profile_text[4];
    snprintf(profile_text, sizeof(profile_text), "%d", profile_index);
    lv_label_set_text(widget->profile_label, profile_text);

    bool is_connected = zmk_ble_profile_is_connected(profile_index);
    bool is_open = zmk_ble_profile_is_open(profile_index);
    bool is_ble_active = (active_transport == ZMK_TRANSPORT_BLE);

    lv_label_set_text(widget->links_label, SYMBOL_WAVES_UP);
    stop_breathing_anim(widget->links_label);

    if (is_connected) {
        if (is_ble_active) {
            lv_obj_set_style_text_font(widget->links_label, &Symbols_Bold_26, LV_PART_MAIN);
            lv_obj_set_style_text_color(widget->links_label, lv_color_hex(SYM_SENDING), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget->profile_label, lv_color_hex(NUM_ACTIVE), LV_PART_MAIN);
            lv_obj_set_style_translate_y(widget->links_label, 2, LV_PART_MAIN);
        } else {
            lv_obj_set_style_text_font(widget->links_label, &Symbols_Regular_28, LV_PART_MAIN);
            lv_obj_set_style_text_color(widget->links_label, lv_color_hex(SYM_CONNECTED), LV_PART_MAIN);
            lv_obj_set_style_text_color(widget->profile_label, lv_color_hex(NUM_INACTIVE), LV_PART_MAIN);
            lv_obj_set_style_translate_y(widget->links_label, 0, LV_PART_MAIN);
        }
    } else {
        lv_obj_set_style_text_font(widget->links_label, &Symbols_Regular_28, LV_PART_MAIN);
        lv_obj_set_style_translate_y(widget->links_label, 0, LV_PART_MAIN);
        lv_obj_set_style_text_color(widget->profile_label, lv_color_hex(NUM_INACTIVE), LV_PART_MAIN);
        if (is_open) {
            lv_obj_set_style_text_color(widget->links_label, lv_color_hex(SYM_UNPAIRED), LV_PART_MAIN);
        } else {
            lv_obj_set_style_text_color(widget->links_label, lv_color_hex(SYM_SEARCHING), LV_PART_MAIN);
            start_breathing_anim(widget->links_label);
        }
    }

    lv_obj_invalidate(widget->container);
}

static void profile_refresh_handler(struct k_work *work) {
    struct zmk_widget_output *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        update_output_widget(widget, active_profile_index);
    }
    set_output_visible(true);
}

static int endpoint_changed_listener(const zmk_event_t *eh) {
    const struct zmk_endpoint_changed *event = as_zmk_endpoint_changed(eh);
    if (event) {
        struct zmk_endpoint_instance selected = zmk_endpoints_selected();
        active_transport = selected.transport;

        k_work_cancel_delayable(&profile_refresh_work);

        if (active_transport == ZMK_TRANSPORT_BLE) {
            k_work_cancel_delayable(&profile_display_timeout_work);
            if (!output_visible) {
                zmk_widget_battery_bar_set_compact(true);
                k_work_reschedule(&output_fade_in_work, BATTERY_BAR_DELAY);
            }
        } else {
            if (!output_visible) {
                zmk_widget_battery_bar_set_compact(true);
                k_work_reschedule(&output_fade_in_work, BATTERY_BAR_DELAY);
            }
            k_work_reschedule(&profile_display_timeout_work, PROFILE_DISPLAY_TIMEOUT);
        }

        struct zmk_widget_output *widget;
        SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
            update_output_widget(widget, active_profile_index);
        }
    }
    return ZMK_EV_EVENT_BUBBLE;
}

static int ble_active_profile_changed_listener(const zmk_event_t *eh) {
    const struct zmk_ble_active_profile_changed *event = as_zmk_ble_active_profile_changed(eh);
    if (event) {
        active_profile_index = event->index;

        if (output_visible) {
            set_output_visible(false);
            k_work_reschedule(&profile_refresh_work, FADE_DURATION);
        } else {
            struct zmk_widget_output *widget;
            SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
                update_output_widget(widget, active_profile_index);
            }
            zmk_widget_battery_bar_set_compact(true);
            k_work_reschedule(&output_fade_in_work, BATTERY_BAR_DELAY);
        }
        k_work_reschedule(&profile_display_timeout_work, PROFILE_DISPLAY_TIMEOUT);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(widget_output_endpoint, endpoint_changed_listener);
ZMK_SUBSCRIPTION(widget_output_endpoint, zmk_endpoint_changed);

ZMK_LISTENER(widget_output_profile, ble_active_profile_changed_listener);
ZMK_SUBSCRIPTION(widget_output_profile, zmk_ble_active_profile_changed);

int zmk_widget_output_init(struct zmk_widget_output *widget, lv_obj_t *parent) {
    widget->container = lv_obj_create(parent);
    lv_obj_set_size(widget->container, 50, 30);
    lv_obj_set_style_border_width(widget->container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(widget->container, 0, LV_PART_MAIN);
    // lv_obj_set_style_bg_color(widget->container, lv_color_hex(0x0000ff), LV_PART_MAIN);
    // lv_obj_set_style_bg_opa(widget->container, LV_OPA_COVER, LV_PART_MAIN);

    widget->links_label = lv_label_create(widget->container);
    lv_obj_set_size(widget->links_label, 30, 30);
    lv_obj_set_style_text_font(widget->links_label, &Symbols_Regular_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(widget->links_label, lv_color_hex(NUM_INACTIVE), LV_PART_MAIN);
    lv_obj_set_style_text_align(widget->links_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(widget->links_label, 0, -1);

    widget->profile_label = lv_label_create(widget->container);
    lv_obj_set_style_text_font(widget->profile_label, &FG_Medium_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(widget->profile_label, lv_color_hex(NUM_INACTIVE), LV_PART_MAIN);
    lv_obj_align(widget->profile_label, LV_ALIGN_RIGHT_MID, 0, 1);

    if (sys_slist_is_empty(&widgets)) {
        active_profile_index = zmk_ble_active_profile_index();
        struct zmk_endpoint_instance selected = zmk_endpoints_selected();
        active_transport = selected.transport;

        if (active_transport == ZMK_TRANSPORT_BLE) {
            zmk_widget_battery_bar_set_compact(true);
            output_visible = true;
        } else {
            lv_obj_set_style_opa(widget->container, 0, LV_PART_MAIN);
        }
    }

    update_output_widget(widget, active_profile_index);

    sys_slist_append(&widgets, &widget->node);

    return 0;
}

lv_obj_t *zmk_widget_output_obj(struct zmk_widget_output *widget) {
    return widget->container;
}
