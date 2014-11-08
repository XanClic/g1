#version 150 core


layout(lines) in;
layout(triangle_strip, max_vertices=8) out;

in vec2 vg_origpos[];

out vec2 gf_texcoord;
out vec3 gf_pos;

uniform mat4 mat_proj;


void main(void)
{
    float tex_start = 10.0 * vg_origpos[0].x / 6.283;
    float tex_end   = 10.0 * vg_origpos[1].x / 6.283;

    tex_end = tex_start + fract(tex_end - tex_start);

    gf_texcoord = vec2(tex_start, 0.0);
    gf_pos      = gl_in[0].gl_Position.xyz;
    gl_Position = mat_proj * vec4(gf_pos, 1.0);
    EmitVertex();

    gf_texcoord = vec2(tex_start, 1.0);
    gf_pos      = 1.04 * gl_in[0].gl_Position.xyz;
    gl_Position = mat_proj * vec4(gf_pos, 1.0);
    EmitVertex();

    gf_texcoord = vec2(tex_end, 0.0);
    gf_pos      = gl_in[1].gl_Position.xyz;
    gl_Position = mat_proj * vec4(gf_pos, 1.0);
    EmitVertex();

    gf_texcoord = vec2(tex_end, 1.0);
    gf_pos      = 1.04 * gl_in[1].gl_Position.xyz;
    gl_Position = mat_proj * vec4(gf_pos, 1.0);
    EmitVertex();

    EndPrimitive();


    gf_texcoord = vec2(tex_start, 0.0);
    gf_pos      = -gl_in[0].gl_Position.xyz;
    gl_Position = mat_proj * vec4(gf_pos, 1.0);
    EmitVertex();

    gf_texcoord = vec2(tex_start, 1.0);
    gf_pos      = -1.04 * gl_in[0].gl_Position.xyz;
    gl_Position = mat_proj * vec4(gf_pos, 1.0);
    EmitVertex();

    gf_texcoord = vec2(tex_end, 0.0);
    gf_pos      = -gl_in[1].gl_Position.xyz;
    gl_Position = mat_proj * vec4(gf_pos, 1.0);
    EmitVertex();

    gf_texcoord = vec2(tex_end, 1.0);
    gf_pos      = -1.04 * gl_in[1].gl_Position.xyz;
    gl_Position = mat_proj * vec4(gf_pos, 1.0);
    EmitVertex();

    EndPrimitive();
}
