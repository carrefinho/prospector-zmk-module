#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_layer_indicator {
    sys_snode_t node;
    lv_obj_t *obj;          // Label widget
    lv_obj_t *container;    // Main container
    lv_obj_t *wheel;        // Layer wheel graphic
};

int zmk_widget_layer_indicator_init(struct zmk_widget_layer_indicator *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_layer_indicator_obj(struct zmk_widget_layer_indicator *widget);