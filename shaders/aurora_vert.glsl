#version 150 core


in vec2 va_position;

out vec2 vg_origpos;

uniform mat4 mat_mv;


void main(void)
{
    vg_origpos = va_position;
    gl_Position = mat_mv * vec4(cos(va_position.x) * sin(va_position.y), cos(va_position.y), sin(va_position.x) * sin(va_position.y), 1.0);
}
