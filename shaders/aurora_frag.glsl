#version 150 core


in vec2 gf_texcoord;
in vec3 gf_pos;

out vec4 out_col;

uniform sampler2D depth, stencil, bands;
uniform vec2 screen_dim, sa_z_dim;
uniform vec3 cam_pos, cam_fwd;


void main(void)
{
    vec2 screen_pos = gl_FragCoord.xy / screen_dim;

    float green = step(0.2, gf_texcoord.y) * exp(smoothstep(0.8, 0.2, gf_texcoord.y) * 5.0 - 5.0) +
                  pow((1.0 - step(0.2, gf_texcoord.y)) * smoothstep(0.0, 0.2, gf_texcoord.y), 5.0);
    float red   = step(0.4, gf_texcoord.y) * exp(smoothstep(1.0, 0.4, gf_texcoord.y) * 5.0 - 5.0) +
                  pow((1.0 - step(0.4, gf_texcoord.y)) * smoothstep(0.2, 0.4, gf_texcoord.y), 5.0);

    vec3 base = mix(vec3(0.01, 0.05, 0.02),
                    vec3(0.02, 0.00, 0.004),
                    red / max(green + red, 0.01));

    float ze = 2.0 * texture(depth, screen_pos).r - 1.0;
    float ze_a = 2.0 * sa_z_dim.x * sa_z_dim.y / (sa_z_dim.y + sa_z_dim.x - ze * (sa_z_dim.y - sa_z_dim.x));
    float za_a = dot(gf_pos - cam_pos, cam_fwd);

    out_col = vec4(base, (green + red) * max(1.0 - texture(stencil, screen_pos).r, step(za_a, ze_a)) * texture(bands, vec2(gf_texcoord.x, 0.0)).r);
}
