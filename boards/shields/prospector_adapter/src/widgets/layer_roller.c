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

static void mask_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);

    static int16_t mask_top_id = -1;
    static int16_t mask_bottom_id = -1;

    if(code == LV_EVENT_COVER_CHECK) {
        lv_event_set_cover_res(e, LV_COVER_RES_MASKED);

    }
    else if(code == LV_EVENT_DRAW_MAIN_BEGIN) {
        /* add mask */
        const lv_font_t * font = lv_obj_get_style_text_font(obj, LV_PART_MAIN);
        lv_coord_t line_space = lv_obj_get_style_text_line_space(obj, LV_PART_MAIN);
        lv_coord_t font_h = lv_font_get_line_height(font);

        lv_area_t roller_coords;
        lv_obj_get_coords(obj, &roller_coords);

        lv_area_t rect_area;
        rect_area.x1 = roller_coords.x1;
        rect_area.x2 = roller_coords.x2;
        rect_area.y1 = roller_coords.y1;
        rect_area.y2 = roller_coords.y1 + (lv_obj_get_height(obj) - font_h - line_space) / 2;

        lv_draw_mask_fade_param_t * fade_mask_top = lv_mem_buf_get(sizeof(lv_draw_mask_fade_param_t));
        lv_draw_mask_fade_init(fade_mask_top, &rect_area, LV_OPA_TRANSP, rect_area.y1, LV_OPA_COVER, rect_area.y2);
        mask_top_id = lv_draw_mask_add(fade_mask_top, NULL);

        rect_area.y1 = rect_area.y2 + font_h + line_space - 1;
        rect_area.y2 = roller_coords.y2;

        lv_draw_mask_fade_param_t * fade_mask_bottom = lv_mem_buf_get(sizeof(lv_draw_mask_fade_param_t));
        lv_draw_mask_fade_init(fade_mask_bottom, &rect_area, LV_OPA_COVER, rect_area.y1, LV_OPA_TRANSP, rect_area.y2);
        mask_bottom_id = lv_draw_mask_add(fade_mask_bottom, NULL);

    }
    else if(code == LV_EVENT_DRAW_POST_END) {
        lv_draw_mask_fade_param_t * fade_mask_top = lv_draw_mask_remove_id(mask_top_id);
        lv_draw_mask_fade_param_t * fade_mask_bottom = lv_draw_mask_remove_id(mask_bottom_id);
        lv_draw_mask_free_param(fade_mask_top);
        lv_draw_mask_free_param(fade_mask_bottom);
        lv_mem_buf_release(fade_mask_top);
        lv_mem_buf_release(fade_mask_bottom);
        mask_top_id = -1;
        mask_bottom_id = -1;
    }
}

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
                // Just use the number for unnamed layers
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
    // lv_style_set_text_letter_space(&style, 2);
    lv_style_set_border_width(&style, 0);
    lv_style_set_pad_all(&style, 0);
    // lv_obj_add_style(lv_scr_act(), &style, 0);

    lv_obj_add_style(widget->obj, &style, 0);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, LV_PART_SELECTED);
    lv_obj_set_style_text_font(widget->obj, &FRAC_Regular_48, LV_PART_SELECTED);
    lv_obj_set_style_text_color(widget->obj, lv_color_hex(0xffffff), LV_PART_SELECTED);
    // lv_obj_set_style_text_line_space(widget->obj, 20, LV_PART_SELECTED);
    // lv_obj_set_style_text_line_space(widget->obj, 20, LV_PART_MAIN);
    lv_obj_set_style_text_font(widget->obj, &FRAC_Thin_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(widget->obj, lv_color_hex(0x909090), LV_PART_MAIN);
    // lv_obj_set_style_text_align(widget->obj, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_add_event_cb(widget->obj, mask_event_cb, LV_EVENT_ALL, NULL);

    // static lv_style_t style_roller;
    // lv_style_init(&style_roller);
    // lv_style_set_text_font(&style_roller, &SF_Compact_Text_Light_24);
    // lv_style_set_text_letter_space(&style_roller, -0.5);
    // lv_style_set_text_line_space(&style_roller, 20);
    // lv_style_set_text_color(&style_roller, lv_color_hex(0x6b6b6b));
    // lv_style_set_bg_color(&style_roller, lv_color_hex(0x050505));
    // lv_style_set_bg_opa(&style_roller, 255);

    // static lv_style_t style_roller_sel;
    // lv_style_init(&style_roller_sel);
    // lv_style_set_text_font(&style_roller_sel, &SF_Compact_Text_Semibold_28);
    // lv_style_set_text_letter_space(&style_roller_sel, -0.5);
    // lv_style_set_text_line_space(&style_roller_sel, 16);
    // lv_style_set_text_color(&style_roller_sel, lv_color_hex(0xffffff));
    // lv_style_set_bg_color(&style_roller_sel, lv_color_hex(0x1c1c1c));
    // lv_style_set_bg_opa(&style_roller_sel, 255);

    // lv_obj_set_style_text_align(widget->obj, LV_TEXT_ALIGN_CENTER, 0);
    // lv_obj_add_style(widget->obj, &style_roller, LV_PART_MAIN);
    // lv_obj_add_style(widget->obj, &style_roller_sel, LV_PART_SELECTED);
    // lv_obj_set_style_radius(widget->obj, 20, LV_PART_MAIN);

    lv_obj_set_style_anim_time(widget->obj, 100, 0);

    sys_slist_append(&widgets, &widget->node);

    widget_layer_roller_init();
    return 0;
}

lv_obj_t *zmk_widget_layer_roller_obj(struct zmk_widget_layer_roller *widget) {
    return widget->obj;
}