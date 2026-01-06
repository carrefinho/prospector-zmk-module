#include "modifier_indicator.h"
#include "line_segments.h"

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

#define MODIFIER_GRID_COL 7
#define MODIFIER_GRID_X (LINE_SEGMENTS_GRID_OFFSET + MODIFIER_GRID_COL * LINE_SEGMENTS_SPACING)
#define MODIFIER_CENTER_OFFSET_X (MODIFIER_GRID_X - LINE_SEGMENTS_WIDTH / 2)

#define GRID_ROW_Y(row) (LINE_SEGMENTS_GRID_OFFSET + (row) * LINE_SEGMENTS_SPACING)
#define MODIFIER_CENTER_OFFSET_Y(row) (GRID_ROW_Y(row) - LINE_SEGMENTS_HEIGHT / 2)

static const int grid_rows[4] = {2, 3, 4, 5};

#define WIN_ICON_SIZE 24
#define WIN_SQUARE_SIZE 10
#define WIN_SQUARE_GAP 4

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct modifier_state {
    bool mods[4];
#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
    bool caps_word;
#endif
};

#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
static bool caps_word_active = false;
#endif

static void set_modifier_visible(lv_obj_t *obj, int grid_row, bool active, bool is_win_icon) {
#ifdef CONFIG_PROSPECTOR_SHOW_INACTIVE_MODIFIERS
    lv_color_t color = active ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x404040);
    if (is_win_icon) {
        for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) {
            lv_obj_set_style_bg_color(lv_obj_get_child(obj, i), color, LV_PART_MAIN);
        }
    } else {
        lv_obj_set_style_text_color(obj, color, LV_PART_MAIN);
    }
    lv_obj_set_style_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    zmk_widget_line_segments_set_cell_excluded(MODIFIER_GRID_COL, grid_row, true);
#else
    lv_color_t color = active ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x000000);
    lv_opa_t opa = active ? LV_OPA_COVER : LV_OPA_TRANSP;
    if (is_win_icon) {
        for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) {
            lv_obj_set_style_bg_color(lv_obj_get_child(obj, i), color, LV_PART_MAIN);
        }
    } else {
        lv_obj_set_style_text_color(obj, color, LV_PART_MAIN);
    }
    lv_obj_set_style_opa(obj, opa, LV_PART_MAIN);
    zmk_widget_line_segments_set_cell_excluded(MODIFIER_GRID_COL, grid_row, active);
#endif
}

static lv_obj_t *create_win_icon(lv_obj_t *parent) {
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, WIN_ICON_SIZE, WIN_ICON_SIZE);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(container, 0, LV_PART_MAIN);
    lv_obj_center(container);

    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 2; col++) {
            lv_obj_t *square = lv_obj_create(container);
            lv_obj_set_size(square, WIN_SQUARE_SIZE, WIN_SQUARE_SIZE);
            lv_obj_set_pos(square, col * (WIN_SQUARE_SIZE + WIN_SQUARE_GAP),
                           row * (WIN_SQUARE_SIZE + WIN_SQUARE_GAP));
#ifdef CONFIG_PROSPECTOR_SHOW_INACTIVE_MODIFIERS
            lv_obj_set_style_bg_color(square, lv_color_hex(0x404040), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(square, LV_OPA_COVER, LV_PART_MAIN);
#else
            lv_obj_set_style_bg_color(square, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(square, LV_OPA_TRANSP, LV_PART_MAIN);
#endif
            lv_obj_set_style_border_width(square, 0, LV_PART_MAIN);
            lv_obj_set_style_radius(square, 2, LV_PART_MAIN);
            lv_obj_set_style_pad_all(square, 0, LV_PART_MAIN);
        }
    }
    return container;
}

static void modifier_update_cb(struct modifier_state state) {
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
                    set_modifier_visible(widget->mod_labels[i], grid_rows[i], true, false);
                    continue;
                } else if (modifier_order_uses_symbols()) {
                    lv_label_set_text(widget->mod_labels[i], SYMBOL_SHIFT);
                }
            }
