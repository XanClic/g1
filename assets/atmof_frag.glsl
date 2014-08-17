#version 150 core


in vec3 vf_pos, vf_nrm, vf_screen_pos;

out vec4 out_col;

uniform sampler2D color, depth;
uniform vec2 z_dim;
uniform vec3 cam_pos, light_dir;


void main(void)
{
    float zb = 2.0 * texture(depth, vf_screen_pos.xy).r - 1.0, zf = vf_screen_pos.z;
    float zb_a = 2.0 * z_dim.x * z_dim.y / (z_dim.y + z_dim.x - zb * (z_dim.y - z_dim.x));
    float zf_a = 2.0 * z_dim.x * z_dim.y / (z_dim.y + z_dim.x - zf * (z_dim.y - z_dim.x));

    vec4 old_sample = texture(color, vf_screen_pos.xy);

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
                     smoothstep(brightness, 0.0, 0.1) *
                     pow(clamp(dot(light_dir, to_viewer), 0.0, 1.0), 5.0));

    // Use alpha blending for background
    out_col = vec4(mix(old_sample.rgb, brightness * color, min(strength + 1.0 - old_sample.a, 1.0)), max(old_sample.a + strength, 0.0));
}
