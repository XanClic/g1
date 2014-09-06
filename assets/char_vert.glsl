#version 150 core


in vec2 va_pos;
in uint va_char;

out vec2 vf_pos;

uniform vec2 position, char_size;


void main(void)
{
    gl_Position = vec4(position + va_pos * char_size, 0.0, 1.0);
    vf_pos = (vec2(float(va_char % 16), float(va_char / 16)) + vec2(mod(va_pos.x, 1.0), 0.5 - va_pos.y)) / 16.0;
}
