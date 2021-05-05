#include "render.hpp"

#include "platform.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <tuple>
#include <unordered_map>

#include "hash_tuple.hpp"

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

Mesh load_obj_mesh(const GraphicsContext &ctx, const std::string &filename) {
    using vertex_tuple = std::tuple<uint32_t, uint32_t, uint32_t>;
    
    std::ifstream file(filename);
    std::string line;

    std::unordered_map<vertex_tuple, uint32_t> vertex_indices;

    std::vector<glm::vec3> raw_positions;
    std::vector<glm::vec3> raw_normals;
    std::vector<glm::vec2> raw_uvs;
    
    std::vector<uint32_t> indices;
    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> uvs;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream iss(line);
        std::string head;

        iss >> head;

        if (head == "f") {
            std::vector<vertex_tuple> face;
            while (iss) {
                std::string block;
                iss >> block;
                if (block.empty()) {
                    continue;
                }

                vertex_tuple vertex(-1, -1, -1);
                char delim1, delim2;

                std::istringstream block_iss(block);
                block_iss >> std::get<0>(vertex);
                block_iss.clear();
                
                block_iss >> delim1;

                block_iss >> std::get<1>(vertex);
                block_iss.clear();

                block_iss >> delim2;
                
                block_iss >> std::get<2>(vertex);
                block_iss.clear();

                if (delim1 != '/' || delim2 != '/') {
                    std::cout << block << "\n";
                    throw std::runtime_error("Expected '/' in .obj file");
                }

                face.push_back(vertex);
            }

            std::vector<uint32_t> face_indices(face.size());
            for (uint32_t i = 0; i < face_indices.size(); i++) {
                if (vertex_indices.find(face[i]) == vertex_indices.end()) {
                    vertex_indices[face[i]] = positions.size();

                    positions.push_back(raw_positions[std::get<0>(face[i]) - 1]);
                    uvs.push_back(raw_uvs[std::get<1>(face[i]) - 1]);
                }
                face_indices[i] = vertex_indices[face[i]];
            }

            for (uint32_t second = 0; second + 1 < face.size(); second++) {
                uint32_t third = second + 1;
                indices.push_back(face_indices[0]);
                indices.push_back(face_indices[second]);
                indices.push_back(face_indices[third]);
            }
        } else if (head == "v") {
            glm::vec3 pos;
            iss >> pos.x >> pos.y >> pos.z;
            raw_positions.push_back(pos);
        } else if (head == "vt") {
            glm::vec2 uv;
            iss >> uv.x >> uv.y;
            raw_uvs.push_back(uv);
        } else if (head == "vn") {
            glm::vec3 normal;
            iss >> normal.x >> normal.y >> normal.z;
            raw_normals.push_back(normal);
        } else {
            std::cout << "Unknown head " << head << "\n";
        }
    }

    return create_mesh(ctx,
                       positions.size(),
                       indices.size() / 3,
                       indices.data(),
                       positions.data(),
                       uvs.data());
}

void destroy_mesh(const GraphicsContext& ctx, Mesh& mesh) {
    gpu_buffer_free(ctx, mesh.index_buffer);
    gpu_buffer_free(ctx, mesh.pos_buffer);
    gpu_buffer_free(ctx, mesh.uv_buffer);
}

