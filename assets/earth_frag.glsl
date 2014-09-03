#version 150 core
#extension GL_ARB_bindless_texture: require


in vec3 vf_pos, vf_nrm;
in vec2 vf_txc;
flat in ivec2 vf_tex;

out vec4 out_col;
out float out_stencil;

layout(bindless_sampler) uniform sampler2D day_textures[20], night_textures[20], cloud_textures[20];
uniform vec4 day_texture_params[20], night_texture_params[20], cloud_texture_params[20];

uniform vec3 light_dir, cam_pos;


void main(void)
{
    vec2 cdtxc = (vf_txc - day_texture_params[vf_tex.x].pq) * day_texture_params[vf_tex.x].st;
    vec3 day = texture(day_textures[vf_tex.x], cdtxc).rgb;

    vec2 cntxc = (vf_txc - night_texture_params[vf_tex.y].pq) * night_texture_params[vf_tex.y].st;
    vec2 night_tex = texture(night_textures[vf_tex.y], cntxc).rg;
    vec3 night = mix(vec3(0.005, 0.0075, 0.01), vec3(0.2, 0.2, 0.1), night_tex.r);

    vec2 cctxc = (vf_txc - cloud_texture_params[vf_tex.x].pq) * cloud_texture_params[vf_tex.x].st;
    float cloud = texture(cloud_textures[vf_tex.x], cctxc).r;

    vec3 normal = normalize(vf_nrm);
    vec3 to_viewer = normalize(cam_pos - vf_pos);

    // Slick Schlick
    float ndoty =  dot(normal, to_viewer);
    float ndotx = -dot(normal, light_dir);

    vec3 n_ny = normalize(to_viewer - light_dir);
    float ndotny_sqr = dot(normal, n_ny);
    float xdotny = dot(to_viewer, n_ny);

    vec3 facet_proj = n_ny - ndotny_sqr * normal;
    float facet_proj_sqr = dot(facet_proj, facet_proj);

    ndotny_sqr *= ndotny_sqr;

    float spec_co = smoothstep(-0.1, 0.05, ndotx) * 0.1 * (0.4 + 0.6 * pow(max(1.0 - xdotny, 0.0), 5.0)) * pow(1.0 + 0.2 * ndotny_sqr - ndotny_sqr, -2.0);

    vec3 night_addition = smoothstep(0.1, -0.05, ndotx) * night;

    vec3 spec_color = mix(vec3(2.0, 2.0, 1.0), vec3(2.0, 2.0, 2.0), smoothstep(0.0, 0.2, dot(normal, to_viewer)));
    spec_color = spec_co * mix(day, spec_color, smoothstep(0.5, 2.0, spec_co));

    vec3 diff_color = smoothstep(-0.1, 0.5, ndotx) * day;

    out_col = vec4((1.0 - cloud * 0.7) * mix(diff_color, spec_color, night_tex.g) + night_addition, 1.0);
    out_stencil = 1.0;
}
