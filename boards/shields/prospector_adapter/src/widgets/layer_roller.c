#include "layer_roller.h"

#include <ctype.h>
#include <zmk/display.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/keymap.h>

#include <fonts.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static char layer_names_buffer[512] = {0};

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct layer_roller_state {
    uint8_t index;
};

static void layer_roller_set_sel(lv_obj_t *roller, struct layer_roller_state state) {
    lv_roller_set_selected(roller, state.index, LV_ANIM_ON);
}

static void layer_roller_update_cb(struct layer_roller_state state) {
    struct zmk_widget_layer_roller *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        layer_roller_set_sel(widget->obj, state);
    }
}

static struct layer_roller_state layer_roller_get_state(const zmk_event_t *eh) {
    uint8_t index = zmk_keymap_highest_layer_active();
    LOG_INF("Roller set to: %d", index);
    return (struct layer_roller_state){
        .index = index,
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_roller, struct layer_roller_state, layer_roller_update_cb,
                            layer_roller_get_state)
ZMK_SUBSCRIPTION(widget_layer_roller, zmk_layer_state_changed);

int zmk_widget_layer_roller_init(struct zmk_widget_layer_roller *widget, lv_obj_t *parent) {
    widget->obj = lv_roller_create(parent);

    layer_names_buffer[0] = '\0';
    char *ptr = layer_names_buffer;

    for (int i = 0; i < ZMK_KEYMAP_LAYERS_LEN; i++) {
        const char *layer_name = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(i));
        if (layer_name) {
            if (i > 0) {
                strcat(ptr, "\n");
                ptr += strlen(ptr);
            }

            if (layer_name && *layer_name) {
#if IS_ENABLED(CONFIG_LAYER_ROLLER_ALL_CAPS)
                while (*layer_name) {
                    *ptr = toupper((unsigned char)*layer_name);
                    ptr++;
                    layer_name++;
                }
                *ptr = '\0';
#else
                strcat(ptr, layer_name);
                ptr += strlen(layer_name);
#endif
            } else {
                char index_str[12];
                snprintf(index_str, sizeof(index_str), "%d", i);
                strcat(ptr, index_str);
                ptr += strlen(index_str);
            }
        }
    }

    lv_roller_set_options(widget->obj, layer_names_buffer, LV_ROLLER_MODE_INFINITE);

    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_bg_color(&style, lv_color_black());
    lv_style_set_text_color(&style, lv_color_white());
    lv_style_set_border_width(&style, 0);
    lv_style_set_pad_all(&style, 0);

    lv_obj_add_style(widget->obj, &style, 0);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, LV_PART_SELECTED);
    lv_obj_set_style_text_font(widget->obj, &FRAC_Regular_48, LV_PART_SELECTED);
    lv_obj_set_style_text_color(widget->obj, lv_color_hex(0xffffff), LV_PART_SELECTED);
    lv_obj_set_style_text_font(widget->obj, &FRAC_Thin_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(widget->obj, lv_color_hex(0x909090), LV_PART_MAIN);

    int32_t fade_height = 40;

    lv_obj_t *fade_top = lv_obj_create(widget->obj);
    lv_obj_remove_style_all(fade_top);
    lv_obj_set_size(fade_top, LV_PCT(100), fade_height);
    lv_obj_align(fade_top, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(fade_top, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(fade_top, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_color(fade_top, lv_color_black(), 0);
    lv_obj_set_style_bg_grad_dir(fade_top, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_opa(fade_top, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_main_opa(fade_top, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_opa(fade_top, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(fade_top, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *fade_bottom = lv_obj_create(widget->obj);
    lv_obj_remove_style_all(fade_bottom);
    lv_obj_set_size(fade_bottom, LV_PCT(100), fade_height);
    lv_obj_align(fade_bottom, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(fade_bottom, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(fade_bottom, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_color(fade_bottom, lv_color_black(), 0);
    lv_obj_set_style_bg_grad_dir(fade_bottom, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_opa(fade_bottom, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_main_opa(fade_bottom, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_grad_opa(fade_bottom, LV_OPA_COVER, 0);
    lv_obj_remove_flag(fade_bottom, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_anim_time(widget->obj, 50, 0);

    sys_slist_append(&widgets, &widget->node);

    widget_layer_roller_init();
    return 0;
}

lv_obj_t *zmk_widget_layer_roller_obj(struct zmk_widget_layer_roller *widget) {
    return widget->obj;
}
