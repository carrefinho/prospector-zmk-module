#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/led.h>
#include <zephyr/sys/printk.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(als, 4);

#include "prospector/display_power.h"

static const struct device *pwm_leds_dev = DEVICE_DT_GET_ONE(pwm_leds);
#define DISP_BL DT_NODE_CHILD_IDX(DT_NODELABEL(disp_bl))

#ifdef CONFIG_PROSPECTOR_USE_AMBIENT_LIGHT_SENSOR

static uint8_t current_brightness = 100;

#define SENSOR_MIN      0       // Minimum sensor reading
#define SENSOR_MAX      100   // Maximum sensor reading
#define PWM_MIN         15      // Minimum PWM duty cycle (%) - keep display visible even in dark
#define PWM_MAX         100     // Maximum PWM duty cycle (%)

#define FADE_STEP                        1
#define FADE_SLEEP_BRIGHTEN_MS           3
#define FADE_SLEEP_DARKEN_MS             10
#define FADE_THRESHOLD                   10

#define NORMAL_SAMPLE_SLEEP_MS           100

#define BURST_SAMPLE_SLEEP_MS            30
#define BURST_SAMPLE_TIMEOUT             10
#define BURST_SAMPLE_CONSECUTIVE         3

uint8_t map_light_to_pwm(int32_t sensor_reading) {
    // Handle invalid/error readings
    if (sensor_reading < SENSOR_MIN) {
        return PWM_MIN;  // Default to minimum brightness on error
    }

    // Clamp to maximum
    if (sensor_reading > SENSOR_MAX) {
        sensor_reading = SENSOR_MAX;
    }

    // Linear mapping
    uint8_t pwm_value = (uint8_t)(
        PWM_MIN + ((PWM_MAX - PWM_MIN) *
        (sensor_reading - SENSOR_MIN)) / (SENSOR_MAX - SENSOR_MIN)
    );

    return pwm_value;
}

uint8_t bl_fade(uint8_t source, uint8_t target) {
    bool increasing = target > source;

    while ((increasing && current_brightness < target) ||
           (!increasing && current_brightness > target)) {

        if (led_set_brightness(pwm_leds_dev, DISP_BL, current_brightness)) {
            LOG_ERR("Failed to set brightness");
        }

        current_brightness += increasing ? FADE_STEP : -FADE_STEP;

        // Ensure we don't overshoot bounds
        if (current_brightness > 100) {
            current_brightness = 100;
        } else if (current_brightness < 0) {
            current_brightness = 0;
        }

        k_msleep(increasing ? FADE_SLEEP_BRIGHTEN_MS : FADE_SLEEP_DARKEN_MS);
    }

    return 0;
}

extern void als_thread(void *d0, void *d1, void *d2) {
    ARG_UNUSED(d0);
    ARG_UNUSED(d1);
    ARG_UNUSED(d2);

    const struct device *dev;
    struct sensor_value intensity;
    uint8_t mapped_brightness;

    dev = DEVICE_DT_GET_ONE(avago_apds9960);
    if (!device_is_ready(dev)) {
        printk("sensor: device not ready.\n");
    }

    // led_set_brightness(pwm_leds_dev, DISP_BL, 100);

    while (1) {

        if (prospector_display_is_sleeping()) {
            if (current_brightness != 0) {
                led_set_brightness(pwm_leds_dev, DISP_BL, 0);
                current_brightness = 0;
            }
            k_msleep(NORMAL_SAMPLE_SLEEP_MS);
            continue;
        }

        k_msleep(NORMAL_SAMPLE_SLEEP_MS);


        if (sensor_sample_fetch(dev)) {
            LOG_ERR("sensor_sample fetch failed\n");
        }

        if (sensor_channel_get(dev, SENSOR_CHAN_LIGHT, &intensity)) {
            LOG_ERR("Cannot read ALS data.\n");
        }

        // LOG_INF("ambient light intensity %d", intensity.val1);

        mapped_brightness = map_light_to_pwm(intensity.val1);
        // LOG_INF("NORMAL: mapped PWM duty cycle %d\n", mapped_brightness);

        if (abs(mapped_brightness - current_brightness) > FADE_THRESHOLD) {
            uint8_t integrator = 0;

            for (int i = 0; i < BURST_SAMPLE_TIMEOUT; i++) {
                k_msleep(BURST_SAMPLE_SLEEP_MS);

                if (sensor_sample_fetch(dev)) {
                    LOG_ERR("sensor_sample fetch failed\n");
                }
                if (sensor_channel_get(dev, SENSOR_CHAN_LIGHT, &intensity)) {
                    LOG_ERR("Cannot read ALS data.\n");
                }

                mapped_brightness = map_light_to_pwm(intensity.val1);
                // LOG_INF("BURST: mapped PWM duty cycle %d\n", mapped_brightness);

                if (abs(mapped_brightness - current_brightness) > FADE_THRESHOLD) {
                    integrator++;
                    // printk("integrator at: %d", integrator);
                    if (integrator >= BURST_SAMPLE_CONSECUTIVE) {
                        bl_fade(current_brightness, mapped_brightness);
                        current_brightness = mapped_brightness;
                        // LOG_INF("SETTING NEW BRIGHTNESS: %d", mapped_brightness);
                        break;
                    }
                }
            }
        }
        // led_set_brightness(pwm_leds_dev, DISP_BL, map_light_to_pwm(intensity.val1));
    }
}

K_THREAD_DEFINE(als_tid, 1024, als_thread, NULL, NULL, NULL, K_LOWEST_APPLICATION_THREAD_PRIO, 0,
                0);

#else

static int init_fixed_brightness(void) {
    led_set_brightness(pwm_leds_dev, DISP_BL, CONFIG_PROSPECTOR_FIXED_BRIGHTNESS);

    return 0;
}

SYS_INIT(init_fixed_brightness, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#endif