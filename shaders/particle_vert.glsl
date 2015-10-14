#version 150 core


in vec3 va_position;
in vec3 va_orientation;

out vec4 vg_start;

uniform mat4 mat_mvp;


void main(void)
{
    vg_start = mat_mvp *
               vec4(va_position - va_orientation, 1.0);
    gl_Position = mat_mvp * vec4(va_position, 1.0);
}
