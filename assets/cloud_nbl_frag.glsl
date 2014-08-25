#version 150 core


in vec3 vf_nrm;
in vec2 vf_txc;

out vec4 out_col;

uniform sampler2D cloud_texture;

uniform vec3 light_dir;


void main(void)
{
    float diff_co = max(0.0, dot(normalize(vf_nrm), -light_dir));

    out_col = vec4(vec3(diff_co), texture(cloud_texture, vf_txc).r);
}
