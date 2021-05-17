#pragma once

#include <glm/glm.hpp>
#include "platform_gpu.hpp"

struct Mesh {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec3> normals;
    
    std::vector<uint32_t> indices;
};

struct Vertex {
    glm::vec3 position;
    glm::vec2 uv;
    glm::vec3 normal;
};
static_assert(sizeof(Vertex) == sizeof(float) * 8, "Wrong size for Vertex");

struct GPUMesh {
    GPUBuffer<Vertex> vertex_buffer;
    GPUBuffer<uint32_t> index_buffer;
    GPUBuffer<glm::vec3> color_buffer;
};

struct GPUModel {
    const GPUMesh* mesh;
    glm::mat4 transform;
};

struct Camera {
    glm::vec3 eye;
    glm::vec3 target;
    float fov;
    float aspect;
    float near;
    float far;
};

GPUMesh gpu_mesh_allocate(const GPUContext& gpu, size_t vertex_count, size_t triangle_count);
void gpu_mesh_upload(const GPUContext& gpu, GPUMesh& gpu_mesh, const Mesh& mesh);

Mesh load_obj_mesh(const std::string& filename);

void gpu_mesh_destroy(const GPUContext& ctx, GPUMesh& mesh);

GraphicsFrame begin_frame(GraphicsContext& ctx);
void end_frame(const GraphicsContext& ctx,
               GraphicsFrame& frame);

void draw_mesh(const GraphicsFrame& frame,
               const GPUMesh& mesh);

void draw_model(const GraphicsFrame& frame,
                const glm::mat4& view,
                const glm::mat4& proj,
                const GPUModel& model);

