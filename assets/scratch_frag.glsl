#version 150 core

in vec2 vf_pos;
in vec3 vf_dir;

out vec4 out_col;

uniform sampler2D fb, scratches;
uniform vec3 sun_dir, sun_bloom_dir;
uniform vec2 sun_position;


void main(void)
{
    vec3 vfn = normalize(vf_dir);

    float sbd_dot = max(dot(-sun_bloom_dir, vfn), 0.0);
    float sd_dot  = max(dot(-sun_dir, vfn), 0.0);
    float omni_sun_strength = pow(max(0.77 + length(sun_dir) * 0.12, sd_dot), 20.0);
    float dir_sun_strength  = pow( sd_dot,  5.0) * 2.0;
    float sun_bloom         = pow(sbd_dot, 30.0) * 2.5;

    vec3 scratch_data = texture(scratches, vf_pos).rgb;
    vec2 scratch_normal = scratch_data.rg * 2.0 - vec2(1.0);

    float sdp = abs(dot(scratch_normal, normalize(vf_pos * 2.0 - vec2(1.0) - sun_position)));

    out_col = vec4(texture(fb, vf_pos).rgb
            +      vec3(pow(sdp, 3.0) * dir_sun_strength)
            +      vec3(scratch_data.b * (1.0 - length(scratch_normal)) * omni_sun_strength)
            +      vec3(sun_bloom), 1.0);
}
