#pragma once

#include "../common/platform.hpp"
#include "internal.hpp"

#include <vulkan/vulkan.h>

#include <vector>
#include <stdexcept>

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

struct VulkanContext {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;

    uint32_t graphics_queue_idx;
    uint32_t compute_queue_idx;
};

template<typename T>
VulkanBuffer<T> gpu_buffer_allocate(const VulkanContext& vk,
                                    uint32_t usage,
                                    size_t count) {
    VulkanBuffer<T> buf;
    buf.count = count;

    std::vector<uint32_t> family_indices;
    if (usage & GRAPHICS) {
        family_indices.push_back(vk.graphics_queue_idx);
    }
    if (usage & COMPUTE) {
        family_indices.push_back(vk.compute_queue_idx);
    }

    VkBufferCreateInfo buffer_ci{};
    buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_ci.size = count * sizeof(T);
    buffer_ci.usage = to_vulkan_flags(usage);
    buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer_ci.queueFamilyIndexCount = family_indices.size();
    buffer_ci.pQueueFamilyIndices = family_indices.data();

    if (vkCreateBuffer(vk.device, &buffer_ci, nullptr, &buf.handle) != VK_SUCCESS) {
        throw std::runtime_error("Could not create buffer.");
    }

    VkMemoryRequirements buffer_memory_requirements;
    vkGetBufferMemoryRequirements(vk.device, buf.handle, &buffer_memory_requirements);

    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(vk.physical_device, &memory_properties);

    VkMemoryAllocateInfo buffer_memory_ai{};
    buffer_memory_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    buffer_memory_ai.allocationSize = buffer_memory_requirements.size;
    buffer_memory_ai.memoryTypeIndex =
        find_memory_type(&memory_properties,
                         buffer_memory_requirements.memoryTypeBits,
                         (VkMemoryPropertyFlagBits) (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                                                     | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    
    if (vkAllocateMemory(vk.device, &buffer_memory_ai, nullptr, &buf.memory) != VK_SUCCESS) {
        throw std::runtime_error("Could not allocate buffer memory.");
    }

    vkBindBufferMemory(vk.device, buf.handle, buf.memory, 0);

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
