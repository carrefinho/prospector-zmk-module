#pragma once

#include <zephyr/devicetree.h>

#if DT_HAS_CHOSEN(zmk_prospector_theme)
#define THEME_NODE DT_CHOSEN(zmk_prospector_theme)
#else
#define THEME_NODE DT_NODELABEL(prospector_blue_theme)
#endif

#define DISPLAY_COLOR_LEFT_PANEL_BG DT_PROP(THEME_NODE, left_panel_bg)
#define DISPLAY_COLOR_MOD_PANEL_BG DT_PROP(THEME_NODE, mod_panel_bg)
#define DISPLAY_COLOR_BATTERY_PANEL_BG DT_PROP(THEME_NODE, battery_panel_bg)
#define DISPLAY_COLOR_ARC_BG DT_PROP(THEME_NODE, arc_bg)
#define DISPLAY_COLOR_ARC_INDICATOR DT_PROP(THEME_NODE, arc_indicator)
#define DISPLAY_COLOR_LAYER_WHEEL DT_PROP(THEME_NODE, layer_wheel_color)
#define DISPLAY_COLOR_LAYER_TEXT DT_PROP(THEME_NODE, layer_text_color)
#define DISPLAY_COLOR_MOD_ACTIVE DT_PROP(THEME_NODE, mod_active_color)
#define DISPLAY_COLOR_MOD_INACTIVE DT_PROP(THEME_NODE, mod_inactive_color)
