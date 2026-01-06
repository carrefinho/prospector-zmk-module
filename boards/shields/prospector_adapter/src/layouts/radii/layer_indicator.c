#include "layer_indicator.h"

#include <math.h>
#include <zmk/display.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/keymap.h>

#include <fonts.h>
#include "display_colors.h"

#define WHEEL_SIZE 48
#define WHEEL_CENTER (WHEEL_SIZE / 2)
#define WHEEL_INNER_RADIUS 12
#define WHEEL_OUTER_RADIUS 20

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct layer_indicator_state {
    uint8_t index;
};

// Animation callback for image rotation
static void set_image_rotation_anim(void *obj, int32_t v) {
    lv_image_set_rotation(obj, v);
}

static void layer_indicator_set_sel(struct zmk_widget_layer_indicator *widget, struct layer_indicator_state state) {
    // Calculate target angle: layer 0 at top (0°), going clockwise
    // lv_image rotation is in 0.1 degree units
    int32_t angle_per_layer = 3600 / ZMK_KEYMAP_LAYERS_LEN;
    int32_t target_angle = -(state.index * angle_per_layer);  // Negative to rotate wheel so current layer is at top

    int32_t current_angle = lv_image_get_rotation(widget->wheel);

    // Normalize for shortest path
    int32_t diff = target_angle - current_angle;
    while (diff > 1800) { target_angle -= 3600; diff = target_angle - current_angle; }
    while (diff < -1800) { target_angle += 3600; diff = target_angle - current_angle; }

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, widget->wheel);
    lv_anim_set_values(&a, current_angle, target_angle);
    lv_anim_set_time(&a, 150);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&a, set_image_rotation_anim);
    lv_anim_start(&a);

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

    // Create a canvas to draw the wheel
    static lv_obj_t *canvas;
    canvas = lv_canvas_create(widget->container);

    // Allocate buffer for canvas - use ARGB8888 for transparency support
    static uint8_t canvas_buf[LV_CANVAS_BUF_SIZE(WHEEL_SIZE, WHEEL_SIZE, 32, 1)];
    lv_canvas_set_buffer(canvas, canvas_buf, WHEEL_SIZE, WHEEL_SIZE, LV_COLOR_FORMAT_ARGB8888);
    lv_canvas_fill_bg(canvas, lv_color_hex(0x000000), LV_OPA_TRANSP);

    // Draw tick lines on canvas using layer API
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = lv_color_hex(DISPLAY_COLOR_LAYER_WHEEL);
    line_dsc.width = 4;
    line_dsc.opa = LV_OPA_COVER;
    line_dsc.round_start = 1;
    line_dsc.round_end = 1;

    for (int i = 0; i < ZMK_KEYMAP_LAYERS_LEN; i++) {
        float angle = ((float)i * 360.0f / ZMK_KEYMAP_LAYERS_LEN - 90.0f) * M_PI / 180.0f;

        line_dsc.p1.x = WHEEL_CENTER + (int)(WHEEL_INNER_RADIUS * cosf(angle));
        line_dsc.p1.y = WHEEL_CENTER + (int)(WHEEL_INNER_RADIUS * sinf(angle));
        line_dsc.p2.x = WHEEL_CENTER + (int)(WHEEL_OUTER_RADIUS * cosf(angle));
        line_dsc.p2.y = WHEEL_CENTER + (int)(WHEEL_OUTER_RADIUS * sinf(angle));

        lv_draw_line(&layer, &line_dsc);
    }

    lv_canvas_finish_layer(canvas, &layer);

    // Now create an image widget that uses the canvas as its source
    widget->wheel = lv_image_create(widget->container);
    lv_image_set_src(widget->wheel, lv_canvas_get_image(canvas));
    lv_image_set_pivot(widget->wheel, WHEEL_CENTER, WHEEL_CENTER);
    lv_image_set_rotation(widget->wheel, 0);

    // Hide the original canvas, we only need the image
    lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);

    // Layer name label
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
