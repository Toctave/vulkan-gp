#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <cmath>

#include <cstring>
#include <cassert>

#include "platform_gpu.hpp"
#include "platform_wm.hpp"

#include "time_util.hpp"
#include "render.hpp"

#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/transform.hpp>

enum MouseButtonMask {
    MOUSE_LEFT = 0x01,
    MOUSE_MIDDLE = 0x02,
    MOUSE_RIGHT = 0x04
};

static const glm::vec3 GLOBAL_UP(0, 0, 1);

glm::mat4 camera_view(const Camera& cam) {
    return  glm::lookAt(cam.eye,
                        cam.target,
                        GLOBAL_UP);
}

glm::mat4 camera_proj(const Camera& cam) {
    return glm::perspective(cam.fov,
                            cam.aspect,
                            cam.near,
                            cam.far);
}

static void update_orbit_camera(Camera& cam, float lat, float lng, float r, glm::vec3 center) {
    cam.target = center;
    cam.eye = center
        + r * glm::vec3(std::cos(lat) * std::sin(lng),
                        std::cos(lat) * std::cos(lng),
                        std::sin(lat));
}

bool intersect(const Mesh& mesh,
               const glm::vec3& ray_o,
               const glm::vec3& ray_d,
               float* t_out) {
    bool rval = false;
    float tmax;

    for (size_t i = 0; i < mesh.indices.size() / 3; i++) {
        glm::vec3 edge1 = mesh.positions[mesh.indices[i * 3 + 1]] - mesh.positions[mesh.indices[i * 3]];
        glm::vec3 edge2 = mesh.positions[mesh.indices[i * 3 + 2]] - mesh.positions[mesh.indices[i * 3]];
        glm::vec3 h = glm::cross(ray_d, edge2);
        float a = glm::dot(edge1, h);
        if (a == 0.0f)
            continue;    // ray parallel to the triangle

        float f = 1.0f / a;
        glm::vec3 s = ray_o - mesh.positions[mesh.indices[i * 3]];
        float u = f * glm::dot(s, h);
        if (u < 0.0f || u > 1.0f)
            continue;
        glm::vec3 q = glm::cross(s, edge1);
        float v = f * glm::dot(ray_d, q);
        if (v < 0.0f || u + v > 1.0f)
            continue;

        // On calcule t pour savoir ou le point d'intersection se situe sur la ligne.
        float t = f * glm::dot(edge2, q);
        if (t > 0.0f && (!rval || t < tmax)) {
            if (t_out) {
                *t_out = t;
            }
            tmax = t;
            rval = true;
        } 
    }

    return rval;
}

