#version 150 core

in vec2 vf_pos;

out float out_value;

uniform sampler2D tex;


void main(void)
{
    out_value = texture(tex, vf_pos).r;
}
