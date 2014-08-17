#version 150 core


in vec3 va_position, va_normal;

out vec3 vf_pos, vf_nrm, vf_screen_pos;

uniform mat4 mat_mv, mat_proj;
uniform mat3 mat_nrm;


void main(void)
{
    vec4 world_pos = mat_mv * vec4(va_position, 1.0);
    vec4 proj = mat_proj * world_pos;
    vec3 screen_pos = proj.xyz / proj.w;

    screen_pos.xy = screen_pos.xy / 2.0 + vec2(0.5);

    vf_pos = world_pos.xyz;
    vf_screen_pos = screen_pos.xyz;
    gl_Position = proj;

    vf_nrm = normalize(mat_nrm * va_normal);
}
