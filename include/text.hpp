#ifndef TEXT_HPP
#define TEXT_HPP

#include <string>

#include <dake/math/matrix.hpp>


enum HAlignment {
    ALIGN_LEFT,
    ALIGN_CENTER,
    ALIGN_RIGHT
};

enum VAlignment {
    ALIGN_TOP,
    ALIGN_MIDDLE,
    ALIGN_BOTTOM
};


void init_text(void);

void set_text_color(const dake::math::vec4 &color);
void draw_text(const dake::math::vec2 &pos, const dake::math::vec2 &size, const std::string &text, HAlignment halign = ALIGN_LEFT, VAlignment valign = ALIGN_MIDDLE);

#endif
