#include "layer_label.h"

#include <zmk/display.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/keymap.h>

#include <fonts.h>

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct layer_label_state {
    uint8_t index;
};

static void layer_label_set_text(lv_obj_t *label, struct layer_label_state state) {
    const char *layer_name = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(state.index));
    if (layer_name && *layer_name) {
        lv_label_set_text(label, layer_name);
    } else {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", state.index);
        lv_label_set_text(label, buf);
    }
}

static void layer_label_update_cb(struct layer_label_state state) {
    struct zmk_widget_layer_label *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        layer_label_set_text(widget->obj, state);
    }
}

static struct layer_label_state layer_label_get_state(const zmk_event_t *eh) {
    uint8_t index = zmk_keymap_highest_layer_active();
    return (struct layer_label_state){
        .index = index,
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_label, struct layer_label_state, layer_label_update_cb,
                            layer_label_get_state)
ZMK_SUBSCRIPTION(widget_layer_label, zmk_layer_state_changed);

int zmk_widget_layer_label_init(struct zmk_widget_layer_label *widget, lv_obj_t *parent) {
    widget->obj = lv_label_create(parent);

    lv_obj_set_style_text_font(widget->obj, &FR_Regular_36, 0);
    lv_obj_set_style_text_color(widget->obj, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, 0);

    sys_slist_append(&widgets, &widget->node);

    widget_layer_label_init();
    return 0;
}

lv_obj_t *zmk_widget_layer_label_obj(struct zmk_widget_layer_label *widget) {
    return widget->obj;
}
