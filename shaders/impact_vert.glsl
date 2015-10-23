#version 150 core


in vec3 va_position;
in float va_lifetime, va_total_lifetime;

out float vg_lifetime, vg_total_lifetime;


uniform mat4 mat_mv;


void main(void)
{
    vg_lifetime = va_lifetime;
    vg_total_lifetime = va_total_lifetime;
    gl_Position = mat_mv * vec4(va_position, 1.0);
}
