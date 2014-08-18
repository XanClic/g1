#version 150 core

in vec2 vf_pos;

out vec4 out_col;

uniform sampler2D fb, bloom;


void main(void)
{
    out_col = vec4(texture(fb, vf_pos).rgb + texture(bloom, vf_pos).rgb, 1.0);
}
