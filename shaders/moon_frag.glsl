#version 150 core


in vec2 vf_position;

out vec4 out_col;

uniform mat3 mat_nrm;
uniform sampler2D moon_tex;


void main(void)
{
    vec2 texcoord = vf_position * 0.5 + vec2(0.5);
    vec3 nrm = normalize(mat_nrm * vec3(vf_position, sqrt(1.0 - dot(vf_position, vf_position))));

    out_col = vec4(10.0 * max(0.0, -nrm.z) * texture(moon_tex, texcoord).rrr, 1.0);
}
