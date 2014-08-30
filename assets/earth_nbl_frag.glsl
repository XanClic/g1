#version 150 core


in vec3 vf_pos, vf_nrm;
in vec2 vf_txc;
flat in ivec2 vf_tex;

out vec4 out_col;
out float out_stencil;

uniform sampler2DArray day_texture, night_texture, cloud_texture;
uniform vec4 day_texture_params[20], night_texture_params[20], cloud_texture_params[20];

uniform vec3 light_dir, cam_pos;


void main(void)
{
    vec2 cdtxc = (vf_txc - day_texture_params[vf_tex.x].pq) * day_texture_params[vf_tex.x].st;
    vec3 day = texture(day_texture, vec3(cdtxc, float(vf_tex.x))).rgb;

    vec2 cntxc = (vf_txc - night_texture_params[vf_tex.y].pq) * night_texture_params[vf_tex.y].st;
    vec2 night_tex = texture(night_texture, vec3(cntxc, float(vf_tex.y))).rg;
    vec3 night = mix(vec3(0.0, 0.01, 0.05), vec3(0.8, 0.8, 0.4), pow(night_tex.r, 2.0));

    vec2 cctxc = (vf_txc - cloud_texture_params[vf_tex.x].pq) * cloud_texture_params[vf_tex.x].st;
    float cloud = texture(cloud_texture, vec3(cctxc, float(vf_tex.x))).r;

    vec3 normal = normalize(vf_nrm);
    vec3 to_viewer = normalize(cam_pos - vf_pos);

    // Slick Schlick
    float ndoty = dot(normal, to_viewer);
    float ndotx = dot(normal, -light_dir);

    vec3 n_ny = normalize(-light_dir + to_viewer);
    float ndotny_sqr = dot(normal, n_ny);
    float xdotny = dot(to_viewer, n_ny);

    vec3 facet_proj = n_ny - ndotny_sqr * normal;
    float facet_proj_sqr = dot(facet_proj, facet_proj);

    ndotny_sqr *= ndotny_sqr;

    float spec_co = 0.1 * (0.2 + 0.8 * pow(1.0 - xdotny, 5.0)) * pow(1.0 + 0.2 * ndotny_sqr - ndotny_sqr, -2.0) * night_tex.g * smoothstep(-0.2, 0.0, ndotx);

    day *= smoothstep(-0.1, 0.5, ndotx);

    vec3 diff_color = mix(night, day, smoothstep(-0.15, 0.0, ndotx));
    vec3 spec_color = mix(vec3(2.0, 2.0, 1.0), vec3(2.0, 2.0, 2.0), smoothstep(0.0, 0.2, dot(normal, to_viewer)));

    out_col = vec4((1.0 - cloud * 0.7) * (diff_color + spec_co * spec_color), 1.0);
    out_stencil = 1.0;
}
