#include "render.hpp"

#include "platform.hpp"

Mesh create_mesh(const GraphicsContext& ctx,
                        size_t vertex_count,
                        size_t triangle_count,
                        uint32_t* indices,
                        glm::vec3* positions,
                        glm::vec2* uvs) {
    Mesh mesh;
    mesh.vertex_count = vertex_count;
    mesh.triangle_count = triangle_count;
    mesh.pos_buffer = allocate_and_fill_buffer(ctx,
                                               positions,
                                               vertex_count,
                                               VERTEX_BUFFER);
    mesh.uv_buffer = allocate_and_fill_buffer(ctx,
                                              uvs,
                                              vertex_count,
                                              VERTEX_BUFFER);
    mesh.index_buffer = allocate_and_fill_buffer(ctx,
                                                 indices,
                                                 triangle_count * 3,
                                                 INDEX_BUFFER);
    return mesh;
}

void destroy_mesh(const GraphicsContext& ctx, Mesh& mesh) {
    gpu_buffer_free(ctx, mesh.index_buffer);
    gpu_buffer_free(ctx, mesh.pos_buffer);
    gpu_buffer_free(ctx, mesh.uv_buffer);
}