int main(int argc, char** argv) {
    GPUContext gpu;
    gpu_init(gpu);

    WMContext wm;
    wm_init(wm);
    
    GraphicsContext gfx;
    graphics_init(&gpu, &wm, gfx);
    
    VulkanComputeContext compute;
    compute_init(&gpu, compute);

    std::vector<Vertex> vertices = {
        {{-.5f, -.5f, 0}, {0, 0}, {0, 0, 1}},
        {{.5f, -.5f, 0}, {0, 0}, {0, 0, 1}},
        {{.5f, .5f, 0}, {0, 0}, {0, 0, 1}},     
        {{-.5f, .5f, 0}, {0, 0}, {0, 0, 1}},
    };

    std::vector<uint32_t> indices = {
        0, 1, 2,
        0, 2, 3,
    };
    assert(indices.size() % 3 == 0);

    Mesh suzanne = load_obj_mesh("suzanne_smooth.obj");
    GPUMesh suzanne_gpu = gpu_mesh_allocate(gpu, suzanne.positions.size(), suzanne.indices.size() / 3);
    gpu_mesh_upload(gpu, suzanne_gpu, suzanne);
    
    GPUBuffer<Vertex> base_vertices = suzanne_gpu.vertex_buffer;
    suzanne_gpu.vertex_buffer = gpu_buffer_allocate<Vertex>(gpu,
                                                            COMPUTE | GRAPHICS | VERTEX_BUFFER | STORAGE_BUFFER,
                                                            suzanne_gpu.vertex_buffer.count);

    // test_compute(cctx, suzanne);

    glm::vec3* suzanne_colors = gpu_buffer_map(gpu, suzanne_gpu.color_buffer);

    for (uint32_t i = 0; i < suzanne.positions.size(); i++) {
        suzanne_colors[i].x = suzanne.uvs[i].x;
        suzanne_colors[i].y = suzanne.uvs[i].y;
        suzanne_colors[i].z = 0.0f;
    }

    gpu_buffer_unmap(gpu, suzanne_gpu.color_buffer);
    
    
    auto kernel =
        compute_kernel_create<GPUBuffer<Vertex>, GPUBuffer<Vertex>, GPUBuffer<float>>(compute, "shaders/wiggle.comp.spv");

    std::vector<GPUModel> models = {
        {&suzanne_gpu, glm::translate(glm::vec3(0, 0, 0))},
    };
    
    float orbit_speed = 2.0f;
    float zoom_speed = .1f;
    float cam_lat = 0.0f;
    float cam_long = 0.0f;
    float cam_r = 3.0f;
    glm::vec3 cam_center(0.0f, 0.0f, 0.0f);
    
    Camera cam;
    cam.aspect = static_cast<float>(gfx.swapchain.extent.width) / static_cast<float>(gfx.swapchain.extent.height);
    update_orbit_camera(cam, cam_lat, cam_long, cam_r, cam_center);
    cam.fov = glm::radians(60.0f);
    cam.near = 0.01f;
    cam.far = 100.0f;
    
    uint32_t current_frame = 0;
    bool should_close = false;

    double t0 = now_seconds();
    double compute_acc = 0.0;
    
    uint32_t mouse_button_down = 0;

    glm::vec2 mouse_position;
    glm::vec2 drag_start;

    
    while (true) {
        while (XPending(wm.display)) {
            XEvent event;
            XNextEvent(wm.display, &event);

            switch (event.type) {
            case ClientMessage:
                if (event.xclient.message_type == XInternAtom(wm.display, "WM_PROTOCOLS", true)
                    && event.xclient.data.l[0] == XInternAtom(wm.display, "WM_DELETE_WINDOW", true)) {
                    should_close = true;
                }
                break;
            case MotionNotify: {
                mouse_position.x = 2.0f * event.xmotion.x / gfx.swapchain.extent.width - 1.0f;
                mouse_position.y = 1.0f - 2.0f * event.xmotion.y / gfx.swapchain.extent.height;

                if (mouse_button_down & MOUSE_MIDDLE) {
                    glm::vec2 drag = mouse_position - drag_start;
                    cam_long += drag.x * orbit_speed;
                    cam_lat -= drag.y * orbit_speed;
                    drag_start = mouse_position;

                    float deg90 = M_PI * .5f - 1.0e-6f;

                    if (cam_lat > deg90) {
                        cam_lat = deg90;
                    }
                    if (cam_lat < -deg90) {
                        cam_lat = -deg90;
                    }
                }

                break;
            }
            case ButtonPress:
                switch (event.xbutton.button) {
                case Button1:
                    mouse_button_down |= MOUSE_LEFT;
                    break;
                case Button2:
                    mouse_button_down |= MOUSE_MIDDLE;
                    drag_start = mouse_position;
                    break;
                case Button3:
                    mouse_button_down |= MOUSE_RIGHT;
                    break;
                case Button4: // Scroll up
                    cam_r /= 1.0f + zoom_speed;
                    if (cam_r > cam.far) {
                        cam_r = cam.far;
                    }
                    break;
                case Button5: // Scroll down
                    cam_r *= 1.0f + zoom_speed;
                    if (cam_r < cam.near) {
                        cam_r = cam.near;
                    }
                    break;
                }
                break;
            case ButtonRelease:
                switch (event.xbutton.button) {
                case Button1:
                    mouse_button_down &= ~MOUSE_LEFT;
                    break;
                case Button2:
                    mouse_button_down &= ~MOUSE_MIDDLE;
                    break;
                case Button3:
                    mouse_button_down &= ~MOUSE_RIGHT;
                    break;
                }
                break;
            case KeyPress:
                break;
            case KeyRelease:
                break;
            default:
                std::cerr << "Unhandled event type " << event.type << "\n";
            }
        }
        if (should_close) {
            break;
        }

        gpu_mesh_upload(gpu, suzanne_gpu, suzanne);

        double t1 = now_seconds();
        float elapsed = static_cast<float>(t1 - t0);
        float freq = .5f;

        GPUBuffer<float> t_buf = gpu_buffer_allocate<float>(gpu, COMPUTE | STORAGE_BUFFER, 1);
        gpu_buffer_upload(gpu, t_buf, &elapsed, 0, 1);
        
        double compute_before = now_seconds();
        compute_kernel_invoke(compute,
                              kernel,
                              suzanne_gpu.vertex_buffer.count / 32 + 1, 1, 1,
                              base_vertices,
                              suzanne_gpu.vertex_buffer,
                              t_buf);
        compute_acc += (now_seconds() - compute_before);
        gpu_buffer_free(gpu, t_buf);

        models[0].transform = glm::scale(glm::vec3(.5f))
            * glm::translate(glm::vec3(std::sin(elapsed), 0, 0))
            * glm::rotate(2.0f * static_cast<float>(M_PI) * freq * elapsed, glm::vec3(0, 0, 1));

        update_orbit_camera(cam, cam_lat, cam_long, cam_r, cam_center);

        GraphicsFrame frame = begin_frame(gfx);
        {
            cam.aspect = static_cast<float>(gfx.swapchain.extent.width)
                / static_cast<float>(gfx.swapchain.extent.height);

            for (const GPUModel &model : models) {
                draw_model(frame, camera_view(cam), camera_proj(cam), model);
            }

            end_frame(gfx, frame);
        }

        current_frame++;
    }

    double t1 = now_seconds();
    double elapsed = (t1 - t0);
    float avg_fps = current_frame / elapsed;
    std::cout << "Average FPS : " << avg_fps << "\n";

    std::cout << "Total elapsed : " << elapsed << ", compute : " << compute_acc << "\n";
    std::cout << "Compute : " << 100.0 * compute_acc / elapsed << "%\n";

    graphics_wait_idle(gfx);

    compute_kernel_destroy(compute, kernel);
    compute_finalize(compute);
    
    gpu_mesh_destroy(gpu, suzanne_gpu);
    gpu_buffer_free(gpu, base_vertices);

    graphics_finalize(gfx);

    wm_finalize(wm);
    
    gpu_finalize(gpu);
}
