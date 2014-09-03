#version 150 core


in vec3 vf_pos, vf_nrm;

out vec4 out_col;

uniform sampler2D color, depth, stencil;
uniform vec2 sa_z_dim, screen_dim;
uniform vec3 cam_pos, cam_fwd, light_dir;


void main(void)
{
    vec2 screen_pos = gl_FragCoord.xy / screen_dim;

    float zb = 2.0 * texture(depth, screen_pos).r - 1.0;
    float zb_a = 2.0 * sa_z_dim.x * sa_z_dim.y / (sa_z_dim.y + sa_z_dim.x - zb * (sa_z_dim.y - sa_z_dim.x));
    float zf_a = dot(vf_pos - cam_pos, cam_fwd);

    vec3 old_sample = texture(color, screen_pos).rgb;
    float stencil_val = texture(stencil, screen_pos).r;

    float div = 1000 + (1.0 - stencil_val) * (900.0 * (1.0 - length(gl_FragCoord.xy - 0.5 * screen_dim) / length(screen_dim)));
    float exp = 3.0 - 2.7 * stencil_val;

    float strength = pow(min(1.0, max((zb_a - zf_a), 0.0) / div), exp);

    vec3 normal = normalize(vf_nrm);
    vec3 to_viewer = normalize(cam_pos - vf_pos);

    float to_viewer_strength = pow(clamp(dot(light_dir, to_viewer), 0.0, 1.0), 5.0);

    float diffuse = dot(normal, -light_dir);
    float brightness = clamp(smoothstep(-0.3, 0.3, diffuse) * 1.5 + (to_viewer_strength - 1.0) * 0.5, 0.03, 1.0);

    vec3 color = mix(vec3(0.51, 0.73, 1.0), vec3(1.0, 0.3, 0.0),
                     smoothstep(-0.2, -0.1, diffuse) * to_viewer_strength);

    // Use alpha blending for background
    out_col = vec4(mix(old_sample, brightness * color, min(strength + 1.0 - stencil_val, 1.0)), min(stencil_val + strength, 1.0));
}
