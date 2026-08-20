#include "led_color.h"

led_rgb_t led_color_hsv_to_rgb(led_hsv_t hsv)
{
    led_rgb_t rgb = {0};

    if (hsv.saturation == 0) {
        rgb.red = hsv.value;
        rgb.green = hsv.value;
        rgb.blue = hsv.value;
        return rgb;
    }

    uint16_t hue = (uint16_t)(hsv.hue % 360);
    uint8_t region = (uint8_t)(hue / 60);
    uint16_t remainder = (uint16_t)((hue % 60) * 255 / 60);

    uint16_t value = hsv.value;
    uint16_t saturation = hsv.saturation;
    uint8_t p = (uint8_t)(value * (255 - saturation) / 255);
    uint8_t q = (uint8_t)(value * (255 - (saturation * remainder) / 255) / 255);
    uint8_t t = (uint8_t)(value * (255 - (saturation * (255 - remainder)) / 255) / 255);

    switch (region) {
    case 0:
        rgb.red = (uint8_t)value;
        rgb.green = t;
        rgb.blue = p;
        break;
    case 1:
        rgb.red = q;
        rgb.green = (uint8_t)value;
        rgb.blue = p;
        break;
    case 2:
        rgb.red = p;
        rgb.green = (uint8_t)value;
        rgb.blue = t;
        break;
    case 3:
        rgb.red = p;
        rgb.green = q;
        rgb.blue = (uint8_t)value;
        break;
    case 4:
        rgb.red = t;
        rgb.green = p;
        rgb.blue = (uint8_t)value;
        break;
    default:
        rgb.red = (uint8_t)value;
        rgb.green = p;
        rgb.blue = q;
        break;
    }

    return rgb;
}

uint8_t led_color_breath_scale(uint8_t phase)
{
    uint16_t mirrored = phase <= 127 ? phase : (uint16_t)(255 - phase);
    uint32_t x = (uint32_t)mirrored * 2;
    if (x > 255) {
        x = 255;
    }

    /*
     * 三次缓入缓出（smoothstep）：3t^2 - 2t^3，t = x / 255。
     * 整个式子放在一次除法里算完，分步取整会让曲线出现回落，
     * 肉眼看就是呼吸过程中的一次抖动。
     */
    uint32_t eased = (3u * x * x * 255u - 2u * x * x * x) / (255u * 255u);
    if (eased > 255) {
        eased = 255;
    }

    /* 轻度伽马整形，贴近人眼对亮度的感知。 */
    uint32_t perceptual = (eased * eased) / 255;
    if (perceptual > 255) {
        perceptual = 255;
    }

    return (uint8_t)perceptual;
}

led_hsv_t led_color_scale_value(led_hsv_t hsv, uint8_t scale)
{
    led_hsv_t scaled = hsv;
    scaled.value = (uint8_t)((uint16_t)hsv.value * scale / 255);
    return scaled;
}
