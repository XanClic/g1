#version 150 core


layout(lines) in;
layout(triangle_strip, max_vertices=8) out;

in float vg_texcoord[];
in float vg_strength[];

out vec2 gf_texcoord;
out float gf_strength;
out vec3 gf_pos;

uniform mat4 mat_proj;


void main(void)
{
    float tex_start = vg_texcoord[0];
    float tex_end   = vg_texcoord[1];

    tex_end = tex_start + fract(tex_end - tex_start);

    gf_texcoord = vec2(tex_start, 0.0);
    gf_strength = vg_strength[0];
    gf_pos      = gl_in[0].gl_Position.xyz;
    gl_Position = mat_proj * vec4(gf_pos, 1.0);
    EmitVertex();

    gf_texcoord = vec2(tex_start, 1.0);
    gf_strength = vg_strength[0];
    gf_pos      = 1.04 * gl_in[0].gl_Position.xyz;
    gl_Position = mat_proj * vec4(gf_pos, 1.0);
    EmitVertex();

    gf_texcoord = vec2(tex_end, 0.0);
    gf_strength = vg_strength[1];
    gf_pos      = gl_in[1].gl_Position.xyz;
    gl_Position = mat_proj * vec4(gf_pos, 1.0);
    EmitVertex();

    gf_texcoord = vec2(tex_end, 1.0);
    gf_strength = vg_strength[1];
    gf_pos      = 1.04 * gl_in[1].gl_Position.xyz;
    gl_Position = mat_proj * vec4(gf_pos, 1.0);
    EmitVertex();

    EndPrimitive();


    gf_texcoord = vec2(tex_start, 0.0);
    gf_strength = vg_strength[0];
    gf_pos      = -gl_in[0].gl_Position.xyz;
    gl_Position = mat_proj * vec4(gf_pos, 1.0);
    EmitVertex();

    gf_texcoord = vec2(tex_start, 1.0);
    gf_strength = vg_strength[0];
    gf_pos      = -1.04 * gl_in[0].gl_Position.xyz;
    gl_Position = mat_proj * vec4(gf_pos, 1.0);
    EmitVertex();

    gf_texcoord = vec2(tex_end, 0.0);
    gf_strength = vg_strength[1];
    gf_pos      = -gl_in[1].gl_Position.xyz;
    gl_Position = mat_proj * vec4(gf_pos, 1.0);
    EmitVertex();

    gf_texcoord = vec2(tex_end, 1.0);
    gf_strength = vg_strength[1];
    gf_pos      = -1.04 * gl_in[1].gl_Position.xyz;
    gl_Position = mat_proj * vec4(gf_pos, 1.0);
    EmitVertex();

    EndPrimitive();
}
