#version 150 core


in vec2 va_pos;

out vec2 vf_pos;

uniform vec2 center, size;


void main(void)
{
    gl_Position = vec4(center + va_pos * size, 0.0, 1.0);
    vf_pos = 0.5 * vec2(va_pos.x + 1.0, 1.0 - va_pos.y);
}
