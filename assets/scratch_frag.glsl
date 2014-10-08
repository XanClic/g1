#version 150 core

in vec2 vf_pos;
in vec3 vf_dir;

out vec4 out_col;

uniform sampler2D fb, scratches;
uniform vec3 sun_dir, sun_bloom_dir;


void main(void)
{
    float sun_strength = pow(max(0.73 + length(sun_dir) * 0.12, dot(-sun_dir, normalize(vf_dir))), 20.0);
    float sun_bloom = max(0.0, pow(dot(-sun_bloom_dir, normalize(vf_dir)), 30.0));

    out_col = vec4(texture(fb, vf_pos).rgb + texture(scratches, vf_pos).rrr * sun_strength + vec3(sun_bloom), 1.0);
}
