#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

#define WPM_BAR_COUNT 26
#define WPM_MAX 120

struct zmk_widget_wpm_meter {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_obj_t *bars[WPM_BAR_COUNT];
    lv_obj_t *wpm_label;
    lv_obj_t *layer_label;
};

int zmk_widget_wpm_meter_init(struct zmk_widget_wpm_meter *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_wpm_meter_obj(struct zmk_widget_wpm_meter *widget);
