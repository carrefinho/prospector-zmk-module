#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_battery_bar {
    sys_snode_t node;
    lv_obj_t *obj;
    bool initialized;
};

int zmk_widget_battery_bar_init(struct zmk_widget_battery_bar *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_battery_bar_obj(struct zmk_widget_battery_bar *widget);