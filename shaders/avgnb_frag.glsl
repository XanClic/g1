#version 150 core

in vec2 vf_pos;

out float out_value;

uniform sampler2D tex;


void main(void)
{
    vec3 color = texture(tex, vf_pos).rgb;
    float avg = float(color.r) * 0.3 + float(color.g) * 0.6 + float(color.b) * 0.1;

    out_value = log(0.0001 + avg);
}
