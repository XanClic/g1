#version 150 core


layout(points) in;
layout(triangle_strip, max_vertices=8) out;


in vec4 vg_start[];

out vec2 gf_pos;


uniform float aspect;


void main(void)
{
    vec2 size = vec2(0.5, 0.5 * aspect);

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


    // TODO: Is this worth it?
    if (length(gl_in[0].gl_Position.xy / gl_in[0].gl_Position.w -
        vg_start[0].xy / vg_start[0].w) < 0.01)
    {
        return;
    }


    gf_pos = vec2(-1.0, 1.0);
    gl_Position = vg_start[0] + vec4(-0.5, 0.0, 0.0, 0.0);
    EmitVertex();

    gf_pos = vec2( 1.0, 1.0);
    gl_Position = vg_start[0] + vec4( 0.5, 0.0, 0.0, 0.0);
    EmitVertex();

    gf_pos = vec2(-1.0, 0.0);
    gl_Position = gl_in[0].gl_Position + vec4(-0.5, 0.0, 0.0, 0.0);
    EmitVertex();

    gf_pos = vec2( 1.0, 0.0);
    gl_Position = gl_in[0].gl_Position + vec4( 0.5, 0.0, 0.0, 0.0);
    EmitVertex();

    EndPrimitive();
}
