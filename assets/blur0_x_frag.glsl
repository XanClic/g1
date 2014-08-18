#version 150 core

in vec2 vf_pos;

out vec4 out_col;

uniform sampler2D tex;
uniform float epsilon;


vec4 access(float diff)
{
    return texture(tex, vec2(vf_pos.x + epsilon * diff, vf_pos.y));
}

void main(void)
{
    out_col =  access( 0.000)                  * 0.180785
            + (access(-0.167) + access(0.167)) * 0.163581
            + (access(-0.333) + access(0.333)) * 0.121184
            + (access(-0.500) + access(0.500)) * 0.073502
            + (access(-0.667) + access(0.667)) * 0.036500
            + (access(-0.833) + access(0.833)) * 0.014840;
}
