#include "line_segments.h"
#include "line_endpoints.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define _USE_MATH_DEFINES
#include <math.h>
#include <lvgl.h>
#include <zmk/event_manager.h>
#include <zmk/events/wpm_state_changed.h>
#include <zephyr/kernel.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif
#ifndef M_PI_4
#define M_PI_4 (M_PI / 4.0f)
#endif
#define TWO_PI (2.0f * M_PI)

#define GRID_COLS LINE_SEGMENTS_GRID_COLS
#define GRID_ROWS LINE_SEGMENTS_GRID_ROWS
#define SPACING LINE_SEGMENTS_SPACING
#define GRID_OFFSET LINE_SEGMENTS_GRID_OFFSET

#define ANIM_BASE_SPEED           0.004f   // Base animation time advancement rate
#define ANIM_WPM_SPEED_MULTIPLIER 0.072f   // How much WPM affects animation speed
#define ANIM_IDLE_WOBBLE_SPEED    0.05f    // Idle wobble time advancement rate

#define DECAY_RATE_FAST_RISE           0.15f   // Fast rise when activity increases
#define DECAY_RATE_SLOW_FALL_ACTIVE    0.005f  // Slow fall during active typing
#define DECAY_RATE_NORMAL_FALL_IDLE    0.02f   // Normal fall when idle
#define ANGLE_SMOOTHING_RATE           0.15f   // Angle interpolation smoothing factor

#define WOBBLE_FREQUENCY        0.5f    // Wobble oscillation frequency
#define WOBBLE_SPATIAL_COL      0.3f    // Column-based phase offset
#define WOBBLE_SPATIAL_ROW      0.2f    // Row-based phase offset
#define WOBBLE_AMPLITUDE        0.5f    // Wobble angle amplitude (radians)

#define LENGTH_BASE_IDLE        0.5f    // Base length when flow=0
#define LENGTH_BASE_ACTIVE      0.5f    // Additional length when flow=1
#define LENGTH_BREATH_FREQ      0.3f    // Breathing animation frequency
#define LENGTH_BREATH_COL       0.5f    // Column-based breath phase offset
#define LENGTH_BREATH_ROW       0.4f    // Row-based breath phase offset
#define LENGTH_BREATH_AMPLITUDE 0.15f   // Breath variation amplitude
#define LENGTH_MIN              0.5f    // Minimum line length (clamped)
#define LENGTH_MAX              1.0f    // Maximum line length (clamped)

#define OPACITY_BASE_IDLE       0.3f    // Base opacity when intensity=0
#define OPACITY_BASE_ACTIVE     0.5f    // Additional opacity when intensity=1
#define OPACITY_VARIATION_SCALE 0.3f    // Spatial variation influence on opacity
#define OPACITY_VARIATION_FREQ  0.2f    // Spatial variation time frequency
#define OPACITY_DECAY_FACTOR    0.15f   // Opacity reduction during decay
#define OPACITY_MIN             0.15f   // Minimum opacity (clamped)
#define OPACITY_MAX             0.7f    // Maximum opacity (clamped)

#define NOISE_ANGLE_INFLUENCE   1.5f    // Multiplier for noise effect on angle (×π)
#define NOISE_SPATIAL_X_SCALE   0.5f    // X-coordinate scale for opacity noise
#define NOISE_SPATIAL_Y_SCALE   0.5f    // Y-coordinate scale for opacity noise

#define TIMER_PERIOD_30HZ       33      // Timer period for 30Hz updates (ms)
#define TIMER_PERIOD_15HZ       66      // Timer period for 15Hz updates (ms)
#define TIMER_PERIOD_2HZ        500     // Timer period for 2Hz idle wobble (ms)

static const int16_t grid_cx[GRID_COLS] = {18, 52, 86, 120, 154, 188, 222, 256};
static const int16_t grid_cy[GRID_ROWS] = {18, 52, 86, 120, 154, 188};

static inline int angle_to_index(float angle) {
    int deg = (int)(angle * (180.0f / M_PI));
    deg = deg % 360;
    if (deg < 0) deg += 360;
    return deg / LINE_ENDPOINT_ANGLE_STEP;
}

#define LUT_SIZE 256
#define LUT_MASK (LUT_SIZE - 1)
static float sin_lut[LUT_SIZE];
static bool lut_initialized = false;

static void init_lut(void) {
    if (lut_initialized) return;
    for (int i = 0; i < LUT_SIZE; i++) {
        sin_lut[i] = sinf((float)i * TWO_PI / LUT_SIZE);
    }
    lut_initialized = true;
}

static inline float fast_sin(float x) {
    float normalized = x * (LUT_SIZE / TWO_PI);
    int index = ((int)normalized) & LUT_MASK;
    return sin_lut[index];
}

