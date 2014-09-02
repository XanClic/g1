#version 150 core

in float va_segment;

uniform vec2 start, end;


void main(void)
{
    gl_Position = vec4(start + (end - start) * va_segment, 0.5, 1.0);
}
