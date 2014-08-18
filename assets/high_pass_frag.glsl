#version 150 core

in vec2 vf_pos;

out vec4 out_col;

uniform sampler2D fb;


void main(void)
{
    out_col = vec4(max((texture(fb, vf_pos).rgb - vec3(0.8)), 0.0), 1.0);
}
