#version 450

#extension GL_EXT_debug_printf : enable

layout(local_size_x = 32) in;

struct Vertex {
    vec3 position;
    float u;
    float v;
    float nx;
    float ny;
    float nz;
};

layout(std430, binding = 0) buffer mesh_in
{
    Vertex vertices_in[];
};

layout(std430, binding = 1) buffer mesh_out
{
    Vertex vertices_out[];
};

layout(std430, binding = 2) buffer parameters
{
    float t;
};

void main() {
    float k = 10.0f;
    float w = 5.0f;
    float amp = .05f;
    
    uint i = gl_GlobalInvocationID.x;
    vertices_out[i] = vertices_in[i];
    
    vertices_out[i].position.x += amp * sin(vertices_in[i].position.z * k - w * t);
}
