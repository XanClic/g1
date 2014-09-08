#include <dake/dake.hpp>

#include <stdexcept>
#include <string>
#include <vector>

#include "localize.hpp"
#include "text.hpp"


using namespace dake;
using namespace dake::math;


static gl::vertex_array *char_va;
static gl::program *char_prg;
static gl::texture *latin_font, *mado_font, *tengwar_font;


struct CharVAElement {
    vec2 pos;
    unsigned chr;
};


void init_text(void)
{
    CharVAElement *cvad = new CharVAElement[320];
    for (int i = 0; i < 320; i++) {
        float ysign = (i % 10 >= 5) ? -1.f : 1.f;
        int m = i % 5;

        if (m == 4) {
            m = 3;
        }

        ysign = (m < 2) ? -ysign : ysign;

        cvad[i].pos = vec2(i / 5 + (m % 2) * .9999f, ysign * .5f);
    }

    char_va = new gl::vertex_array;

    char_va->attrib(0)->format(2);
    char_va->attrib(0)->data(cvad, 320 * sizeof(CharVAElement), GL_DYNAMIC_DRAW, false);
    char_va->attrib(0)->load(sizeof(CharVAElement), offsetof(CharVAElement, pos));

    char_va->attrib(1)->format(1, GL_UNSIGNED_INT);
    char_va->attrib(1)->reuse_buffer(char_va->attrib(0));


    char_prg = new gl::program {gl::shader::vert("assets/char_vert.glsl"), gl::shader::frag("assets/char_frag.glsl")};
    char_prg->bind_attrib("va_pos", 0);
    char_prg->bind_attrib("va_char", 1);
    char_prg->bind_frag("out_col", 0);


    latin_font = new gl::texture("assets/latin.png");
    latin_font->filter(GL_NEAREST);

    mado_font = new gl::texture("assets/mado.png");
    mado_font->filter(GL_NEAREST);

    tengwar_font = new gl::texture("assets/tengwar.png");
    tengwar_font->filter(GL_NEAREST);

    char_prg->uniform<gl::texture>("font") = *latin_font;
}


void set_text_color(const vec4 &color)
{
    char_prg->uniform<vec4>("color") = color;
}


void draw_text(const vec2 &pos, const vec2 &size, const std::string &text, HAlignment halign, VAlignment valign)
{
    std::vector<unsigned> tt;
    for (int i = 0; text[i]; i++) {
        if (text[i] & 0x80) {
            if ((text[i] & 0xe0) == 0xc0) {
                if ((text[i + 1] & 0xc0) == 0x80) {
                    unsigned v = ((text[i] & 0x1f) << 6) | (text[i + 1] & 0x3f);
                    ++i;
                    if (v < 256) {
                        tt.push_back(v);
                    } else {
                        tt.push_back(0);
                    }
                } else {
                    tt.push_back(0);
                }
            } else {
                tt.push_back(0);
            }
        } else {
            tt.push_back(text[i]);
        }
    }

    if (tt.size() > 64) {
        throw std::invalid_argument("Text is too long");
    }

    CharVAElement *cvad = static_cast<CharVAElement *>(char_va->attrib(0)->map());
    for (int i = 0; i < static_cast<int>(tt.size()); i++) {
        for (int j = 0; j < 5; j++) {
            cvad[i * 5 + j].chr = tt[i];
        }
    }
    char_va->attrib(0)->unmap();

    char_va->attrib(1)->load(sizeof(CharVAElement), offsetof(CharVAElement, chr));

    vec2 position = pos;
    switch (halign) {
        case ALIGN_LEFT:   break;
        case ALIGN_CENTER: position.x() -= tt.size() * size.x() / 2.f; break;
        case ALIGN_RIGHT:  position.x() -= tt.size() * size.x();       break;
        default: throw std::invalid_argument("draw_text(): Invalid value given for halign");
    }
    switch (valign) {
        case ALIGN_TOP:    position.y() -= size.y() / 2.f; break;
        case ALIGN_MIDDLE: break;
        case ALIGN_BOTTOM: position.y() += size.y() / 2.f; break;
        default: throw std::invalid_argument("draw_text(): Invalid value given for valign");
    }

    gl::texture *font = (olo == DE_MADO)    ? mado_font
                      : (olo == DE_TENGWAR) ? tengwar_font
                      :                       latin_font;
    font->bind();


    char_prg->use();
    char_prg->uniform<vec2>("position") = position;
    char_prg->uniform<vec2>("char_size") = size;

    char_va->set_elements(tt.size() * 5);
    char_va->draw(GL_TRIANGLE_STRIP);
}
