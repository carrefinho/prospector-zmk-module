#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

// Grid configuration - shared with modifier_indicator for alignment
#define LINE_SEGMENTS_GRID_COLS 8
#define LINE_SEGMENTS_GRID_ROWS 6
#define LINE_SEGMENTS_SPACING 34
#define LINE_SEGMENTS_GRID_OFFSET 18

// Widget dimensions (for positioning calculations)
#define LINE_SEGMENTS_WIDTH 274
#define LINE_SEGMENTS_HEIGHT 206

// Object-based exclusions (for non-grid-aligned elements like labels)
#define LINE_SEGMENTS_MAX_EXCLUSIONS 4

struct zmk_widget_line_segments {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_obj_t *exclusion_objs[LINE_SEGMENTS_MAX_EXCLUSIONS];
    uint8_t exclusion_count;
};

int zmk_widget_line_segments_init(struct zmk_widget_line_segments *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_line_segments_obj(struct zmk_widget_line_segments *widget);

// Object-based exclusion for non-grid-aligned elements (labels, etc.)
void zmk_widget_line_segments_add_exclusion(struct zmk_widget_line_segments *widget, lv_obj_t *obj);

// Grid cell exclusion - O(1) bitmask check, for grid-aligned elements
void zmk_widget_line_segments_set_cell_excluded(int col, int row, bool excluded);
