

#ifndef COLOR_H
#define COLOR_H

#if ARDUINO >= 100
 #include <Arduino.h>
#else
 #include <WProgram.h>
#endif

class Color {

    public:
        Color() {r = 0; g = 0; b = 0;}
        Color(const Color& color);
        Color(uint8_t r, uint8_t g, uint8_t b);


        Color& operator =(const Color& c) {
            r = c.r;
            g = c.g;
            b = c.b;
        }

        uint8_t getRed()   const { return r; }
        uint8_t getGreen() const { return g; }
        uint8_t getBlue()  const { return b; }

        uint32_t getPackedColor() const { return ((uint32_t)r << 16) | ((uint32_t)g <<  8) | b; };
        uint32_t getPackedColorGRB() const { return ((uint32_t)g << 16) | ((uint32_t)r <<  8) | b; };

        void dump(Stream& stream) const;

        static const Color interpolate(const Color& c1, const Color& c2, float factor);
         
    private:

        uint8_t r;
        uint8_t g;
        uint8_t b;
};

#endif
