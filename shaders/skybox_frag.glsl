#version 150 core


in vec3 vf_pos;

out vec4 out_col;

uniform samplerCube skybox;


void main(void)
{
    out_col = vec4(pow(texture(skybox, vf_pos).rgb * 0.5, vec3(2.0)), 1.0);
}
