#include "modifier_indicator.h"

#include <zmk/display.h>
#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
#include <zmk/events/caps_word_state_changed.h>
#endif
#include <zmk/events/keycode_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/hid.h>
#include <dt-bindings/zmk/hid_usage.h>
#include <dt-bindings/zmk/hid_usage_pages.h>

#include <fonts.h>
#include <symbols.h>

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct modifier_state {
#ifdef CONFIG_PROSPECTOR_SHOW_MODIFIERS
    bool cmd;
    bool opt;
    bool ctrl;
    bool shift;
#endif
#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
    bool caps_word;
#endif
};

#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
static bool caps_word_active = false;
#endif

static void set_modifier_state(lv_obj_t *label, bool active) {
#ifdef CONFIG_PROSPECTOR_SHOW_INACTIVE_MODIFIERS
    lv_color_t color = active ? lv_color_hex(0x00ffe5) : lv_color_hex(0x101010);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
#else
    lv_obj_set_style_text_color(label, lv_color_hex(0x00ffe5), LV_PART_MAIN);
    lv_obj_set_style_opa(label, active ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);
#endif
}

static void modifier_update_cb(struct modifier_state state) {
    struct zmk_widget_modifier_indicator *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
#ifdef CONFIG_PROSPECTOR_SHOW_MODIFIERS
        set_modifier_state(widget->cmd_label, state.cmd);
        set_modifier_state(widget->opt_label, state.opt);
        set_modifier_state(widget->ctrl_label, state.ctrl);
#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
        if (state.caps_word) {
            lv_label_set_text(widget->shift_label, SYMBOL_SHIFT_FILLED);
            set_modifier_state(widget->shift_label, true);
        } else {
            lv_label_set_text(widget->shift_label, SYMBOL_SHIFT);
            set_modifier_state(widget->shift_label, state.shift);
        }
#else
        set_modifier_state(widget->shift_label, state.shift);
#endif
#else
#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
        set_modifier_state(widget->shift_label, state.caps_word);
#endif
#endif
    }
}

static struct modifier_state modifier_indicator_get_state(const zmk_event_t *eh) {
#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
    if (eh != NULL) {
        const struct zmk_caps_word_state_changed *ev = as_zmk_caps_word_state_changed(eh);
        if (ev != NULL) {
            caps_word_active = ev->active;
        }
    }
#endif

    struct modifier_state state = {
#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
        .caps_word = caps_word_active,
#endif
#ifdef CONFIG_PROSPECTOR_SHOW_MODIFIERS
        .cmd = false,
        .opt = false,
        .ctrl = false,
        .shift = false,
#endif
    };

#ifdef CONFIG_PROSPECTOR_SHOW_MODIFIERS
    zmk_mod_flags_t mods = zmk_hid_get_explicit_mods();
    state.shift = (mods & (MOD_LSFT | MOD_RSFT)) != 0;
    state.ctrl = (mods & (MOD_LCTL | MOD_RCTL)) != 0;
    state.opt = (mods & (MOD_LALT | MOD_RALT)) != 0;
    state.cmd = (mods & (MOD_LGUI | MOD_RGUI)) != 0;
#endif

    return state;
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_modifier_indicator, struct modifier_state,
                            modifier_update_cb, modifier_indicator_get_state)

#ifdef CONFIG_PROSPECTOR_SHOW_MODIFIERS
ZMK_SUBSCRIPTION(widget_modifier_indicator, zmk_keycode_state_changed);
#endif

#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
ZMK_SUBSCRIPTION(widget_modifier_indicator, zmk_caps_word_state_changed);
#endif

int zmk_widget_modifier_indicator_init(struct zmk_widget_modifier_indicator *widget,
                                        lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(widget->obj, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(widget->obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(widget->obj, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(widget->obj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(widget->obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(widget->obj, 8, LV_PART_MAIN);

#ifdef CONFIG_PROSPECTOR_SHOW_MODIFIERS
    widget->cmd_label = lv_label_create(widget->obj);
    lv_label_set_text(widget->cmd_label, SYMBOL_COMMAND);
    lv_obj_set_style_text_font(widget->cmd_label, &Symbols_Semibold_32, LV_PART_MAIN);
    lv_obj_set_style_text_color(widget->cmd_label, lv_color_hex(0x101010), LV_PART_MAIN);

    widget->opt_label = lv_label_create(widget->obj);
    lv_label_set_text(widget->opt_label, SYMBOL_OPTION);
    lv_obj_set_style_text_font(widget->opt_label, &Symbols_Semibold_32, LV_PART_MAIN);
    lv_obj_set_style_text_color(widget->opt_label, lv_color_hex(0x101010), LV_PART_MAIN);

    widget->ctrl_label = lv_label_create(widget->obj);
    lv_label_set_text(widget->ctrl_label, SYMBOL_CONTROL);
    lv_obj_set_style_text_font(widget->ctrl_label, &Symbols_Semibold_32, LV_PART_MAIN);
    lv_obj_set_style_text_color(widget->ctrl_label, lv_color_hex(0x101010), LV_PART_MAIN);
#endif

    widget->shift_label = lv_label_create(widget->obj);
#if defined(CONFIG_PROSPECTOR_SHOW_MODIFIERS)
    lv_label_set_text(widget->shift_label, SYMBOL_SHIFT);
#else
    lv_label_set_text(widget->shift_label, SYMBOL_SHIFT_FILLED);
#endif
    lv_obj_set_style_text_font(widget->shift_label, &Symbols_Semibold_32, LV_PART_MAIN);
    lv_obj_set_style_text_color(widget->shift_label, lv_color_hex(0x101010), LV_PART_MAIN);

    sys_slist_append(&widgets, &widget->node);

    widget_modifier_indicator_init();

    return 0;
}

lv_obj_t *zmk_widget_modifier_indicator_obj(struct zmk_widget_modifier_indicator *widget) {
    return widget->obj;
}
