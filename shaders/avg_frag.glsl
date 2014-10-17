#version 150 core

in vec2 vf_pos;

out float out_value;

uniform sampler2D tex, bloom;


void main(void)
{
    vec3 color = texture(tex, vf_pos).rgb + texture(bloom, vf_pos).rgb * 0.4;
    float avg = float(color.r) * 0.3 + float(color.g) * 0.6 + float(color.b) * 0.1;

    out_value = log(0.0001 + avg);
}
