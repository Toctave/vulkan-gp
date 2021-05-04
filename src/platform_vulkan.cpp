#include "platform.hpp"

#include <cstring>
#include <fstream>
#include <iostream>
#include <cassert>

VkBufferUsageFlags to_vulkan_flags(GPUBufferUsage usage) {
    switch (usage) {
    case INDEX_BUFFER:
        return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    case VERTEX_BUFFER:
        return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    default:
        assert(false && "Unknown buffer usage flags passed to to_vulkan_flags");
        return 0;
    }
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

static VulkanImage allocate_image(VkDevice device, VkPhysicalDevice physical_device, VkFormat format, VkImageUsageFlags usage, uint32_t width, uint32_t height) {
    VulkanImage image;

    VkImageCreateInfo image_ci{};
    image_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_ci.imageType = VK_IMAGE_TYPE_2D;
    image_ci.arrayLayers = 1;
    image_ci.format = format;
    image_ci.extent.width = width;
    image_ci.extent.height = height;
    image_ci.extent.depth = 1;
    image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
    image_ci.usage = usage;
    image_ci.mipLevels = 1;
    image_ci.arrayLayers = 1;

    if (vkCreateImage(device, &image_ci, nullptr, &image.handle) != VK_SUCCESS) {
        throw std::runtime_error("Could not create image.");
    }

    VkMemoryRequirements image_req;
    vkGetImageMemoryRequirements(device, image.handle, &image_req);

    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &props);

    int32_t image_mem_type = find_memory_type(&props, image_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkMemoryAllocateInfo image_memory_ai{};
    image_memory_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    image_memory_ai.allocationSize = image_req.size;
    image_memory_ai.memoryTypeIndex = image_mem_type;
    
    if (vkAllocateMemory(device, &image_memory_ai, nullptr, &image.memory) != VK_SUCCESS) {
        throw std::runtime_error("Could not allocate image memory.");
    }
    
    vkBindImageMemory(device, image.handle, image.memory, 0);

    VkImageViewCreateInfo view_ci{};
    view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_ci.image = image.handle;
    view_ci.subresourceRange.baseArrayLayer = 0;
    view_ci.subresourceRange.layerCount = 1;
    view_ci.subresourceRange.baseMipLevel = 0;
    view_ci.subresourceRange.levelCount = 1;
    view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    view_ci.format = format;
    view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;

    if (vkCreateImageView(device, &view_ci, nullptr, &image.view) != VK_SUCCESS) {
        throw std::runtime_error("Could not create image view.");
    }

    return image;
}

static Swapchain create_swapchain(VkDevice device,
                                  VkPhysicalDevice physical_device,
                                  VkSurfaceKHR surface,
                                  VkSwapchainKHR old_swapchain_handle = VK_NULL_HANDLE) {
    Swapchain swapchain;
    
    // Pick a format
    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr);

    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, formats.data());

    size_t format_index = 0;
    for (size_t i = 0; i < formats.size(); i++) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            format_index = i;
        }
    }
    swapchain.format = formats[format_index];

    // Find out extent
    VkSurfaceCapabilitiesKHR surface_capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_capabilities);
    swapchain.extent = surface_capabilities.currentExtent;
    
    const VkFormat depth_format = VK_FORMAT_D32_SFLOAT;
    swapchain.depth_image = allocate_image(device,
                                           physical_device,
                                           depth_format,
                                           VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                           swapchain.extent.width,
                                           swapchain.extent.height);
    
    VkAttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_ref{};
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    std::vector<VkSubpassDescription> subpasses;
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = nullptr;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    subpass.pDepthStencilAttachment = &depth_attachment_ref;
    subpass.preserveAttachmentCount = 0;

    subpasses.push_back(subpass);
    
    std::vector<VkAttachmentDescription> attachments;
    VkAttachmentDescription color_attachment{};
    color_attachment.format = swapchain.format.format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    attachments.push_back(color_attachment);

    VkAttachmentDescription depth_attachment{};
    depth_attachment.format = depth_format;
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    attachments.push_back(depth_attachment);    

    VkRenderPassCreateInfo render_pass_ci{};
    render_pass_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_ci.attachmentCount = attachments.size();
    render_pass_ci.pAttachments = attachments.data();
    render_pass_ci.subpassCount = subpasses.size();
    render_pass_ci.pSubpasses = subpasses.data();

    if (vkCreateRenderPass(device, &render_pass_ci, nullptr, &swapchain.render_pass) != VK_SUCCESS) {
        throw std::runtime_error("Could not create render pass");
    }

    uint32_t present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr);
    std::vector<VkPresentModeKHR> present_modes(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, present_modes.data());

    std::vector<VkPresentModeKHR> present_mode_preferences = {
        VK_PRESENT_MODE_IMMEDIATE_KHR,
        VK_PRESENT_MODE_FIFO_KHR,
        VK_PRESENT_MODE_FIFO_RELAXED_KHR,
        VK_PRESENT_MODE_MAILBOX_KHR,
        VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR,
        VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR,
    };

    VkPresentModeKHR chosen_present_mode = VK_PRESENT_MODE_MAX_ENUM_KHR;
    for (VkPresentModeKHR mode : present_mode_preferences) {
        for (VkPresentModeKHR available : present_modes) {
            if (mode == available) {
                chosen_present_mode = mode;
                break;
            }
        }
        if (chosen_present_mode != VK_PRESENT_MODE_MAX_ENUM_KHR) {
            break;
        }
    }
    if (chosen_present_mode == VK_PRESENT_MODE_MAX_ENUM_KHR) {
        throw std::runtime_error("Could not find a suitable present mode.");
    }

    VkSwapchainCreateInfoKHR swapchain_ci{};
    swapchain_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_ci.surface = surface;
    swapchain_ci.minImageCount = surface_capabilities.minImageCount;
    swapchain_ci.imageFormat = swapchain.format.format;
    swapchain_ci.imageColorSpace = swapchain.format.colorSpace;
    swapchain_ci.imageExtent = swapchain.extent;
    swapchain_ci.imageArrayLayers = 1;
    swapchain_ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_ci.preTransform = surface_capabilities.currentTransform;
    swapchain_ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_ci.presentMode = chosen_present_mode;
    swapchain_ci.clipped = VK_TRUE;
    swapchain_ci.oldSwapchain = old_swapchain_handle;

    if (vkCreateSwapchainKHR(device, &swapchain_ci, nullptr, &swapchain.handle) != VK_SUCCESS) {
        throw std::runtime_error("Could not create swapchain.");
    }

    uint32_t swapchain_image_count;
    if (vkGetSwapchainImagesKHR(device, swapchain.handle, &swapchain_image_count, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("Could not query swapchain image count.");
    }

    swapchain.images.resize(swapchain_image_count);
    if (vkGetSwapchainImagesKHR(device, swapchain.handle, &swapchain_image_count, swapchain.images.data()) != VK_SUCCESS) {
        throw std::runtime_error("Could not query swapchain images.");
    }

    swapchain.image_views.resize(swapchain_image_count);
    for (uint32_t i = 0; i < swapchain_image_count; i++) {
        VkImageViewCreateInfo fb_color_ci{};
        fb_color_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        fb_color_ci.image = swapchain.images[i];
        fb_color_ci.subresourceRange.baseArrayLayer = 0;
        fb_color_ci.subresourceRange.layerCount = 1;
        fb_color_ci.subresourceRange.baseMipLevel = 0;
        fb_color_ci.subresourceRange.levelCount = 1;
        fb_color_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        fb_color_ci.format = swapchain.format.format;
        fb_color_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;

        if (vkCreateImageView(device, &fb_color_ci, nullptr, &swapchain.image_views[i]) != VK_SUCCESS) {
            throw std::runtime_error("Could not create swapchain image view.");
        }
    }

    swapchain.frames.resize(swapchain_image_count, -1);

    swapchain.framebuffers.resize(swapchain.images.size());
    for (size_t i = 0; i < swapchain.images.size(); i++) {
        std::vector<VkImageView> fb_attachments = {swapchain.image_views[i], swapchain.depth_image.view};
        VkFramebufferCreateInfo framebuffer_ci{};
        framebuffer_ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_ci.width = swapchain.extent.width;
        framebuffer_ci.height = swapchain.extent.height;
        framebuffer_ci.attachmentCount = fb_attachments.size();
        framebuffer_ci.pAttachments = fb_attachments.data();
        framebuffer_ci.renderPass = swapchain.render_pass;
        framebuffer_ci.layers = 1;
    
        if (vkCreateFramebuffer(device, &framebuffer_ci, nullptr, &swapchain.framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Could not create framebuffer.");
        }
    }

    return swapchain;
}

static void vulkan_init(VulkanContext& ctx) {
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
        "VK_KHR_maintenance1"
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

    int family_index = -1;
    for (int i = 0; i < families.size(); i++) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            family_index = i;
            break;
        }
    }

    if (family_index < 0) {
        throw std::runtime_error("GPU does not support graphics.");
    }
    ctx.queue_family_index = family_index;

    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_ci{};
    queue_ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_ci.queueFamilyIndex = family_index;
    queue_ci.queueCount = 1;
    queue_ci.pQueuePriorities = &priority;

    VkDeviceCreateInfo device_ci{};
    device_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_ci.queueCreateInfoCount = 1;
    device_ci.pQueueCreateInfos = &queue_ci;
    device_ci.ppEnabledExtensionNames = required_device_extensions.data();
    device_ci.enabledExtensionCount = required_device_extensions.size();
    
    if (vkCreateDevice(ctx.physical_device, &device_ci, nullptr, &ctx.device) != VK_SUCCESS) {
        throw std::runtime_error("Could not create Vulkan device.");
    }
}

