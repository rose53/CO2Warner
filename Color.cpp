
#include "Color.h"

const float Color::intToFloat = 1.0 / 255;

Color::Color(uint8_t r, uint8_t g, uint8_t b): 
    r(r),
    g(g),
    b(b)
{    
}


const Color& Color::interpolate(const Color& c1, const Color& c2, float factor) {

    float red   = c1.r * intToFloat + (c2.r - c1.r) * intToFloat * factor;
    float green = c1.g * intToFloat + (c2.g - c1.g) * intToFloat * factor;
    float blue  = c1.b * intToFloat + (c2.b - c1.b) * intToFloat * factor;

    red   = constrain(red, 0, 1);
    green = constrain(green, 0, 1);
    blue  = constrain(blue, 0, 1);

    return Color((uint8_t)(red / intToFloat), (uint8_t)(green / intToFloat), (uint8_t)(blue / intToFloat));
}
