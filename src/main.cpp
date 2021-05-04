#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <cmath>

#include <string.h>

#include "time_util.hpp"
#include "platform.hpp"
#include "render.hpp"

#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

enum MouseButtonMask {
    MOUSE_LEFT = 0x01,
    MOUSE_MIDDLE = 0x02,
    MOUSE_RIGHT = 0x04
};

static const glm::vec3 GLOBAL_UP(0, 0, 1);

glm::mat4 camera_viewproj(const Camera& cam) {
    glm::mat4 view = glm::lookAt(
                                 cam.eye,
                                 cam.target,
                                 GLOBAL_UP);

    glm::mat4 proj = glm::perspective(
                                      cam.fov,
                                      cam.aspect,
                                      cam.near,
                                      cam.far);

    return proj * view;
}

static void update_orbit_camera(Camera& cam, float lat, float lng, float r, glm::vec3 center) {
    cam.target = center;
    cam.eye = center
        + r * glm::vec3(std::cos(lat) * std::sin(lng),
                        std::cos(lat) * std::cos(lng),
                        std::sin(lat));
}

int main(int argc, char** argv) {
    GraphicsContext ctx;

    graphics_init(ctx);

    std::vector<glm::vec3> vertex_positions = {
        {-.5f, -.5f, 0}, 
        {.5f, -.5f, 0},           
        {.5f, .5f, 0},            
        {-.5f, .5f, 0},           
    };

    std::vector<glm::vec2> vertex_uvs = {
        {0, 0},
        {1, 0},
        {1, 1},
        {0, 1},
    };
    
    assert(vertex_uvs.size() == vertex_positions.size());

    std::vector<uint32_t> vertex_indices = {
        0, 1, 2,
        0, 2, 3,
    };
    assert(vertex_indices.size() % 3 == 0);

    Mesh plane = create_mesh(ctx,
                             vertex_positions.size(),
                             vertex_indices.size() / 3,
                             vertex_indices.data(),
                             vertex_positions.data(),
                             vertex_uvs.data());
    
    std::vector<Model> models = {
        {&plane, glm::translate(glm::vec3(0, 0, 0))},
        {&plane, glm::translate(glm::vec3(0, 0, 1))},
    };
    
    float orbit_speed = 2.0f;
    float zoom_speed = .1f;
    float cam_lat = 0.0f;
    float cam_long = 0.0f;
    float cam_r = 3.0f;
    glm::vec3 cam_center(0.0f, 0.0f, 0.0f);
    
    Camera cam;
    cam.aspect = static_cast<float>(ctx.swapchain.extent.width) / static_cast<float>(ctx.swapchain.extent.height);
    update_orbit_camera(cam, cam_lat, cam_long, cam_r, cam_center);
    cam.fov = glm::radians(60.0f);
    cam.near = 0.01f;
    cam.far = 100.0f;

    
    uint32_t current_frame = 0;
    bool should_close = false;

    double t0 = now_seconds();
    
    uint32_t mouse_button_down = 0;

    glm::vec2 mouse_position;
    glm::vec2 drag_start;

    
    while (true) {
        while (XPending(ctx.wm.display)) {
            XEvent event;
            XNextEvent(ctx.wm.display, &event);

            switch (event.type) {
            case ClientMessage:
                if (event.xclient.message_type ==
                    XInternAtom(ctx.wm.display, "WM_PROTOCOLS", true) &&
                    event.xclient.data.l[0] ==
                    XInternAtom(ctx.wm.display, "WM_DELETE_WINDOW", true)) {
                    should_close = true;
                }
                break;
            case MotionNotify: {
                mouse_position.x = 2.0f * event.xmotion.x / ctx.swapchain.extent.width - 1.0f;
                mouse_position.y = 1.0f - 2.0f * event.xmotion.y / ctx.swapchain.extent.height;

                if (mouse_button_down & MOUSE_MIDDLE) {
                    glm::vec2 drag = mouse_position - drag_start;
                    cam_long += drag.x * orbit_speed;
                    cam_lat -= drag.y * orbit_speed;
                    drag_start = mouse_position;
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
                case Button4:
                    cam_r /= 1.0f + zoom_speed;
                    break;
                case Button5:
                    cam_r *= 1.0f + zoom_speed;
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

        double t1 = now_seconds();
        float elapsed = static_cast<float>(t1 - t0);
        float freq = .5f;

        models[0].transform =
            glm::translate(glm::vec3(std::sin(elapsed), 0, 0))
            * glm::rotate(2.0f * static_cast<float>(M_PI) * freq * elapsed, glm::vec3(0, 0, 1))
            * glm::rotate(glm::radians(90.0f), glm::vec3(0, 1, 0));

        update_orbit_camera(cam, cam_lat, cam_long, cam_r, cam_center);
        
        // render_frame(ctx, current_frame, models, cam);
        GraphicsFrame frame = begin_frame(ctx);

        cam.aspect = static_cast<float>(ctx.swapchain.extent.width) / static_cast<float>(ctx.swapchain.extent.height);
        draw_model(frame, camera_viewproj(cam), models[0]);

        end_frame(ctx, frame);
        
        current_frame++;
    }

    double t1 = now_seconds();
    float elapsed = static_cast<float>(t1 - t0);
    float avg_fps = current_frame / elapsed;
    std::cout << "Average FPS : " << avg_fps << "\n";

    graphics_wait_idle(ctx);

    destroy_mesh(ctx, plane);

    graphics_finalize(ctx);
}