#endif
            set_modifier_visible(widget->mod_labels[i], grid_rows[i], active, is_win_icon);
        }
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
        .mods = {false, false, false, false},
#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
        .caps_word = caps_word_active,
#endif
    };

    zmk_mod_flags_t mods = zmk_hid_get_explicit_mods();
    state.mods[MOD_TYPE_GUI] = (mods & (MOD_LGUI | MOD_RGUI)) != 0;
    state.mods[MOD_TYPE_ALT] = (mods & (MOD_LALT | MOD_RALT)) != 0;
    state.mods[MOD_TYPE_CTRL] = (mods & (MOD_LCTL | MOD_RCTL)) != 0;
    state.mods[MOD_TYPE_SHIFT] = (mods & (MOD_LSFT | MOD_RSFT)) != 0;

    return state;
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_modifier_indicator, struct modifier_state,
                            modifier_update_cb, modifier_indicator_get_state)
ZMK_SUBSCRIPTION(widget_modifier_indicator, zmk_keycode_state_changed);

#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
ZMK_SUBSCRIPTION(widget_modifier_indicator, zmk_caps_word_state_changed);
#endif

static lv_obj_t *create_modifier_container(lv_obj_t *parent, int grid_row) {
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, LINE_SEGMENTS_SPACING, LINE_SEGMENTS_SPACING);
    lv_obj_set_style_bg_opa(container, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(container, 0, LV_PART_MAIN);
    lv_obj_align(container, LV_ALIGN_CENTER, MODIFIER_CENTER_OFFSET_X, MODIFIER_CENTER_OFFSET_Y(grid_row));
    return container;
}

static lv_obj_t *create_modifier_label(lv_obj_t *container, const char *text, bool use_symbols) {
    lv_obj_t *label = lv_label_create(container);
    lv_label_set_text(label, text);
    if (use_symbols) {
        lv_obj_set_style_text_font(label, &Symbols_Regular_28, LV_PART_MAIN);
    } else {
        lv_obj_set_style_text_font(label, &DINishCondensed_SemiBold_20, LV_PART_MAIN);
    }
#ifdef CONFIG_PROSPECTOR_SHOW_INACTIVE_MODIFIERS
    lv_obj_set_style_text_color(label, lv_color_hex(0x404040), LV_PART_MAIN);
    lv_obj_set_style_opa(label, LV_OPA_COVER, LV_PART_MAIN);
#else
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_opa(label, LV_OPA_TRANSP, LV_PART_MAIN);
#endif
    lv_obj_center(label);
    return label;
}

int zmk_widget_modifier_indicator_init(struct zmk_widget_modifier_indicator *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 0, 0);
    lv_obj_set_style_bg_opa(widget->obj, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(widget->obj, 0, LV_PART_MAIN);
    lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);

    bool use_symbols = modifier_order_uses_symbols();
    bool is_windows = modifier_order_is_windows();
    for (int i = 0; i < 4; i++) {
        widget->mod_containers[i] = create_modifier_container(parent, grid_rows[i]);
        enum modifier_type type = modifier_order_get(i);
        if (is_windows && type == MOD_TYPE_GUI) {
            widget->mod_labels[i] = create_win_icon(widget->mod_containers[i]);
        } else {
            const char *text = use_symbols ? modifier_order_get_symbol(i) : modifier_order_get_text(i);
            widget->mod_labels[i] = create_modifier_label(widget->mod_containers[i], text, use_symbols);
        }
    }

#ifdef CONFIG_PROSPECTOR_SHOW_INACTIVE_MODIFIERS
    for (int i = 0; i < 4; i++) {
        zmk_widget_line_segments_set_cell_excluded(MODIFIER_GRID_COL, grid_rows[i], true);
    }
#endif

    sys_slist_append(&widgets, &widget->node);

    widget_modifier_indicator_init();

    return 0;
}

lv_obj_t *zmk_widget_modifier_indicator_obj(struct zmk_widget_modifier_indicator *widget) {
    return widget->obj;
}
