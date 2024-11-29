#include "caps_word_indicator.h"

#include <zmk/display.h>
#include <zmk/events/caps_word_state_changed.h>
#include <zmk/event_manager.h>

#include <fonts.h>
#include <sf_symbols.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

static bool initialized = false;

struct caps_word_indicator_state {
    bool active;
};

static void caps_word_indicator_set_active(lv_obj_t *label, struct caps_word_indicator_state state) {
    if (initialized) {
        if (state.active) {
            lv_obj_set_style_text_color(label, lv_color_hex(0x00ffe5), LV_PART_MAIN);
        } else {
            lv_obj_set_style_text_color(label, lv_color_hex(0x202020), LV_PART_MAIN);
        }
    }
}

static void caps_word_indicator_update_cb(struct caps_word_indicator_state state) {
    struct zmk_widget_caps_word_indicator *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        caps_word_indicator_set_active(widget->obj, state);
    }
}

static struct caps_word_indicator_state caps_word_indicator_get_state(const zmk_event_t *eh) {
    const struct zmk_caps_word_state_changed *ev =
        as_zmk_caps_word_state_changed(eh);
    LOG_INF("DISP | Caps Word State Changed: %d", ev->active);
    return (struct caps_word_indicator_state){
        .active = ev->active,
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_caps_word_indicator, struct caps_word_indicator_state,
                            caps_word_indicator_update_cb, caps_word_indicator_get_state)
ZMK_SUBSCRIPTION(widget_caps_word_indicator, zmk_caps_word_state_changed);

int zmk_widget_caps_word_indicator_init(struct zmk_widget_caps_word_indicator *widget,
                                        lv_obj_t *parent) {
    widget->obj = lv_label_create(parent);

    // LV_FONT_DECLARE(SF_Compact_Text_Bold_32);

    lv_label_set_text(widget->obj, SF_SYMBOL_CHARACTER_CURSOR_IBEAM);
    lv_obj_set_style_text_color(widget->obj, lv_color_hex(0x030303), LV_PART_MAIN);
    lv_obj_set_style_text_font(widget->obj, &SF_Compact_Text_Bold_32, LV_PART_MAIN);
    lv_obj_set_style_text_align(widget->obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    sys_slist_append(&widgets, &widget->node);

    widget_caps_word_indicator_init();

    initialized = true;

    return 0;
}

lv_obj_t *zmk_widget_caps_word_indicator_obj(struct zmk_widget_caps_word_indicator *widget) {
    return widget->obj;
}