static void window_init(GraphicsContext& ctx) {
    wm_init(ctx.wm);
    
    XSelectInput(ctx.wm.display,
                 ctx.wm.window,
                 ButtonPressMask
                 | ButtonReleaseMask
                 | KeyPressMask
                 | KeyReleaseMask
                 | PointerMotionMask);
    
    XStoreName(ctx.wm.display, ctx.wm.window, "Vulkan-gp");

    Atom protocol = XInternAtom(ctx.wm.display, "WM_DELETE_WINDOW", false);
    XSetWMProtocols(ctx.wm.display, ctx.wm.window, &protocol, 1);

    XMapRaised(ctx.wm.display, ctx.wm.window);

    VkXlibSurfaceCreateInfoKHR surface_ci{};
    surface_ci.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    surface_ci.dpy = ctx.wm.display;
    surface_ci.window = ctx.wm.window;

    if (vkCreateXlibSurfaceKHR(ctx.instance, &surface_ci, nullptr, &ctx.surface) != VK_SUCCESS) {
        throw std::runtime_error("Could not create X11 surface.");
    }

    VkBool32 surface_support;
    vkGetPhysicalDeviceSurfaceSupportKHR(ctx.physical_device, ctx.queue_family_index, ctx.surface, &surface_support);
    if (!surface_support) {
        throw std::runtime_error("Surface does not support presentation.");
    }

    VkCommandPoolCreateInfo command_pool_ci{};
    command_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    command_pool_ci.queueFamilyIndex = ctx.queue_family_index;
	
    if (vkCreateCommandPool(ctx.device, &command_pool_ci, nullptr, &ctx.command_pool) != VK_SUCCESS) {
        throw std::runtime_error("Could not create command pool.");
    }

    ctx.swapchain = create_swapchain(ctx.device, ctx.physical_device, ctx.surface);

    ctx.command_buffers.resize(ctx.swapchain.images.size());
    
    VkCommandBufferAllocateInfo command_buffer_ai{};
    command_buffer_ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_ai.commandPool = ctx.command_pool;
    command_buffer_ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_ai.commandBufferCount = ctx.command_buffers.size();

    vkAllocateCommandBuffers(ctx.device, &command_buffer_ai, ctx.command_buffers.data());

    VkSemaphoreCreateInfo semaphore_ci{};
    semaphore_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fence_ci{};
    fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(ctx.device, &semaphore_ci, nullptr, &ctx.swapchain_image_ready[i]) != VK_SUCCESS
            || vkCreateSemaphore(ctx.device, &semaphore_ci, nullptr, &ctx.swapchain_submit_done[i]) != VK_SUCCESS
            || vkCreateFence(ctx.device, &fence_ci, nullptr, &ctx.frame_finished[i]) != VK_SUCCESS) {
            throw std::runtime_error("Could not create sync objects for frame.");
        }
    }
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

