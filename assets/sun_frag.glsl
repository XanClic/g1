#version 150 core


in vec2 vf_position;

out vec4 out_col;


void main(void)
{
    out_col = vec4(30.0) * max(1.0 - length(vf_position), 0.0);
}
