#include "led_color.h"
#include "test_support.h"

static void test_zero_saturation_is_grey(void)
{
    led_rgb_t rgb = led_color_hsv_to_rgb((led_hsv_t){.hue = 200, .saturation = 0, .value = 120});
    CHECK_INT(rgb.red, 120);
    CHECK_INT(rgb.green, 120);
    CHECK_INT(rgb.blue, 120);
}

static void test_primary_hues(void)
{
    led_rgb_t red = led_color_hsv_to_rgb((led_hsv_t){.hue = 0, .saturation = 255, .value = 255});
    CHECK_INT(red.red, 255);
    CHECK_INT(red.green, 0);
    CHECK_INT(red.blue, 0);

    led_rgb_t green = led_color_hsv_to_rgb((led_hsv_t){.hue = 120, .saturation = 255, .value = 255});
    CHECK_INT(green.red, 0);
    CHECK_INT(green.green, 255);
    CHECK_INT(green.blue, 0);

    led_rgb_t blue = led_color_hsv_to_rgb((led_hsv_t){.hue = 240, .saturation = 255, .value = 255});
    CHECK_INT(blue.red, 0);
    CHECK_INT(blue.green, 0);
    CHECK_INT(blue.blue, 255);
}

static void test_hue_wraps_around(void)
{
    led_rgb_t a = led_color_hsv_to_rgb((led_hsv_t){.hue = 30, .saturation = 200, .value = 200});
    led_rgb_t b = led_color_hsv_to_rgb((led_hsv_t){.hue = 390, .saturation = 200, .value = 200});
    CHECK_INT(a.red, b.red);
    CHECK_INT(a.green, b.green);
    CHECK_INT(a.blue, b.blue);
}

static void test_value_zero_is_dark(void)
{
    led_rgb_t rgb = led_color_hsv_to_rgb((led_hsv_t){.hue = 90, .saturation = 255, .value = 0});
    CHECK_INT(rgb.red, 0);
    CHECK_INT(rgb.green, 0);
    CHECK_INT(rgb.blue, 0);
}

static void test_breath_curve_is_symmetric_and_bounded(void)
{
    CHECK_INT(led_color_breath_scale(0), 0);
    CHECK_INT(led_color_breath_scale(127), led_color_breath_scale(128));

    uint8_t peak = led_color_breath_scale(127);
    CHECK(peak > 240);

    /* 曲线关于 127/128 折返，因此 f(p) 必须等于 f(255 - p)。 */
    for (int phase = 0; phase <= 255; ++phase) {
        CHECK_INT(led_color_breath_scale((uint8_t)phase), led_color_breath_scale((uint8_t)(255 - phase)));
    }

    /* 上升段必须单调不减，否则呼吸会出现抖动。 */
    for (int phase = 1; phase <= 127; ++phase) {
        CHECK(led_color_breath_scale((uint8_t)phase) >= led_color_breath_scale((uint8_t)(phase - 1)));
    }
}

static void test_scale_value(void)
{
    led_hsv_t scaled = led_color_scale_value((led_hsv_t){.hue = 10, .saturation = 20, .value = 200}, 128);
    CHECK_INT(scaled.hue, 10);
    CHECK_INT(scaled.saturation, 20);
    CHECK_INT(scaled.value, 100);

    led_hsv_t full = led_color_scale_value((led_hsv_t){.hue = 10, .saturation = 20, .value = 200}, 255);
    CHECK_INT(full.value, 200);
}

int main(void)
{
    RUN(test_zero_saturation_is_grey);
    RUN(test_primary_hues);
    RUN(test_hue_wraps_around);
    RUN(test_value_zero_is_dark);
    RUN(test_breath_curve_is_symmetric_and_bounded);
    RUN(test_scale_value);
    TEST_MAIN_END();
}
