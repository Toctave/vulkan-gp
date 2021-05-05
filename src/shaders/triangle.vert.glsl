#version 450

struct Matrices {
    mat4 mvp;
    mat4 model_view;
};

layout(push_constant) uniform push_constants {
    Matrices u_mat;
};

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec3 normal;

layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec3 out_normal;

void main() {
    gl_Position = u_mat.mvp * vec4(position, 1);
    out_uv = uv;
    out_normal = (u_mat.model_view * vec4(normal, 0)).xyz;
}
