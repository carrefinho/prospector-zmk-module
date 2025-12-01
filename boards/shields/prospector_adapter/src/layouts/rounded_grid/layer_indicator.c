#include "layer_indicator.h"

#include <math.h>
#include <zmk/display.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/keymap.h>

#include <fonts.h>
#include "display_colors.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct layer_indicator_state {
    uint8_t index;
};

static void set_angle_anim(void *obj, int32_t v) {
    lv_obj_set_style_transform_angle(obj, v, 0);
}

static void layer_indicator_set_sel(struct zmk_widget_layer_indicator *widget, struct layer_indicator_state state) {
    int16_t current_angle = lv_obj_get_style_transform_angle(widget->wheel, 0);
    int16_t target_angle = ((state.index * 3600) / ZMK_KEYMAP_LAYERS_LEN) - 900;

    lv_anim_t a_rot;
    lv_anim_init(&a_rot);
    lv_anim_set_var(&a_rot, widget->wheel);
    lv_anim_set_values(&a_rot, current_angle, target_angle);
    lv_anim_set_time(&a_rot, 150);
    lv_anim_set_path_cb(&a_rot, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&a_rot, set_angle_anim);
    lv_anim_start(&a_rot);

    const char *layer_name = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(state.index));
    char display_name[64];

    if (layer_name && *layer_name) {
        snprintf(display_name, sizeof(display_name), "%s", layer_name);
    } else {
        snprintf(display_name, sizeof(display_name), "%d", state.index);
    }

    lv_label_set_text(widget->obj, display_name);
}

static void layer_indicator_update_cb(struct layer_indicator_state state) {
    struct zmk_widget_layer_indicator *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        layer_indicator_set_sel(widget, state);
    }
}

static struct layer_indicator_state layer_indicator_get_state(const zmk_event_t *eh) {
    uint8_t index = zmk_keymap_highest_layer_active();
    return (struct layer_indicator_state){
        .index = index,
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_indicator, struct layer_indicator_state, layer_indicator_update_cb,
                            layer_indicator_get_state)
ZMK_SUBSCRIPTION(widget_layer_indicator, zmk_layer_state_changed);

int zmk_widget_layer_indicator_init(struct zmk_widget_layer_indicator *widget, lv_obj_t *parent) {
    widget->container = lv_obj_create(parent);
    lv_obj_set_size(widget->container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(widget->container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(widget->container, 0, 0);
    lv_obj_set_style_pad_all(widget->container, 0, 0);
    lv_obj_set_flex_flow(widget->container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(widget->container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(widget->container, 12, 0);

    widget->wheel = lv_canvas_create(widget->container);
    static lv_color_t wheel_buf[LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(48, 48)];
    lv_canvas_set_buffer(widget->wheel, wheel_buf, 48, 48, LV_IMG_CF_TRUE_COLOR_ALPHA);
    lv_obj_set_style_transform_pivot_x(widget->wheel, 24, 0);
    lv_obj_set_style_transform_pivot_y(widget->wheel, 24, 0);
    lv_obj_set_style_bg_opa(widget->wheel, LV_OPA_TRANSP, 0);

    lv_canvas_fill_bg(widget->wheel, lv_color_hex(0x000000), LV_OPA_TRANSP);
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = lv_color_hex(DISPLAY_COLOR_LAYER_WHEEL);
    line_dsc.width = 4;
    line_dsc.opa = LV_OPA_COVER;
    line_dsc.round_end = 1;
    line_dsc.round_start = 1;

    float angle_offset = 0.0f;
    for (int i = 0; i < ZMK_KEYMAP_LAYERS_LEN; i++) {
        float angle = (i * 360.0f / ZMK_KEYMAP_LAYERS_LEN) * 3.14159f / 180.0f + angle_offset;
        lv_point_t points[2];
        points[0].x = 24 + (int)(12 * cosf(angle));
        points[0].y = 24 + (int)(12 * sinf(angle));
        points[1].x = 24 + (int)(20 * cosf(angle));
        points[1].y = 24 + (int)(20 * sinf(angle));
        lv_canvas_draw_line(widget->wheel, points, 2, &line_dsc);
    }

    widget->obj = lv_label_create(widget->container);
    lv_obj_set_style_text_font(widget->obj, &PPF_NarrowThin_64, 0);
    lv_obj_set_style_text_color(widget->obj, lv_color_hex(DISPLAY_COLOR_LAYER_TEXT), 0);
    lv_obj_set_width(widget->obj, 148);
    lv_label_set_long_mode(widget->obj, LV_LABEL_LONG_WRAP);
    lv_label_set_text(widget->obj, "");

    sys_slist_append(&widgets, &widget->node);
    widget_layer_indicator_init();
    return 0;
}

lv_obj_t *zmk_widget_layer_indicator_obj(struct zmk_widget_layer_indicator *widget) {
    return widget->container;
}