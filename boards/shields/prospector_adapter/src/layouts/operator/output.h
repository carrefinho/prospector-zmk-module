#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>
#include <zmk/ble.h>

struct zmk_widget_output {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_obj_t *usb_btn;
    lv_obj_t *ble_btn;
    lv_obj_t *slots[ZMK_BLE_PROFILE_COUNT];
};

int zmk_widget_output_init(struct zmk_widget_output *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_output_obj(struct zmk_widget_output *widget);
