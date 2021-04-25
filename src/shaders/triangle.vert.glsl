#version 450

layout(push_constant) uniform push_constants {
    mat4 u_mvp;
};

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;

layout(location = 0) out vec2 outUV;

void main() {
    gl_Position = u_mvp * vec4(position, 1);
    outUV = uv;
}
