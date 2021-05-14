#pragma once

#include <string>
#include <vulkan/vulkan.h>

#include "../common/platform.hpp"

int32_t find_memory_type(const VkPhysicalDeviceMemoryProperties* props,
                         uint32_t memory_type_req,
                         VkMemoryPropertyFlagBits properties_req);

VkShaderModule create_shader_module(VkDevice device, const std::string& file_name);

VkBufferUsageFlags to_vulkan_flags(uint32_t usage);

