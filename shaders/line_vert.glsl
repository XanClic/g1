#version 150 core

in vec2 va_position;


void main(void)
{
    gl_Position = vec4(va_position, 0.5, 1.0);
}
