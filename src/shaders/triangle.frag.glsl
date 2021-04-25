#version 450

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 outputColor;

void main() {
    outputColor = vec4(uv, 0.0f, 1.0f);
}
