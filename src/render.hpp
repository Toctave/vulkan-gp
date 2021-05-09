#pragma once

#include <glm/glm.hpp>
#include "platform_gpu.hpp"

struct Vertex {
    glm::vec3 position;
    glm::vec2 uv;
    glm::vec3 normal;
};
static_assert(sizeof(Vertex) == sizeof(float) * 8, "Wrong size for Vertex");

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

struct GPUMesh {
    GPUBuffer<Vertex> vertex_buffer;
    GPUBuffer<uint32_t> index_buffer;
};

struct Model {
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

GPUMesh gpu_mesh_create(const GPUContext& ctx,
                        size_t vertex_count,
                        size_t triangle_count,
                        const Vertex* vertices,
                        const uint32_t* indices);
GPUMesh gpu_mesh_create(const GPUContext& ctx, const Mesh& mesh);

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
                const Model& model);

