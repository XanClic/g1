#version 150 core
#extension GL_ARB_bindless_texture: require


in vec3 vf_nrm;
in vec2 vf_txc;
flat in ivec2 vf_tex;

out vec4 out_col;

layout(bindless_sampler) uniform sampler2D cloud_textures[20];
uniform vec4 cloud_texture_params[20];

uniform vec3 light_dir;


void main(void)
{
    vec2 ctxc = (vf_txc - cloud_texture_params[vf_tex.x].pq) * cloud_texture_params[vf_tex.x].st;

    float diff_co = max(0.0, dot(normalize(vf_nrm), -light_dir));

    out_col = vec4(vec3(diff_co), texture(cloud_textures[vf_tex.x], ctxc).r);
}
