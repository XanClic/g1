#version 150 core

in vec2 in_pos;

out vec2 vf_pos;


void main(void)
{
    gl_Position = vec4(in_pos, 0.0, 1.0);
    vf_pos = (in_pos + vec2(1.0, 1.0)) / 2.0;
}
