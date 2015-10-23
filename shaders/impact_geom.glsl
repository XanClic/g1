#version 150 core


layout(points) in;
layout(triangle_strip, max_vertices=4) out;


in float vg_lifetime[], vg_total_lifetime[];

out vec2 gf_pos;
out float gf_brightness;


uniform mat4 mat_proj;


void main(void)
{
    float size = (vg_total_lifetime[0] - vg_lifetime[0]) * 10.0;

    gf_brightness = vg_lifetime[0];
    gf_pos = vec2(0.0, 0.0);
    gl_Position = mat_proj * (gl_in[0].gl_Position + vec4(-0.5, -0.5, 0.0, 0.0) * size);
    EmitVertex();

    gf_brightness = vg_lifetime[0];
    gf_pos = vec2(1.0, 0.0);
    gl_Position = mat_proj * (gl_in[0].gl_Position + vec4( 0.5, -0.5, 0.0, 0.0) * size);
    EmitVertex();

    gf_brightness = vg_lifetime[0];
    gf_pos = vec2(0.0, 1.0);
    gl_Position = mat_proj * (gl_in[0].gl_Position + vec4(-0.5,  0.5, 0.0, 0.0) * size);
    EmitVertex();

    gf_brightness = vg_lifetime[0];
    gf_pos = vec2(1.0, 1.0);
    gl_Position = mat_proj * (gl_in[0].gl_Position + vec4( 0.5,  0.5, 0.0, 0.0) * size);
    EmitVertex();

    EndPrimitive();
}
