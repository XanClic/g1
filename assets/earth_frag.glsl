#version 150 core
#extension GL_ARB_bindless_texture: require


in vec3 vf_pos, vf_nrm;
in vec2 vf_txc;
flat in ivec2 vf_tex;

out vec4 out_col;

layout(bindless_sampler) uniform sampler2D day_textures[20], night_textures[20];
uniform vec4 day_texture_params[20], night_texture_params[20];

uniform vec3 light_dir, cam_pos;


void main(void)
{
    vec2 cdtxc = (vf_txc - day_texture_params[vf_tex.x].pq) * day_texture_params[vf_tex.x].st;
    vec3 day = texture(day_textures[vf_tex.x], cdtxc).rgb;

    vec2 cntxc = (vf_txc - night_texture_params[vf_tex.y].pq) * night_texture_params[vf_tex.y].st;
    vec2 night_tex = texture(night_textures[vf_tex.y], cntxc).rg;
    vec3 night = mix(vec3(0.0, 0.01, 0.05), vec3(0.8, 0.8, 0.4), pow(night_tex.r, 2.0));

    vec3 normal = normalize(vf_nrm);
    vec3 to_viewer = normalize(cam_pos - vf_pos);

    float day_co = dot(normal, -light_dir);
    float spec_co = pow(max(0.0, dot(reflect(light_dir, normal), to_viewer) * night_tex.g), 10.0);

    vec3 color = mix(night, day, smoothstep(-0.15, 0.3, day_co));

    out_col = vec4(color + spec_co * vec3(1.0, 1.0, 1.0), 1.0);
}
