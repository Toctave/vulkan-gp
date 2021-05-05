#pragma once

#include "platform_wm.hpp"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include <vector>
#include <stdexcept>

#define MAX_FRAMES_IN_FLIGHT 3

template<typename T>
struct VulkanBuffer {
    size_t count;
    VkBuffer handle;
    VkDeviceMemory memory;
};

struct VulkanImage {
    VkImage handle;
    VkImageView view;
    VkDeviceMemory memory;
};

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

struct VulkanContext {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    uint32_t queue_family_index;
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

    WMContext wm;
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

int32_t find_memory_type(const VkPhysicalDeviceMemoryProperties* props,
                         uint32_t memory_type_req,
                         VkMemoryPropertyFlagBits properties_req);
void recreate_swapchain(VulkanContext& ctx);

VkBufferUsageFlags to_vulkan_flags(GPUBufferUsage usage);

template<typename T>
VulkanBuffer<T> gpu_buffer_allocate(const VulkanContext& ctx, GPUBufferUsage usage, size_t count) {
    VulkanBuffer<T> buf;
    buf.count = count;

    VkBufferCreateInfo buffer_ci{};
    buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_ci.size = count * sizeof(T);
    buffer_ci.usage = to_vulkan_flags(usage);
    buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer_ci.queueFamilyIndexCount = 1;
    buffer_ci.pQueueFamilyIndices = &ctx.queue_family_index;

    if (vkCreateBuffer(ctx.device, &buffer_ci, nullptr, &buf.handle) != VK_SUCCESS) {
        throw std::runtime_error("Could not create buffer.");
    }

    VkMemoryRequirements buffer_memory_requirements;
    vkGetBufferMemoryRequirements(ctx.device, buf.handle, &buffer_memory_requirements);

    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(ctx.physical_device, &memory_properties);

    VkMemoryAllocateInfo buffer_memory_ai{};
    buffer_memory_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    buffer_memory_ai.allocationSize = buffer_memory_requirements.size;
    buffer_memory_ai.memoryTypeIndex =
        find_memory_type(&memory_properties,
                         buffer_memory_requirements.memoryTypeBits,
                         (VkMemoryPropertyFlagBits) (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                                                     | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    
    if (vkAllocateMemory(ctx.device, &buffer_memory_ai, nullptr, &buf.memory) != VK_SUCCESS) {
        throw std::runtime_error("Could not allocate buffer memory.");
    }

    vkBindBufferMemory(ctx.device, buf.handle, buf.memory, 0);

    return buf;
}

template <typename T>
T* gpu_buffer_map(const VulkanContext &ctx, VulkanBuffer<T>& buf) {
    T* ptr;

    if (vkMapMemory(ctx.device, buf.memory, 0, buf.count * sizeof(T), 0,
                    reinterpret_cast<void **>(&ptr)) != VK_SUCCESS) {
        throw std::runtime_error("Could not bind buffer");
    }

    return ptr;
}

template <typename T>
void gpu_buffer_unmap(const VulkanContext& ctx, VulkanBuffer<T>& buf) {
    vkUnmapMemory(ctx.device, buf.memory);
}

template <typename T>
static void gpu_buffer_free(const VulkanContext& ctx, VulkanBuffer<T>& buf) {
    vkFreeMemory(ctx.device, buf.memory, nullptr);
    vkDestroyBuffer(ctx.device, buf.handle, nullptr);
}

