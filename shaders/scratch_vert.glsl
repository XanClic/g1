#version 150 core

in vec2 va_pos;

out vec2 vf_pos;
out vec3 vf_dir;

uniform vec3 cam_fwd, cam_right, cam_up;
uniform float xhfov, yhfov;


void main(void)
{
    gl_Position = vec4(va_pos, 0.0, 1.0);
    vf_pos = (va_pos + vec2(1.0, 1.0)) / 2.0;

    vf_dir = cam_fwd + tan(va_pos.x * xhfov) * cam_right + tan(va_pos.y * yhfov) * cam_up;
}
