
#include "Color.h"


Color::Color(uint8_t _r, uint8_t _g, uint8_t _b) 
{    
    r = _r;
    g = _g;
    b = _b;
}

Color::Color(const Color& color) {
    r = color.r;
    g = color.g;
    b = color.b;    
}

const Color Color::interpolate(const Color& c1, const Color& c2, float factor) {

    float red   = c1.r / 255.0 + (c2.r - c1.r) / 255.0 * factor;
    float green = c1.g / 255.0 + (c2.g - c1.g) / 255.0 * factor;
    float blue  = c1.b / 255.0 + (c2.b - c1.b) / 255.0 * factor;

    red   = constrain(red, 0, 1);
    green = constrain(green, 0, 1);
    blue  = constrain(blue, 0, 1);

    Color retVal = Color((uint8_t)(red * 255.0), (uint8_t)(green * 255.0), (uint8_t)(blue * 255.0));
 //   retVal.dump(Serial);
    return retVal;
}

void Color::dump(Stream& stream) const {
    stream.print("(r,g,b) = (");
    stream.print(r); stream.print(","); stream.print(g); stream.print(","); stream.print(b);
    stream.print(")");
}
