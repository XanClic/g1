#version 150 core

in vec2 vf_pos;

out vec4 out_col;

uniform sampler2D tex;
uniform float epsilon;


vec4 access(float diff)
{
    return texture(tex, vec2(vf_pos.x, vf_pos.y + epsilon * diff));
}

void main(void)
{
    out_col = access( 0.0) * 0.5
            + access(-1.0) * 0.25
            + access( 1.0) * 0.25;
}
