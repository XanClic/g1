#version 150 core


in vec3 va_position;

out vec3 vf_pos;

uniform mat4 mat_mvp;


void main(void)
{
    vf_pos = va_position;
    gl_Position = mat_mvp * vec4(1000.0 * va_position, 1.0);
}
