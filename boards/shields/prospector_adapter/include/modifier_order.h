#pragma once

#include <stdbool.h>

enum modifier_type {
    MOD_TYPE_GUI = 0,
    MOD_TYPE_ALT,
    MOD_TYPE_CTRL,
    MOD_TYPE_SHIFT,
    MOD_TYPE_COUNT
};

enum modifier_type modifier_order_get(int position);
bool modifier_order_uses_symbols(void);
bool modifier_order_is_windows(void);
const char *modifier_order_get_symbol(int position);
const char *modifier_order_get_text(int position);
