#version 150 core

in vec2 vf_pos;

out vec4 out_col;

uniform sampler2D fb;


void main(void)
{
  out_col = texture(fb, vf_pos);
}
