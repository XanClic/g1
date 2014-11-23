#version 150 core


in vec3 vf_nrm;
in vec2 vf_txc;
flat in ivec2 vf_tex;

out vec4 out_col;

uniform sampler2DArray day_texture;
uniform vec4 day_texture_params[20];

uniform sampler2D cloud_normal_map;

uniform vec3 light_dir;
uniform mat3 mat_nrm;


void main(void)
{
    vec2 ctxc = (vf_txc - day_texture_params[vf_tex.x].pq) * day_texture_params[vf_tex.x].st;
    float density = texture(day_texture, vec3(ctxc, float(vf_tex.x))).a;

    vec3 local_z = normalize(vf_nrm);
    vec3 local_x = vec3(local_z.z, 0.0, -local_z.x); // cross(vec3(0.0, 1.0, 0.0), local_z);
    vec3 local_y = cross(local_z, local_x);

    vec3 local_normal = texture(cloud_normal_map, vf_txc).rgb - vec3(0.5);
    vec3 normal = normalize(local_normal.x * local_x + local_normal.y * local_y + local_normal.z * local_z);
    vec3 earth_normal = normalize(mat_nrm * local_z);

    float diff = dot(normalize(mat_nrm * normal), -light_dir);

    float brightness = smoothstep(-0.05, 0.2, diff);
    vec3 color = mix(vec3(1.0, 0.6, 0.0), vec3(1.0),
                     smoothstep(0.0, 0.1, abs(dot(earth_normal, -light_dir))));

    out_col = vec4(brightness * color, density);
}
