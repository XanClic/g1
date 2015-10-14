#version 150 core


in vec3 va_position;
in vec3 va_velocity;

uniform mat4 mat_mvp;


void main(void)
{
    gl_Position = mat_mvp * vec4(va_position, 1.0);
}