static inline float fast_cos(float x) {
    float normalized = (x + M_PI / 2.0f) * (LUT_SIZE / TWO_PI);
    int index = ((int)normalized) & LUT_MASK;
    return sin_lut[index];
}

static inline float clampf(float val, float min_val, float max_val) {
    if (val < min_val) return min_val;
    if (val > max_val) return max_val;
    return val;
}

typedef struct {
    float current_value;
    float at_stop_value;
    float target_value;
} decay_param_t;

static inline decay_param_t compute_decay_param(
    decay_param_t state,
    int current_wpm,
    uint32_t idle_ms,
    uint32_t decay_ms
) {
    decay_param_t result = state;

    // Compute target based on WPM or decay progress
    if (current_wpm > 0) {
        result.target_value = fminf(1.0f, current_wpm / 60.0f);
        result.at_stop_value = result.current_value;  // Capture smoothed value
    } else if (idle_ms < decay_ms) {
        float progress = (float)idle_ms / decay_ms;
        result.target_value = result.at_stop_value * (1.0f - progress);
    } else {
        result.target_value = 0.0f;
    }

    // Smooth toward target with adaptive rate
    float rate;
    if (current_wpm > 0 && result.target_value > result.current_value) {
        rate = DECAY_RATE_FAST_RISE;
    } else if (current_wpm > 0) {
        rate = DECAY_RATE_SLOW_FALL_ACTIVE;
    } else {
        rate = DECAY_RATE_NORMAL_FALL_IDLE;
    }

    result.current_value += (result.target_value - result.current_value) * rate;
    return result;
}

static float lines_time = 0.0f;
static float idle_wobble_time = 0.0f;
static float flow = 0.0f;
static float intensity = 0.0f;
static uint32_t last_keypress_time = 0;
static int current_wpm = 0;
static bool animation_started = false;

static float smoothed_angles[GRID_COLS * GRID_ROWS];
static uint8_t line_endpoint_idx[GRID_COLS * GRID_ROWS];
static float line_length_scale[GRID_COLS * GRID_ROWS];
static lv_opa_t line_opacity[GRID_COLS * GRID_ROWS];

static uint64_t label_excluded_cells = 0;     // From labels (computed from width)
static uint64_t modifier_excluded_cells = 0;  // From modifiers (set via API)

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
static lv_timer_t *animation_timer = NULL;
static uint32_t last_timer_period = 33;

static float lines_noise(float x, float y, float t) {
    float n1 = fast_sin(x * 0.007f + t * 0.15f) * fast_cos(y * 0.008f - t * 0.12f);
    float n2 = fast_sin(y * 0.006f + t * 0.1f + x * 0.005f);
    return (n1 + n2 * 0.7f) / 1.7f;
}

static int wpm_event_handler(const zmk_event_t *eh) {
    const struct zmk_wpm_state_changed *ev = as_zmk_wpm_state_changed(eh);
    if (ev) {
        uint32_t now = k_uptime_get_32();
        int new_wpm = ev->state;

        if (now < 10000) {
            return ZMK_EV_EVENT_BUBBLE;
        }

        if (new_wpm > 300) {
            return ZMK_EV_EVENT_BUBBLE;
        }

        current_wpm = new_wpm;
        if (current_wpm > 0) {
            last_keypress_time = now;
            animation_started = true;
        }
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(widget_line_segments, wpm_event_handler);
ZMK_SUBSCRIPTION(widget_line_segments, zmk_wpm_state_changed);

// Calculate how many columns a label width covers (starting from column 0)
static int width_to_columns(int width) {
    // Labels start at x≈6
    // Exclude column if label overlaps grid box (center ± SPACING/2)
    int label_right = 6 + width;
    for (int col = 0; col < GRID_COLS; col++) {
        int cell_left = grid_cx[col] - SPACING / 2;
        if (label_right <= cell_left) {
            return col;
        }
    }
    return GRID_COLS;
}

static void update_label_excluded_cells(void) {
    label_excluded_cells = 0;

    struct zmk_widget_line_segments *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        // Layer label excludes cells in row 0
        if (widget->layer_label) {
            int width = lv_obj_get_width(widget->layer_label);
            int cols = width_to_columns(width);
            for (int col = 0; col < cols; col++) {
                label_excluded_cells |= (1ULL << col);
            }
        }

        // Battery label excludes cells in row 5
        if (widget->battery_label) {
            int width = lv_obj_get_width(widget->battery_label);
            int cols = width_to_columns(width);
            for (int col = 0; col < cols; col++) {
                label_excluded_cells |= (1ULL << (5 * GRID_COLS + col));
            }
        }
        break;
    }
}

static void label_size_changed_cb(lv_event_t *e) {
    update_label_excluded_cells();

    // Invalidate widget to redraw with new exclusions
    struct zmk_widget_line_segments *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        lv_obj_invalidate(widget->obj);
    }
}

