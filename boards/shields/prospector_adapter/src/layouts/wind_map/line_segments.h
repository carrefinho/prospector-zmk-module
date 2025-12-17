#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

// Grid configuration
#define LINE_SEGMENTS_GRID_COLS 8
#define LINE_SEGMENTS_GRID_ROWS 6
#define LINE_SEGMENTS_SPACING 34
#define LINE_SEGMENTS_GRID_OFFSET 18

// Widget dimensions
#define LINE_SEGMENTS_WIDTH 274
#define LINE_SEGMENTS_HEIGHT 206

struct zmk_widget_line_segments {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_obj_t *layer_label;
    lv_obj_t *battery_label;
};

int zmk_widget_line_segments_init(struct zmk_widget_line_segments *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_line_segments_obj(struct zmk_widget_line_segments *widget);

// Set the labels to exclude from line drawing
void zmk_widget_line_segments_set_labels(struct zmk_widget_line_segments *widget,
                                         lv_obj_t *layer_label,
                                         lv_obj_t *battery_label);

// Exclude/include a specific grid cell (for fixed-position elements like modifiers)
void zmk_widget_line_segments_set_cell_excluded(int col, int row, bool excluded);
