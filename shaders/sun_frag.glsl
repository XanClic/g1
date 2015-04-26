#version 150 core


in vec2 vf_position;

out vec4 out_col;


void main(void)
{
    out_col = vec4(2.0) * smoothstep(0.0, 0.1, 1.0 - length(vf_position));
}
