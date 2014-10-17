#version 150 core


in vec3 va_position, va_normal;
in vec2 va_texcoord;
in ivec2 va_textures;

out vec3 vf_nrm;
out vec2 vf_txc;
flat out ivec2 vf_tex;

uniform mat4 mat_mv, mat_proj;


void main(void)
{
    vec4 world_pos = mat_mv * vec4(va_position, 1.0);
    gl_Position = mat_proj * world_pos;

    vf_nrm = va_normal;
    vf_txc = va_texcoord;
    vf_tex = va_textures;
}
