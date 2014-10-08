#version 150 core

in vec2 vf_pos;
in vec3 vf_dir;

out vec4 out_col;

uniform sampler2D fb, bloom;
uniform float factor;


void main(void)
{
    out_col = vec4(factor * texture(fb, vf_pos).rgb + texture(bloom, vf_pos).rgb, 1.0);
}
