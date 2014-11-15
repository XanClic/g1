#version 150 core


in vec2 va_position;

out vec2 vf_position;

uniform vec3 moon_position, right, up;
uniform mat4 mat_mvp;


void main(void)
{
    gl_Position = (mat_mvp * vec4(moon_position + 1738.0 * (va_position.x * right + va_position.y * up), 1.0)).xyww;

    vf_position = va_position;
}
