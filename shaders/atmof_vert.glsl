#version 150 core


in vec3 va_position, va_normal;

out vec3 vf_pos, vf_nrm;

uniform mat4 mat_mv, mat_proj;
uniform mat3 mat_nrm;


void main(void)
{
    vec4 world_pos = mat_mv * vec4(va_position, 1.0);
    vec4 proj = mat_proj * world_pos;

    vf_pos = world_pos.xyz;
    gl_Position = proj;

    vf_nrm = normalize(mat_nrm * va_normal);
}
