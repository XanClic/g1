#version 150 core


in vec2 vf_pos;

out vec4 out_col;

uniform sampler2D sprite;
uniform float brightness;


void main(void)
{
    out_col = texture(sprite, vf_pos) * vec4(vec3(brightness), 1.0);
}
