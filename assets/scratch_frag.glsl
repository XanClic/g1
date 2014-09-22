#version 150 core

in vec2 vf_pos;
in vec3 vf_dir;

out vec4 out_col;

uniform sampler2D fb, scratches;
uniform vec3 sun_dir;


void main(void)
{
    float sun_strength = pow(max(0.7 + length(sun_dir) * 0.15, dot(-sun_dir, normalize(vf_dir))), 20.0);

    out_col = texture(fb, vf_pos) + texture(scratches, vf_pos).rrra * sun_strength;
}
