#version 450

#extension GL_EXT_debug_printf : enable

layout(local_size_x = 1) in;

struct Vertex {
    vec3 position;
    float u;
    float v;
    float nx;
    float ny;
    float nz;
};

layout(std430, binding = 0) buffer b1
{
    Vertex vertices[];
};

void main() {
    uint i = gl_GlobalInvocationID.x;
    vertices[i].position.x += .1 * sin(vertices[i].position.z * 10.0f);
}
