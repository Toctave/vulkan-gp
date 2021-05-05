#pragma once

#include <glm/glm.hpp>
#include "platform.hpp"

struct Mesh {
    GPUBuffer<glm::vec3> pos_buffer;
    GPUBuffer<glm::vec2> uv_buffer;
    GPUBuffer<glm::vec3> normal_buffer;
    GPUBuffer<uint32_t> index_buffer;
    size_t vertex_count;
    size_t triangle_count;
};

struct Model {
    const Mesh* mesh;
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

Mesh create_mesh(const GraphicsContext& ctx,
                 size_t vertex_count,
                 size_t triangle_count,
                 uint32_t* indices,
                 glm::vec3* positions,
                 glm::vec2* uvs,
                 glm::vec3* normals);
Mesh load_obj_mesh(const GraphicsContext& ctx,
                   const std::string& filename);
void destroy_mesh(const GraphicsContext& ctx, Mesh& mesh);

GraphicsFrame begin_frame(GraphicsContext& ctx);
void end_frame(const GraphicsContext& ctx,
               GraphicsFrame& frame);

void draw_mesh(const GraphicsFrame& frame,
               const Mesh& mesh);

void draw_model(const GraphicsFrame& frame,
                const glm::mat4& view,
                const glm::mat4& proj,
                const Model& model);

