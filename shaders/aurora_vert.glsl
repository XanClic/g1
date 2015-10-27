#version 150 core


in vec2 va_position;
in float va_texcoord, va_strength;

out float vg_texcoord, vg_strength;

uniform mat4 mat_mv;


void main(void)
{
    vg_texcoord = va_texcoord;
    vg_strength = va_strength;
    gl_Position = mat_mv * vec4(cos(va_position.x) * sin(va_position.y),
                                cos(va_position.y),
                                sin(va_position.x) * sin(va_position.y),
                                1.0);
}