static VkShaderModule create_shader_module(VkDevice device, const std::string& file_name) {
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

static void destroy_image(VkDevice device, VulkanImage& image) {
    vkDestroyImageView(device, image.view, nullptr);
    vkFreeMemory(device, image.memory, nullptr);
    vkDestroyImage(device, image.handle, nullptr);
}

static void pipeline_init(GraphicsContext& ctx) {
    VkPipelineShaderStageCreateInfo vertex_shader_stage{};
    vertex_shader_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertex_shader_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertex_shader_stage.module = create_shader_module(ctx.device, "shaders/triangle.vert.spv");
    vertex_shader_stage.pName = "main";
    ctx.shaders.push_back(vertex_shader_stage.module);

    VkPipelineShaderStageCreateInfo fragment_shader_stage{};
    fragment_shader_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragment_shader_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragment_shader_stage.module = create_shader_module(ctx.device, "shaders/triangle.frag.spv");
    fragment_shader_stage.pName = "main";
    ctx.shaders.push_back(fragment_shader_stage.module);

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {
        vertex_shader_stage,
        fragment_shader_stage,
    };

    std::vector<VkVertexInputAttributeDescription> vertex_attributes = {
        {
            // position :
            0,                          // location
            0,                          // binding
            VK_FORMAT_R32G32B32_SFLOAT, // format
            0,                          // offset
        },
        {
            // UV :
            1,                       // location
            1,                       // binding
            VK_FORMAT_R32G32_SFLOAT, // format
            0,                       // offset
        },
    };

    std::vector<VkVertexInputBindingDescription> vertex_bindings = {
        {
            // position :
            0,                           // binding
            sizeof(float) * 3,           // stride
            VK_VERTEX_INPUT_RATE_VERTEX, // input rate
        },
        {
            // UV :
            1,                           // binding
            sizeof(float) * 2,           // stride
            VK_VERTEX_INPUT_RATE_VERTEX, // input rate
        },
    };

    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = vertex_bindings.size();
    vertex_input.pVertexBindingDescriptions = vertex_bindings.data();
    vertex_input.vertexAttributeDescriptionCount = vertex_attributes.size();
    vertex_input.pVertexAttributeDescriptions = vertex_attributes.data();

    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterization{};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.depthClampEnable = false;
    rasterization.rasterizerDiscardEnable = false;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = true;
    depth_stencil.depthWriteEnable = true;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState color_blend_attachment_state{};
    color_blend_attachment_state.blendEnable = true;
    color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_state.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT
        | VK_COLOR_COMPONENT_G_BIT
        | VK_COLOR_COMPONENT_B_BIT
        | VK_COLOR_COMPONENT_A_BIT;
    
    VkPipelineColorBlendStateCreateInfo color_blend{};
    color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend.attachmentCount = 1;
    color_blend.pAttachments = &color_blend_attachment_state;
    color_blend.logicOpEnable = false;

    std::vector<VkDynamicState> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamic_state_ci{};
    dynamic_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_ci.dynamicStateCount = dynamic_states.size();
    dynamic_state_ci.pDynamicStates = dynamic_states.data();

    std::vector<VkDescriptorSetLayout> descriptor_set_layouts;

    std::vector<VkPushConstantRange> push_constants;
    VkPushConstantRange push_mvp;
    push_mvp.offset = 0;
    push_mvp.size = sizeof(float) * 4 * 4;
    push_mvp.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    
    push_constants.push_back(push_mvp);

    VkPipelineLayoutCreateInfo layout_ci{};
    layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_ci.setLayoutCount = descriptor_set_layouts.size();
    layout_ci.pSetLayouts = descriptor_set_layouts.data();
    layout_ci.pushConstantRangeCount = push_constants.size();
    layout_ci.pPushConstantRanges = push_constants.data();

    if (vkCreatePipelineLayout(ctx.device, &layout_ci, nullptr, &ctx.pipeline_layout) != VK_SUCCESS) {
        throw std::runtime_error("Could not create pipeline layout.");
    }
    
    VkGraphicsPipelineCreateInfo pipeline_ci{};
    pipeline_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_ci.stageCount = shader_stages.size();
    pipeline_ci.pStages = shader_stages.data();
    pipeline_ci.pVertexInputState = &vertex_input;
    pipeline_ci.pInputAssemblyState = &input_assembly;
    pipeline_ci.pViewportState = &viewport_state;
    pipeline_ci.pRasterizationState = &rasterization;
    pipeline_ci.pMultisampleState = &multisample;
    pipeline_ci.pDepthStencilState = &depth_stencil;
    pipeline_ci.pColorBlendState = &color_blend;
    pipeline_ci.pDynamicState = &dynamic_state_ci;
    pipeline_ci.layout = ctx.pipeline_layout;
    pipeline_ci.renderPass = ctx.swapchain.render_pass;

    if (vkCreateGraphicsPipelines(ctx.device, VK_NULL_HANDLE, 1, &pipeline_ci, nullptr, &ctx.pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Could not create graphics pipeline.");
    }
}

static void destroy_swapchain(VkDevice device, Swapchain& swapchain) {
    vkDestroyRenderPass(device, swapchain.render_pass, nullptr);
    
    for (size_t i = 0; i < swapchain.images.size(); i++) {
        vkDestroyFramebuffer(device, swapchain.framebuffers[i], nullptr);
        vkDestroyImageView(device, swapchain.image_views[i], nullptr);
    }
    destroy_image(device, swapchain.depth_image);

    vkDestroySwapchainKHR(device, swapchain.handle, nullptr);
}

void graphics_finalize(GraphicsContext& ctx) {
    vkDeviceWaitIdle(ctx.device);
    
    // Pipeline :
    for (VkShaderModule shader : ctx.shaders) {
        vkDestroyShaderModule(ctx.device, shader, nullptr);
    }
    vkDestroyPipeline(ctx.device, ctx.pipeline, nullptr);
    vkDestroyPipelineLayout(ctx.device, ctx.pipeline_layout, nullptr);

    // Window :
    destroy_swapchain(ctx.device, ctx.swapchain);
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyFence(ctx.device, ctx.frame_finished[i], nullptr);
        vkDestroySemaphore(ctx.device, ctx.swapchain_image_ready[i], nullptr);
        vkDestroySemaphore(ctx.device, ctx.swapchain_submit_done[i], nullptr);
    }
    
    vkDestroyCommandPool(ctx.device, ctx.command_pool, nullptr);
    vkDestroySurfaceKHR(ctx.instance, ctx.surface, nullptr);

    XDestroyWindow(ctx.wm.display, ctx.wm.window);
    XCloseDisplay(ctx.wm.display);

    // Vulkan :
    vkDestroyDevice(ctx.device, nullptr);
    vkDestroyInstance(ctx.instance, nullptr);
}

void recreate_swapchain(GraphicsContext& ctx) {
    vkWaitForFences(ctx.device, MAX_FRAMES_IN_FLIGHT, ctx.frame_finished, VK_TRUE, UINT64_MAX);
    Swapchain new_swapchain = create_swapchain(ctx.device, ctx.physical_device, ctx.surface, ctx.swapchain.handle);
    destroy_swapchain(ctx.device, ctx.swapchain);
    ctx.swapchain = new_swapchain;
}

void graphics_init(VulkanContext& ctx) {
    vulkan_init(ctx);
    window_init(ctx);
    pipeline_init(ctx);
    ctx.next_frame = 0;
}

void graphics_wait_idle(const GraphicsContext &ctx) {
    vkDeviceWaitIdle(ctx.device);
}