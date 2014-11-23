#version 150 core


in vec4 va_data;

out float vg_texcoord, vg_strength;

uniform mat4 mat_mv;


void main(void)
{
    vg_texcoord = va_data.z;
    vg_strength = va_data.w;
    gl_Position = mat_mv * vec4(cos(va_data.x) * sin(va_data.y),
                                cos(va_data.y),
                                sin(va_data.x) * sin(va_data.y),
                                1.0);
}