static uint32_t perf_update_us = 0;
static uint32_t perf_draw_us = 0;
static uint32_t perf_frame_count = 0;

static void lines_update(void) {
    uint32_t start = k_cycle_get_32();

    const float delta_time = 1.0f / 30.0f;
    uint32_t now = k_uptime_get_32();
    uint32_t idle_ms = now - last_keypress_time;

    const uint32_t INTENSITY_DECAY_MS = CONFIG_PROSPECTOR_ANIMATION_INTENSITY_DECAY_SEC * 1000;
    const uint32_t FLOW_DECAY_MS = CONFIG_PROSPECTOR_ANIMATION_FLOW_DECAY_SEC * 1000;

    static float intensity_at_stop = 0.0f;
    static float flow_at_stop = 0.0f;

    if (current_wpm > 0 || idle_ms < INTENSITY_DECAY_MS / 10) {
        float speed = ANIM_BASE_SPEED + (current_wpm / (float)CONFIG_PROSPECTOR_ANIMATION_WPM_REFERENCE) * ANIM_WPM_SPEED_MULTIPLIER;
        lines_time += speed * delta_time * 60.0f;
    }

    idle_wobble_time += ANIM_IDLE_WOBBLE_SPEED * delta_time * 60.0f;

    decay_param_t flow_state = {flow, flow_at_stop, 0};
    flow_state = compute_decay_param(flow_state, current_wpm, idle_ms, FLOW_DECAY_MS);
    flow = flow_state.current_value;
    flow_at_stop = flow_state.at_stop_value;

    decay_param_t intensity_state = {intensity, intensity_at_stop, 0};
    intensity_state = compute_decay_param(intensity_state, current_wpm, idle_ms, INTENSITY_DECAY_MS);
    intensity = intensity_state.current_value;
    intensity_at_stop = intensity_state.at_stop_value;

    float time = lines_time;

    for (int row = 0; row < GRID_ROWS; row++) {
            for (int col = 0; col < GRID_COLS; col++) {
                int line_idx = row * GRID_COLS + col;
                int cx = grid_cx[col];
                int cy = grid_cy[row];

                float noise_val = lines_noise((float)cx, (float)cy, time);
                float base_angle = -M_PI_4;
                float active_offset = noise_val * M_PI * NOISE_ANGLE_INFLUENCE;
                float target_angle = base_angle + active_offset * flow;

                smoothed_angles[line_idx] += (target_angle - smoothed_angles[line_idx]) * ANGLE_SMOOTHING_RATE;

                float idle_wobble = fast_sin(idle_wobble_time * WOBBLE_FREQUENCY +
                                            col * WOBBLE_SPATIAL_COL +
                                            row * WOBBLE_SPATIAL_ROW) * WOBBLE_AMPLITUDE;
                float final_angle = smoothed_angles[line_idx] + idle_wobble;

                line_endpoint_idx[line_idx] = (uint8_t)angle_to_index(final_angle);

                float base_scale = LENGTH_BASE_IDLE + flow * LENGTH_BASE_ACTIVE;
                float breath = fast_sin(time * LENGTH_BREATH_FREQ +
                                       col * LENGTH_BREATH_COL +
                                       row * LENGTH_BREATH_ROW) * LENGTH_BREATH_AMPLITUDE;
                line_length_scale[line_idx] = clampf(base_scale + breath * flow, LENGTH_MIN, LENGTH_MAX);

                float base_opa = OPACITY_BASE_IDLE + intensity * OPACITY_BASE_ACTIVE;
                float spatial_var = lines_noise((float)cx * NOISE_SPATIAL_X_SCALE,
                                               (float)cy * NOISE_SPATIAL_Y_SCALE,
                                               time * OPACITY_VARIATION_FREQ);
                spatial_var = (spatial_var + 1.0f) * 0.5f;
                float opa_variation = spatial_var * OPACITY_VARIATION_SCALE * intensity;
                float final_opa = base_opa + opa_variation - OPACITY_DECAY_FACTOR * intensity;
                line_opacity[line_idx] = (lv_opa_t)(clampf(final_opa, OPACITY_MIN, OPACITY_MAX) * 255.0f);
            }
    }

    uint32_t elapsed = k_cycle_get_32() - start;
    perf_update_us = k_cyc_to_us_floor32(elapsed);
}

