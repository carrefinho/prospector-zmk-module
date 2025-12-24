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
#include <modifier_order.h>
#include "display_colors.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

#define WIN_ICON_SIZE 32
#define WIN_SQUARE_SIZE 14
#define WIN_SQUARE_GAP 4

struct modifier_indicator_state {
    bool mods[4];
#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
    bool caps_word;
#endif
};

#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
static bool caps_word_active = false;
#endif

static void set_modifier_color(lv_obj_t *obj, bool active, bool is_win_icon) {
    lv_color_t color = active ? lv_color_hex(DISPLAY_COLOR_MOD_ACTIVE)
                               : lv_color_hex(DISPLAY_COLOR_MOD_INACTIVE);
    if (is_win_icon) {
        for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) {
            lv_obj_set_style_bg_color(lv_obj_get_child(obj, i), color, LV_PART_MAIN);
        }
    } else {
        lv_obj_set_style_text_color(obj, color, 0);
    }
}

static lv_obj_t *create_win_icon(lv_obj_t *parent) {
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, WIN_ICON_SIZE, WIN_ICON_SIZE);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(container, 0, LV_PART_MAIN);

    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 2; col++) {
            lv_obj_t *square = lv_obj_create(container);
            lv_obj_set_size(square, WIN_SQUARE_SIZE, WIN_SQUARE_SIZE);
            lv_obj_set_pos(square, col * (WIN_SQUARE_SIZE + WIN_SQUARE_GAP),
                           row * (WIN_SQUARE_SIZE + WIN_SQUARE_GAP));
            lv_obj_set_style_bg_color(square, lv_color_hex(DISPLAY_COLOR_MOD_INACTIVE), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(square, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_border_width(square, 0, LV_PART_MAIN);
            lv_obj_set_style_radius(square, 2, LV_PART_MAIN);
            lv_obj_set_style_pad_all(square, 0, LV_PART_MAIN);
        }
    }
    return container;
}

static void modifier_indicator_update_cb(struct modifier_indicator_state state) {
    struct zmk_widget_modifier_indicator *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        bool is_windows = modifier_order_is_windows();
        for (int i = 0; i < 4; i++) {
            enum modifier_type type = modifier_order_get(i);
            bool active = state.mods[type];
            bool is_win_icon = is_windows && type == MOD_TYPE_GUI;
#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
            if (type == MOD_TYPE_SHIFT) {
                if (state.caps_word) {
                    if (modifier_order_uses_symbols()) {
                        lv_label_set_text(widget->mod_labels[i], SYMBOL_SHIFT_FILLED);
                    }
                    set_modifier_color(widget->mod_labels[i], true, false);
                    continue;
                } else if (modifier_order_uses_symbols()) {
                    lv_label_set_text(widget->mod_labels[i], SYMBOL_SHIFT);
                }
            }
#endif
            set_modifier_color(widget->mod_labels[i], active, is_win_icon);
        }
    }
}

#define PANEL_HEIGHT_FULL 178
#define PANEL_HEIGHT_COMPACT 148

static void animate_panel_resize(lv_obj_t *obj, int32_t target_height) {
    int32_t current_height = lv_obj_get_height(obj);

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_values(&anim, current_height, target_height);
    lv_anim_set_time(&anim, 150);
    lv_anim_set_exec_cb(&anim, (lv_anim_exec_xcb_t)lv_obj_set_height);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_start(&anim);
}

void zmk_widget_modifier_indicator_set_compact(bool compact) {
    struct zmk_widget_modifier_indicator *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        animate_panel_resize(widget->obj, compact ? PANEL_HEIGHT_COMPACT : PANEL_HEIGHT_FULL);
    }
}

static struct modifier_indicator_state modifier_indicator_get_state(const zmk_event_t *eh) {
    zmk_mod_flags_t mods = zmk_hid_get_explicit_mods();

#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
    if (eh != NULL) {
        const struct zmk_caps_word_state_changed *ev = as_zmk_caps_word_state_changed(eh);
        if (ev != NULL) {
            caps_word_active = ev->active;
        }
    }
#endif

    struct modifier_indicator_state state = {
        .mods = {false, false, false, false},
#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
        .caps_word = caps_word_active,
#endif
    };

    state.mods[MOD_TYPE_GUI] = (mods & (MOD_LGUI | MOD_RGUI)) != 0;
    state.mods[MOD_TYPE_ALT] = (mods & (MOD_LALT | MOD_RALT)) != 0;
    state.mods[MOD_TYPE_CTRL] = (mods & (MOD_LCTL | MOD_RCTL)) != 0;
    state.mods[MOD_TYPE_SHIFT] = (mods & (MOD_LSFT | MOD_RSFT)) != 0;

    return state;
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_modifier_indicator, struct modifier_indicator_state,
                            modifier_indicator_update_cb, modifier_indicator_get_state)
ZMK_SUBSCRIPTION(widget_modifier_indicator, zmk_keycode_state_changed);

#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
ZMK_SUBSCRIPTION(widget_modifier_indicator, zmk_caps_word_state_changed);
#endif

static const int32_t mod_positions[4][2] = {
    {14, 27}, {50, 27}, {14, 64}, {50, 64}
};

int zmk_widget_modifier_indicator_init(struct zmk_widget_modifier_indicator *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 108, 178);
    lv_obj_set_style_bg_color(widget->obj, lv_color_hex(DISPLAY_COLOR_MOD_PANEL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(widget->obj, 255, LV_PART_MAIN);
    lv_obj_set_style_radius(widget->obj, 24, LV_PART_MAIN);
    lv_obj_set_style_border_width(widget->obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(widget->obj, 0, LV_PART_MAIN);

    bool use_symbols = modifier_order_uses_symbols();
    bool is_windows = modifier_order_is_windows();
    for (int i = 0; i < 4; i++) {
        enum modifier_type type = modifier_order_get(i);
        if (is_windows && type == MOD_TYPE_GUI) {
            widget->mod_labels[i] = create_win_icon(widget->obj);
        } else if (use_symbols) {
            widget->mod_labels[i] = lv_label_create(widget->obj);
            lv_label_set_text(widget->mod_labels[i], modifier_order_get_symbol(i));
            lv_obj_set_style_text_font(widget->mod_labels[i], &Symbols_Semibold_32, LV_PART_MAIN);
            lv_obj_set_style_text_color(widget->mod_labels[i], lv_color_hex(DISPLAY_COLOR_MOD_INACTIVE), LV_PART_MAIN);
        } else {
            widget->mod_labels[i] = lv_label_create(widget->obj);
            lv_label_set_text(widget->mod_labels[i], modifier_order_get_text(i));
            lv_obj_set_style_text_font(widget->mod_labels[i], &DINishCondensed_SemiBold_22, LV_PART_MAIN);
            lv_obj_set_style_text_color(widget->mod_labels[i], lv_color_hex(DISPLAY_COLOR_MOD_INACTIVE), LV_PART_MAIN);
        }
        lv_obj_set_pos(widget->mod_labels[i], mod_positions[i][0], mod_positions[i][1]);
    }

    sys_slist_append(&widgets, &widget->node);

    widget_modifier_indicator_init();

    return 0;
}

lv_obj_t *zmk_widget_modifier_indicator_obj(struct zmk_widget_modifier_indicator *widget) {
    return widget->obj;
}
