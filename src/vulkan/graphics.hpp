#pragma once

#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "gpu.hpp"

#include "../platform_wm.hpp"

#define MAX_FRAMES_IN_FLIGHT 3

struct Swapchain {
    VkSwapchainKHR handle;
    
    VkExtent2D extent;
    VkSurfaceFormatKHR format;
    
    VkRenderPass render_pass;
    
    VulkanImage depth_image;
    std::vector<VkImage> images;
    std::vector<VkImageView> image_views;
    std::vector<int64_t> frames;
    std::vector<VkFramebuffer> framebuffers;
};

struct VulkanGraphicsContext {
    const VulkanContext* vk;
    const WMContext* wm;
    
    VkCommandPool command_pool;
    std::vector<VkCommandBuffer> command_buffers;

    VkPipelineLayout pipeline_layout;
    std::vector<VkShaderModule> shaders;
    VkPipeline pipeline;

    Swapchain swapchain;
    uint32_t next_frame;

    VkSemaphore swapchain_image_ready[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore swapchain_submit_done[MAX_FRAMES_IN_FLIGHT];
    VkFence frame_finished[MAX_FRAMES_IN_FLIGHT];

    VkSurfaceKHR surface;
};

struct VulkanFrame {
    VkCommandBuffer command_buffer;
    VkPipelineLayout pipeline_layout;
    uint32_t frame_index;
    uint32_t image_index;
};

struct PushMatrices {
    glm::mat4 mvp;
    glm::mat4 model_view;
};

void recreate_swapchain(VulkanGraphicsContext& ctx);
