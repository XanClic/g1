#version 150 core


in vec3 vf_pos, vf_nrm;
in vec2 vf_txc;

out vec4 out_col;

uniform sampler2D tex;
uniform vec3 light_dir, cam_pos;


void main(void)
{
    vec4 day = texture(tex, vf_txc * vec2(1.0, 0.5));
    vec4 night = texture(tex, vf_txc * vec2(1.0, 0.5) + vec2(0.0, 0.5));

    vec3 normal = normalize(vf_nrm);
    vec3 to_viewer = normalize(cam_pos - vf_pos);

    float day_co = dot(normal, -light_dir);
    float spec_co = pow(max(0.0, dot(reflect(light_dir, normal), to_viewer) * day.a), 10.0);

    vec3 color = mix(night.rgb, day.rgb, clamp(2.0 * day_co + 0.5, 0.0, 1.0));

    out_col = vec4(color + spec_co * vec3(1.0, 1.0, 1.0), 1.0);
}
