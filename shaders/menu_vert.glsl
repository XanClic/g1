#version 150 core

in vec2 va_pos;

out vec2 vf_pos;

uniform vec2 size;


void main(void)
{
    gl_Position = vec4(va_pos * size, 0.0, 1.0);
    vf_pos = (vec2(va_pos.x, -va_pos.y) + vec2(1.0, 1.0)) / 2.0;
}
