#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_output {
    sys_snode_t node;
    lv_obj_t *container;
    lv_obj_t *links_label;
    lv_obj_t *profile_label;
};

int zmk_widget_output_init(struct zmk_widget_output *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_output_obj(struct zmk_widget_output *widget);
