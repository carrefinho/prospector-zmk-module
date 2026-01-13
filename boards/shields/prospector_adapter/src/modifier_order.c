#include <modifier_order.h>
#include <symbols.h>
#include <string.h>
#include <ctype.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(modifier_order, CONFIG_ZMK_LOG_LEVEL);

#define MODIFIER_ORDER_DEFAULT "GACS"

static enum modifier_type order[MOD_TYPE_COUNT] = {
    MOD_TYPE_GUI, MOD_TYPE_ALT, MOD_TYPE_CTRL, MOD_TYPE_SHIFT
};

static const char *symbols_mac[MOD_TYPE_COUNT] = {
    SYMBOL_COMMAND, SYMBOL_OPTION, SYMBOL_CONTROL, SYMBOL_SHIFT
};

#if defined(CONFIG_PROSPECTOR_MODIFIER_OS_MAC)
static const char *texts[MOD_TYPE_COUNT] = {
    "CMD", "OPT", "CTRL", "SHFT"
};
#elif defined(CONFIG_PROSPECTOR_MODIFIER_OS_WINDOWS)
static const char *texts[MOD_TYPE_COUNT] = {
    "WIN", "ALT", "CTRL", "SHFT"
};
#else
static const char *texts[MOD_TYPE_COUNT] = {
    "GUI", "ALT", "CTRL", "SHFT"
};
#endif

static enum modifier_type char_to_modifier(char ch) {
    switch (toupper((unsigned char)ch)) {
        case 'G': return MOD_TYPE_GUI;
        case 'A': return MOD_TYPE_ALT;
        case 'C': return MOD_TYPE_CTRL;
        case 'S': return MOD_TYPE_SHIFT;
        default:  return MOD_TYPE_COUNT;
    }
}

static bool parse_modifier_order(const char *order_str) {
    if (!order_str || strlen(order_str) != MOD_TYPE_COUNT) {
        return false;
    }

    bool seen[MOD_TYPE_COUNT] = {false, false, false, false};
    enum modifier_type temp[MOD_TYPE_COUNT];

    for (int i = 0; i < MOD_TYPE_COUNT; i++) {
        enum modifier_type type = char_to_modifier(order_str[i]);
        if (type >= MOD_TYPE_COUNT || seen[type]) {
            return false;
        }
        seen[type] = true;
        temp[i] = type;
    }

    memcpy(order, temp, sizeof(order));
    return true;
}

static int modifier_order_init(void) {
#ifdef CONFIG_PROSPECTOR_MODIFIER_ORDER
    const char *config_str = CONFIG_PROSPECTOR_MODIFIER_ORDER;
    if (!parse_modifier_order(config_str)) {
        LOG_WRN("Invalid modifier order '%s', using default '%s'",
                config_str, MODIFIER_ORDER_DEFAULT);
    }
#endif
    return 0;
}

SYS_INIT(modifier_order_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

enum modifier_type modifier_order_get(int position) {
    if (position < 0 || position >= MOD_TYPE_COUNT) {
        LOG_WRN("Invalid modifier position %d", position);
        return MOD_TYPE_GUI;
    }
    return order[position];
}

bool modifier_order_uses_symbols(void) {
    return IS_ENABLED(CONFIG_PROSPECTOR_MODIFIER_OS_MAC);
}

bool modifier_order_is_windows(void) {
    return IS_ENABLED(CONFIG_PROSPECTOR_MODIFIER_OS_WINDOWS);
}

const char *modifier_order_get_symbol(int position) {
    return symbols_mac[modifier_order_get(position)];
}

const char *modifier_order_get_text(int position) {
    return texts[modifier_order_get(position)];
}
