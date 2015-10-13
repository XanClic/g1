#version 150 core


layout(points) in;
layout(triangle_strip, max_vertices=4) out;


out vec2 gf_pos;


uniform float aspect;


void main(void)
{
    vec2 size = vec2(0.002, 0.002 * aspect);

    gf_pos = vec2(-1.0, -1.0);
    gl_Position = gl_in[0].gl_Position + vec4(gf_pos * size, 0.0, 0.0);
    EmitVertex();

    gf_pos = vec2( 1.0, -1.0);
    gl_Position = gl_in[0].gl_Position + vec4(gf_pos * size, 0.0, 0.0);
    EmitVertex();

    gf_pos = vec2(-1.0,  1.0);
    gl_Position = gl_in[0].gl_Position + vec4(gf_pos * size, 0.0, 0.0);
    EmitVertex();

    gf_pos = vec2( 1.0,  1.0);
    gl_Position = gl_in[0].gl_Position + vec4(gf_pos * size, 0.0, 0.0);
    EmitVertex();

    EndPrimitive();
}
