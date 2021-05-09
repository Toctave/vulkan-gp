#include "internal.hpp"

#include "../common/platform.hpp"

#include <fstream>
#include <vector>

VkBufferUsageFlags to_vulkan_flags(uint32_t usage) {
    VkBufferUsageFlags rval = 0;
    if (usage & INDEX_BUFFER) {
        rval |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }
    if (usage & VERTEX_BUFFER) {
        rval |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }
    if (usage & UNIFORM_BUFFER) {
        rval |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }
    if (usage & STORAGE_BUFFER) {
        rval |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }

    return rval;
}

int32_t find_memory_type(const VkPhysicalDeviceMemoryProperties* props,
                         uint32_t memory_type_req,
                         VkMemoryPropertyFlagBits properties_req) {
    for (uint32_t i = 0; i < props->memoryTypeCount; i++) {
        uint32_t memory_type_bitmask = (1 << i);
        bool type_ok = memory_type_bitmask & memory_type_req;
	
        bool props_ok = ((props->memoryTypes[i].propertyFlags & properties_req) == properties_req);

        if (type_ok && props_ok) {
            return i;
        }
    }

    return -1;
}

static std::vector<uint32_t> load_spirv_file(const std::string& file_name) {
    std::ifstream file(file_name, std::ios::binary | std::ios::ate);

    if (!file) {
        throw std::runtime_error("Could not open file " + file_name);
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size % 4) {
        throw std::runtime_error("Spir-V code size not a multiple of 4");
    }

    std::vector<uint32_t> buffer(size / 4);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        throw std::runtime_error("Could not read Spir-V code");
    }

    return buffer;
}

VkShaderModule create_shader_module(VkDevice device, const std::string& file_name) {
    std::vector<uint32_t> code = load_spirv_file(file_name);

    VkShaderModule module;
    VkShaderModuleCreateInfo module_ci{};
    module_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    module_ci.codeSize = code.size() * 4;
    module_ci.pCode = code.data();

    if (vkCreateShaderModule(device, &module_ci, nullptr, &module) != VK_SUCCESS) {
        throw std::runtime_error("Could not create shader module from " + file_name + ".");
    }

    return module;
}

