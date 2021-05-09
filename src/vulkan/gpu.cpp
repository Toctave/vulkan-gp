#include "gpu.hpp"

#include <cstring>

#include "../memory_util.hpp"

void gpu_init(VulkanContext& ctx) {
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

    std::vector<VkLayerProperties> layer_properties(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, layer_properties.data());

    std::vector<const char*> required_layers = {
        "VK_LAYER_KHRONOS_validation"
    };

    for (const char* name : required_layers) {
        bool found = false;
        for (const auto& prop : layer_properties) {
            if (!strcmp(prop.layerName, name)) {
                found = true;
                break;
            }
        }
        if (!found) {
            throw std::runtime_error("Could not find required layer " + std::string(name) + ".");
        }
    }

    uint32_t extension_count;
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
    
    std::vector<VkExtensionProperties> extension_properties(extension_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extension_properties.data());

    std::vector<const char*> required_extensions = {
        "VK_KHR_surface",
        "VK_KHR_xlib_surface",
    };

    for (const char* name : required_extensions) {
        bool found = false;
        for (const auto& prop : extension_properties) {
            if (!strcmp(prop.extensionName, name)) {
                found = true;
                break;
            }
        }
        if (!found) {
            throw std::runtime_error("Could not find required extension " + std::string(name) + ".");
        }
    }

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instance_ci{};
    instance_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_ci.enabledLayerCount = required_layers.size();
    instance_ci.ppEnabledLayerNames = required_layers.data();
    instance_ci.enabledExtensionCount = required_extensions.size();
    instance_ci.ppEnabledExtensionNames = required_extensions.data();
    instance_ci.pApplicationInfo = &app_info;

    if (vkCreateInstance(&instance_ci, nullptr, &ctx.instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create vulkan instance.");
    }

    uint32_t physical_device_count;
    vkEnumeratePhysicalDevices(ctx.instance, &physical_device_count, nullptr);

    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    vkEnumeratePhysicalDevices(ctx.instance, &physical_device_count, physical_devices.data());

    std::vector<const char*> required_device_extensions = {
        "VK_KHR_swapchain",
        "VK_KHR_maintenance1",
        "VK_KHR_shader_non_semantic_info",
    };
    
    ctx.physical_device = VK_NULL_HANDLE;
    for (const auto& candidate : physical_devices) {
        uint32_t device_extension_count;
        vkEnumerateDeviceExtensionProperties(candidate, nullptr, &device_extension_count, nullptr);
    
        std::vector<VkExtensionProperties> device_extension_properties(device_extension_count);
        vkEnumerateDeviceExtensionProperties(candidate, nullptr, &device_extension_count, device_extension_properties.data());

        bool all_found = true;
        for (const char* name : required_device_extensions) {
            bool found = false;
            for (const auto& prop : device_extension_properties) {
                if (!strcmp(prop.extensionName, name)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                all_found = false;
                break;
            }
        }
        if (!all_found) {
            continue;
        }

        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(candidate, &properties);

        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            ctx.physical_device = candidate;
            break;
        }
    }

    if (ctx.physical_device == VK_NULL_HANDLE) {
        throw std::runtime_error("Could not find a GPU.");
    }

    uint32_t family_count;
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.physical_device, &family_count, nullptr);
    std::vector<VkQueueFamilyProperties> families(family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.physical_device, &family_count, families.data());

    int graphics_idx = -1;
    int compute_idx = -1;
    for (int i = 0; i < families.size(); i++) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_idx = i;
        }
        if (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            compute_idx = i;
        }
    }

    if (graphics_idx < 0) {
        throw std::runtime_error("GPU does not support graphics.");
    }
    if (compute_idx < 0) {
        throw std::runtime_error("GPU does not support compute.");
    }
    ctx.graphics_queue_idx = graphics_idx;
    ctx.compute_queue_idx = compute_idx;

    float priority = 1.0f;
    VkDeviceQueueCreateInfo graphics_queue_ci{};
    graphics_queue_ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphics_queue_ci.queueFamilyIndex = graphics_idx;
    graphics_queue_ci.queueCount = 1;
    graphics_queue_ci.pQueuePriorities = &priority;

    VkDeviceQueueCreateInfo compute_queue_ci{};
    compute_queue_ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    compute_queue_ci.queueFamilyIndex = compute_idx;
    compute_queue_ci.queueCount = 1;
    compute_queue_ci.pQueuePriorities = &priority;

    VkDeviceQueueCreateInfo queue_cis[] = {graphics_queue_ci, compute_queue_ci};

    VkDeviceCreateInfo device_ci{};
    device_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_ci.queueCreateInfoCount = ARRAY_SIZE(queue_cis);
    device_ci.pQueueCreateInfos = queue_cis;
    device_ci.ppEnabledExtensionNames = required_device_extensions.data();
    device_ci.enabledExtensionCount = required_device_extensions.size();
    
    if (vkCreateDevice(ctx.physical_device, &device_ci, nullptr, &ctx.device) != VK_SUCCESS) {
        throw std::runtime_error("Could not create Vulkan device.");
    }
}

void gpu_finalize(VulkanContext& ctx) {
    vkDestroyDevice(ctx.device, nullptr);
    vkDestroyInstance(ctx.instance, nullptr);
}


