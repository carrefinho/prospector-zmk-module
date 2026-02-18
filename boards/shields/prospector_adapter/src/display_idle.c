#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/led.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(display_idle, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/position_state_changed.h>

#if IS_ENABLED(CONFIG_PM_DEVICE)
#include <zephyr/pm/device.h>
#endif

#include "prospector/display_power.h"

static const struct device *display_dev;
static const struct device *pwm_leds_dev;

#define DISP_BL DT_NODE_CHILD_IDX(DT_NODELABEL(disp_bl))

static struct k_work_delayable idle_work;
static atomic_t sleeping = ATOMIC_INIT(0);

static void prospector_display_blank_on(void) {
    if (!display_dev) {
        return;
    }

#if IS_ENABLED(CONFIG_PROSPECTOR_DISPLAY_SLEEP_USE_PM) && IS_ENABLED(CONFIG_PM_DEVICE)
    pm_device_action_run(display_dev, PM_DEVICE_ACTION_SUSPEND);
#else
    display_blanking_on(display_dev);
#endif
}

static void prospector_display_blank_off(void) {
    if (!display_dev) {
        return;
    }

#if IS_ENABLED(CONFIG_PROSPECTOR_DISPLAY_SLEEP_USE_PM) && IS_ENABLED(CONFIG_PM_DEVICE)
    pm_device_action_run(display_dev, PM_DEVICE_ACTION_RESUME);
#else
    display_blanking_off(display_dev);
#endif
}

void prospector_display_set_sleeping(bool s) {
    atomic_set(&sleeping, s ? 1 : 0);
}

bool prospector_display_is_sleeping(void) {
    return atomic_get(&sleeping) != 0;
}

static void display_go_to_sleep(void) {
    if (prospector_display_is_sleeping()) {
        return;
    }

    prospector_display_blank_on();

    if (pwm_leds_dev) {
        led_set_brightness(pwm_leds_dev, DISP_BL, 0);
    }

    prospector_display_set_sleeping(true);
}

static void display_wake_up(void) {
    if (!prospector_display_is_sleeping()) {
        return;
    }

    prospector_display_set_sleeping(false);

    prospector_display_blank_off();

#if !IS_ENABLED(CONFIG_PROSPECTOR_USE_AMBIENT_LIGHT_SENSOR)
    if (pwm_leds_dev) {
        led_set_brightness(pwm_leds_dev, DISP_BL, CONFIG_PROSPECTOR_FIXED_BRIGHTNESS);
    }
#else
    /* For ALS mode: force minimum visible brightness on wake (20%)
     * ALS thread will adjust to proper level shortly after */
    if (pwm_leds_dev) {
        led_set_brightness(pwm_leds_dev, DISP_BL, 20);
    }
    /* Give ALS thread time to react and adjust brightness */
    k_msleep(150);
#endif
}

static void idle_work_handler(struct k_work *work) {
    ARG_UNUSED(work);
    if (!IS_ENABLED(CONFIG_PROSPECTOR_DISPLAY_SLEEP_ENABLE)) {
        return;
    }
    display_go_to_sleep();
}

static int display_idle_init(const struct device *unused) {
    ARG_UNUSED(unused);

    display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display_dev)) {
        return 0;
    }

    pwm_leds_dev = DEVICE_DT_GET_ONE(pwm_leds);
    if (!device_is_ready(pwm_leds_dev)) {
        pwm_leds_dev = NULL;
    }

    k_work_init_delayable(&idle_work, idle_work_handler);

#if IS_ENABLED(CONFIG_PROSPECTOR_DISPLAY_SLEEP_ENABLE)
    k_work_schedule(&idle_work, K_MSEC(CONFIG_PROSPECTOR_DISPLAY_IDLE_TIMEOUT_MS));
#endif

    return 0;
}
SYS_INIT(display_idle_init, APPLICATION, 70);

static int on_any_key_event(const zmk_event_t *eh) {
    if (!IS_ENABLED(CONFIG_PROSPECTOR_DISPLAY_SLEEP_ENABLE)) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    const struct zmk_keycode_state_changed *kc = as_zmk_keycode_state_changed(eh);
    const struct zmk_position_state_changed *pos = as_zmk_position_state_changed(eh);

    bool pressed = false;
    if (kc) {
        pressed = kc->state;
    } else if (pos) {
        pressed = pos->state;
    }

    if (!pressed) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    display_wake_up();
#if IS_ENABLED(CONFIG_PROSPECTOR_DISPLAY_SLEEP_ENABLE)
    k_work_reschedule(&idle_work, K_MSEC(CONFIG_PROSPECTOR_DISPLAY_IDLE_TIMEOUT_MS));
#endif

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(prospector_display_idle, on_any_key_event);
ZMK_SUBSCRIPTION(prospector_display_idle, zmk_keycode_state_changed);
ZMK_SUBSCRIPTION(prospector_display_idle, zmk_position_state_changed);


