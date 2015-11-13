#version 150 core

in vec2 va_pos;

out vec2 vf_pos;
out vec3 vf_dir;

uniform vec3 cam_fwd, cam_right, cam_up;
uniform float tan_xhfov, tan_yhfov;


void main(void)
{
    gl_Position = vec4(va_pos, 0.0, 1.0);
    vf_pos = (va_pos + vec2(1.0, 1.0)) / 2.0;

    vf_dir = cam_fwd + va_pos.x * tan_xhfov * cam_right + va_pos.y * tan_yhfov * cam_up;
}
