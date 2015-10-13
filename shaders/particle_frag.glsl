#version 150 core


in vec2 gf_pos;

out vec4 out_col;


void main(void)
{
    out_col = vec4(2.0, 0.5, 0.2, max(0.0, 1.0 - length(gf_pos)));
}
