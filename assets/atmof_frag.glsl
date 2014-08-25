#version 150 core


in vec3 vf_pos, vf_nrm;

out vec4 out_col;

uniform sampler2D color, depth;
uniform vec2 sa_z_dim, screen_dim;
uniform vec3 cam_pos, cam_fwd, light_dir;


void main(void)
{
    vec2 screen_pos = gl_FragCoord.xy / screen_dim;

    float zb = 2.0 * texture(depth, screen_pos).r - 1.0;
    float zb_a = 2.0 * sa_z_dim.x * sa_z_dim.y / (sa_z_dim.y + sa_z_dim.x - zb * (sa_z_dim.y - sa_z_dim.x));
    float zf_a = dot(vf_pos - cam_pos, cam_fwd);

    vec4 old_sample = texture(color, screen_pos);

    /*
    float div = old_sample.a == 0.0 ? 2000.0 : 1000.0;
    float exp = old_sample.a == 0.0 ? 2.0 : 0.5;
    */

    float div = 2000.0 - 1000.0 * old_sample.a;
    float exp =    2.0 -    1.5 * old_sample.a;

    float strength = pow(min(1.0, max((zb_a - zf_a), 0.0) / div), exp);

    vec3 normal = normalize(vf_nrm);
    vec3 to_viewer = normalize(cam_pos - vf_pos);

    float brightness = clamp(pow(dot(normal, -light_dir) + 1.0, 2.0), 0.1, 1.0);

    vec3 color = mix(vec3(0.5, 0.7, 1.0), vec3(1.0, 0.3, 0.0),
                     smoothstep(brightness, 0.0, 0.2) *
                     pow(clamp(dot(light_dir, to_viewer), 0.0, 1.0), 10.0));

    // Use alpha blending for background
    out_col = vec4(mix(old_sample.rgb, brightness * color, min(strength + 1.0 - old_sample.a, 1.0)), min(old_sample.a + strength, 1.0));
}