static void draw_cb(lv_event_t *e) {
    uint32_t start = k_cycle_get_32();
    lv_obj_t *obj = lv_event_get_target(e);
    lv_layer_t *layer = lv_event_get_layer(e);

    lv_area_t obj_coords;
    lv_obj_get_coords(obj, &obj_coords);

    int32_t obj_x1 = obj_coords.x1;
    int32_t obj_y1 = obj_coords.y1;

    uint64_t excluded = label_excluded_cells | modifier_excluded_cells;

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = lv_color_white();
    line_dsc.width = 2;
    line_dsc.round_start = 0;
    line_dsc.round_end = 0;

    for (int row = 0; row < GRID_ROWS; row++) {
        for (int col = 0; col < GRID_COLS; col++) {
            int line_idx = row * GRID_COLS + col;

            if (excluded & (1ULL << line_idx)) {
                continue;
            }

            int cx = grid_cx[col];
            int cy = grid_cy[row];

            line_dsc.opa = line_opacity[line_idx];

            uint8_t idx = line_endpoint_idx[line_idx];
            int8_t dx_base = line_endpoints[idx][0];
            int8_t dy_base = line_endpoints[idx][1];
            float scale = line_length_scale[line_idx];
            int16_t dx = (int16_t)(dx_base * scale);
            int16_t dy = (int16_t)(dy_base * scale);

            line_dsc.p1.x = obj_x1 + cx - dx;
            line_dsc.p1.y = obj_y1 + cy - dy;
            line_dsc.p2.x = obj_x1 + cx + dx;
            line_dsc.p2.y = obj_y1 + cy + dy;
            lv_draw_line(layer, &line_dsc);
        }
    }

    uint32_t elapsed = k_cycle_get_32() - start;
    perf_draw_us = k_cyc_to_us_floor32(elapsed);

    perf_frame_count++;
    if (perf_frame_count >= 30) {
        LOG_INF("perf: update=%uus draw=%uus", perf_update_us, perf_draw_us);
        perf_frame_count = 0;
    }
}

static void timer_cb(lv_timer_t *timer) {
    uint32_t now = k_uptime_get_32();
    uint32_t idle_ms = now - last_keypress_time;

    uint32_t target_period;
    if (current_wpm > 0) {
        target_period = TIMER_PERIOD_30HZ;
    } else if (animation_started && idle_ms < CONFIG_PROSPECTOR_ANIMATION_FLOW_DECAY_SEC * 1000) {
        target_period = TIMER_PERIOD_15HZ;
    } else {
        target_period = TIMER_PERIOD_2HZ;
    }

    if (target_period != last_timer_period) {
        lv_timer_set_period(animation_timer, target_period);
        last_timer_period = target_period;
    }

    lines_update();

    struct zmk_widget_line_segments *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        lv_obj_invalidate(widget->obj);
    }
}

int zmk_widget_line_segments_init(struct zmk_widget_line_segments *widget, lv_obj_t *parent) {
    init_lut();

    for (int row = 0; row < GRID_ROWS; row++) {
        for (int col = 0; col < GRID_COLS; col++) {
            int line_idx = row * GRID_COLS + col;
            smoothed_angles[line_idx] = -M_PI_4;
        }
    }

    widget->obj = lv_obj_create(parent);
    widget->layer_label = NULL;
    widget->battery_label = NULL;

    lv_obj_remove_style_all(widget->obj);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, 0);

    lv_obj_add_event_cb(widget->obj, draw_cb, LV_EVENT_DRAW_MAIN, widget);

    sys_slist_append(&widgets, &widget->node);

    if (animation_timer == NULL) {
        animation_timer = lv_timer_create(timer_cb, 33, NULL);
    }

    return 0;
}

lv_obj_t *zmk_widget_line_segments_obj(struct zmk_widget_line_segments *widget) {
    return widget->obj;
}

void zmk_widget_line_segments_set_labels(struct zmk_widget_line_segments *widget,
                                         lv_obj_t *layer_label,
                                         lv_obj_t *battery_label) {
    widget->layer_label = layer_label;
    widget->battery_label = battery_label;

    if (layer_label) {
        lv_obj_add_event_cb(layer_label, label_size_changed_cb, LV_EVENT_SIZE_CHANGED, NULL);
    }
    if (battery_label) {
        lv_obj_add_event_cb(battery_label, label_size_changed_cb, LV_EVENT_SIZE_CHANGED, NULL);
    }

    update_label_excluded_cells();
}

void zmk_widget_line_segments_set_cell_excluded(int col, int row, bool excluded) {
    if (col < 0 || col >= GRID_COLS || row < 0 || row >= GRID_ROWS) {
        return;
    }
    uint64_t mask = 1ULL << (row * GRID_COLS + col);
    if (excluded) {
        modifier_excluded_cells |= mask;
    } else {
        modifier_excluded_cells &= ~mask;
    }
}
