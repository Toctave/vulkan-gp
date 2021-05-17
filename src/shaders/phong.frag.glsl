#version 450

layout (location = 0) in vec2 uv;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 albedo;

layout (location = 0) out vec4 out_color;

void main() {
    vec3 n = normalize(normal);
    vec3 l = normalize(vec3(1, 1, 1));

    vec3 r = reflect(l, n);

    vec3 ambient = vec3(.05, .05, .05);
    vec3 specular = vec3(1, 1, 1);

    float alpha = 10;

    float d = max(0, dot(n, l));
    float s = pow(max(0, -r.z), alpha);

    vec3 radiance = albedo * (ambient + d) + specular * s;

    out_color = vec4(radiance, 1);
}
