#version 150 core


in vec2 va_position;

out vec2 vf_position;

uniform vec2 sun_position;
uniform vec2 sun_size;


void main(void)
{
    gl_Position = vec4(sun_position + va_position * sun_size, 0.9999, 1.0);
    vf_position = va_position;
}
