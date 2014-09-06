#version 150 core


in vec2 vf_pos;

out vec4 out_col;

uniform sampler2D font;
uniform vec4 color;


void main(void)
{
    if (texture(font, vf_pos).r > 0.5) {
        discard;
    } else {
        out_col = color;
    }
}
