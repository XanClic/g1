#version 150 core


in vec3 vf_pos, vf_nrm;
in vec2 vf_txc;

out vec4 out_col;

uniform sampler2D day_texture, night_texture;

uniform vec3 light_dir, cam_pos;


void main(void)
{
    vec3 day = texture(day_texture, vf_txc).rgb;

    vec2 night_tex = texture(night_texture, vf_txc).rg;
    vec3 night = mix(vec3(0.0, 0.01, 0.05), vec3(0.8, 0.8, 0.4), pow(night_tex.r, 2.0));

    vec3 normal = normalize(vf_nrm);
    vec3 to_viewer = normalize(cam_pos - vf_pos);

    float day_co = dot(normal, -light_dir);
    float spec_co = pow(max(0.0, dot(reflect(light_dir, normal), to_viewer) * night_tex.g), 10.0);

    vec3 color = mix(night, day, clamp(2.0 * day_co + 0.5, 0.0, 1.0));

    out_col = vec4(color + spec_co * vec3(1.0, 1.0, 1.0), 1.0);
}
