#version 150 core

in vec2 vf_pos;

out vec4 out_col;

uniform sampler2D menu_bg;


void main(void)
{
    out_col = texture(menu_bg, vf_pos);
}
