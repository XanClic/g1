#version 150 core


in vec2 gf_pos;
in float gf_brightness;

out vec4 out_col;


uniform sampler2D tex;


void main(void)
{
    vec4 col = texture(tex, gf_pos);
    out_col = vec4(col.rgb, col.a * gf_brightness);
}
