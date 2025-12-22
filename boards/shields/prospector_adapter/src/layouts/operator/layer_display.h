#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

#define LAYER_DOT_COUNT 7

struct zmk_widget_layer_display {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_obj_t *dots[LAYER_DOT_COUNT];
};

int zmk_widget_layer_display_init(struct zmk_widget_layer_display *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_layer_display_obj(struct zmk_widget_layer_display *widget);
