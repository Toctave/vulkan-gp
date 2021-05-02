#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <cmath>

#include <string.h>

#include <X11/Xlib.h>
#include <vulkan/vulkan.h>

#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Mesh.hpp"

#define MAX_FRAMES_IN_FLIGHT 3

static const glm::vec3 GLOBAL_UP(0, 0, 1);

struct WMContext {
    Display* display;
    Window window;
};

struct GPUImage {
    VkImage handle;
    VkImageView view;
    VkDeviceMemory memory;
};

struct GraphicsContext {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    uint32_t queue_family_index;
    VkCommandPool command_pool;
    
    VkPipelineLayout pipeline_layout;
    std::vector<VkShaderModule> shaders;
    VkPipeline pipeline;
    VkRenderPass render_pass;

    VkSwapchainKHR swapchain;
    VkExtent2D swapchain_extent;
    VkSurfaceFormatKHR swapchain_format;
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_image_views;
    std::vector<int32_t> swapchain_frames;

    GPUImage depth_image;
    std::vector<VkFramebuffer> framebuffers;
    std::vector<VkCommandBuffer> command_buffers;
    
    VkSemaphore swapchain_image_ready[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore swapchain_submit_done[MAX_FRAMES_IN_FLIGHT];
    VkFence frame_finished_fences[MAX_FRAMES_IN_FLIGHT];
    
    VkSurfaceKHR surface;

    WMContext wm;
};

static void vk_init(GraphicsContext& ctx) {
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

static int32_t find_memory_type(const VkPhysicalDeviceMemoryProperties* props,
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

static GPUImage allocate_image(VkDevice device, VkPhysicalDevice physical_device, VkFormat format, VkImageUsageFlags usage, uint32_t width, uint32_t height) {
    GPUImage image;

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

static void create_swapchain(GraphicsContext& ctx) {
    VkSurfaceCapabilitiesKHR surface_capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.physical_device, ctx.surface, &surface_capabilities);
    ctx.swapchain_extent = surface_capabilities.currentExtent;
    
    const VkFormat depth_format = VK_FORMAT_D32_SFLOAT;
    ctx.depth_image = allocate_image(ctx.device,
                                     ctx.physical_device,
                                     depth_format,
                                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                     ctx.swapchain_extent.width,
                                     ctx.swapchain_extent.height);
    
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
    color_attachment.format = ctx.swapchain_format.format;
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

    if (vkCreateRenderPass(ctx.device, &render_pass_ci, nullptr, &ctx.render_pass) != VK_SUCCESS) {
        throw std::runtime_error("Could not create render pass");
    }

    uint32_t present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.physical_device, ctx.surface, &present_mode_count, nullptr);
    std::vector<VkPresentModeKHR> present_modes(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.physical_device, ctx.surface, &present_mode_count, present_modes.data());

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

    VkSwapchainKHR swapchain;
    VkSwapchainCreateInfoKHR swapchain_ci{};
    swapchain_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_ci.surface = ctx.surface;
    swapchain_ci.minImageCount = surface_capabilities.minImageCount;
    swapchain_ci.imageFormat = ctx.swapchain_format.format;
    swapchain_ci.imageColorSpace = ctx.swapchain_format.colorSpace;
    swapchain_ci.imageExtent = ctx.swapchain_extent;
    swapchain_ci.imageArrayLayers = 1;
    swapchain_ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_ci.preTransform = surface_capabilities.currentTransform;
    swapchain_ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_ci.presentMode = chosen_present_mode;
    swapchain_ci.clipped = VK_TRUE;
    swapchain_ci.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(ctx.device, &swapchain_ci, nullptr, &ctx.swapchain) != VK_SUCCESS) {
        throw std::runtime_error("Could not create swapchain.");
    }

    uint32_t swapchain_image_count;
    if (vkGetSwapchainImagesKHR(ctx.device, ctx.swapchain, &swapchain_image_count, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("Could not query swapchain image count.");
    }

    ctx.swapchain_images.resize(swapchain_image_count);
    if (vkGetSwapchainImagesKHR(ctx.device, ctx.swapchain, &swapchain_image_count, ctx.swapchain_images.data()) != VK_SUCCESS) {
        throw std::runtime_error("Could not query swapchain images.");
    }

    ctx.swapchain_image_views.resize(swapchain_image_count);
    for (uint32_t i = 0; i < swapchain_image_count; i++) {
        VkImageViewCreateInfo fb_color_ci{};
        fb_color_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        fb_color_ci.image = ctx.swapchain_images[i];
        fb_color_ci.subresourceRange.baseArrayLayer = 0;
        fb_color_ci.subresourceRange.layerCount = 1;
        fb_color_ci.subresourceRange.baseMipLevel = 0;
        fb_color_ci.subresourceRange.levelCount = 1;
        fb_color_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        fb_color_ci.format = ctx.swapchain_format.format;
        fb_color_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;

        if (vkCreateImageView(ctx.device, &fb_color_ci, nullptr, &ctx.swapchain_image_views[i]) != VK_SUCCESS) {
            throw std::runtime_error("Could not create swapchain image view.");
        }
    }

    ctx.swapchain_frames.resize(swapchain_image_count, -1);

    ctx.framebuffers.resize(ctx.swapchain_images.size());
    for (size_t i = 0; i < ctx.swapchain_images.size(); i++) {
        std::vector<VkImageView> fb_attachments = {ctx.swapchain_image_views[i], ctx.depth_image.view};
        VkFramebufferCreateInfo framebuffer_ci{};
        framebuffer_ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_ci.width = ctx.swapchain_extent.width;
        framebuffer_ci.height = ctx.swapchain_extent.height;
        framebuffer_ci.attachmentCount = fb_attachments.size();
        framebuffer_ci.pAttachments = fb_attachments.data();
        framebuffer_ci.renderPass = ctx.render_pass;
        framebuffer_ci.layers = 1;
    
        if (vkCreateFramebuffer(ctx.device, &framebuffer_ci, nullptr, &ctx.framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Could not create framebuffer.");
        }
    }

    VkSemaphoreCreateInfo semaphore_ci{};
    semaphore_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fence_ci{};
    fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(ctx.device, &semaphore_ci, nullptr, &ctx.swapchain_image_ready[i]) != VK_SUCCESS
            || vkCreateSemaphore(ctx.device, &semaphore_ci, nullptr, &ctx.swapchain_submit_done[i]) != VK_SUCCESS
            || vkCreateFence(ctx.device, &fence_ci, nullptr, &ctx.frame_finished_fences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Could not create sync objects for frame.");
        }
    }

    ctx.command_buffers.resize(ctx.swapchain_images.size());
    
    VkCommandBufferAllocateInfo command_buffer_ai{};
    command_buffer_ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_ai.commandPool = ctx.command_pool;
    command_buffer_ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_ai.commandBufferCount = ctx.command_buffers.size();

    vkAllocateCommandBuffers(ctx.device, &command_buffer_ai, ctx.command_buffers.data());
}

static void window_init(GraphicsContext& ctx) {
    ctx.wm.display = XOpenDisplay(nullptr);
    ctx.wm.window = XCreateSimpleWindow(ctx.wm.display, XDefaultRootWindow(ctx.wm.display), 0, 0, 1920, 1080, 0, 0, XBlackPixel(ctx.wm.display, XDefaultScreen(ctx.wm.display)));

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

    // Pick a format
    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.physical_device, ctx.surface, &format_count, nullptr);

    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.physical_device, ctx.surface, &format_count, formats.data());

    size_t format_index = 0;
    for (size_t i = 0; i < formats.size(); i++) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            format_index = i;
        }
    }
    ctx.swapchain_format = formats[format_index];

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

    create_swapchain(ctx);    
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

static void destroy_image(const GraphicsContext& ctx, GPUImage& image) {
    vkDestroyImageView(ctx.device, image.view, nullptr);
    vkFreeMemory(ctx.device, image.memory, nullptr);
    vkDestroyImage(ctx.device, image.handle, nullptr);
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

    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = ctx.swapchain_extent.height;
    viewport.width = ctx.swapchain_extent.width;
    viewport.height = -static_cast<float>(ctx.swapchain_extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissors;
    scissors.offset = { 0, 0 };
    scissors.extent = ctx.swapchain_extent;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissors;

    VkPipelineRasterizationStateCreateInfo rasterization{};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.depthClampEnable = false;
    rasterization.rasterizerDiscardEnable = false;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
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
    pipeline_ci.layout = ctx.pipeline_layout;
    pipeline_ci.renderPass = ctx.render_pass;

    if (vkCreateGraphicsPipelines(ctx.device, VK_NULL_HANDLE, 1, &pipeline_ci, nullptr, &ctx.pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Could not create graphics pipeline.");
    }
}

template<typename T>
struct GPUBuffer {
    size_t count;
    VkBuffer handle;
    VkDeviceMemory memory;
};

template<typename T>
static GPUBuffer<T> gpu_buffer_allocate(const GraphicsContext& ctx, VkBufferUsageFlags usage, size_t count) {
    GPUBuffer<T> buf;
    buf.count = count;

    VkBufferCreateInfo buffer_ci{};
    buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_ci.size = count * sizeof(T);
    buffer_ci.usage = usage;
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
static T* gpu_buffer_map(const GraphicsContext &ctx, GPUBuffer<T>& buf) {
    T* ptr;

    if (vkMapMemory(ctx.device, buf.memory, 0, buf.count * sizeof(T), 0,
                    reinterpret_cast<void **>(&ptr)) != VK_SUCCESS) {
        throw std::runtime_error("Could not bind buffer");
    }

    return ptr;
}

template <typename T>
static void gpu_buffer_unmap(const GraphicsContext& ctx, GPUBuffer<T>& buf) {
    vkUnmapMemory(ctx.device, buf.memory);
}

template<typename T>
static GPUBuffer<T> allocate_and_fill_buffer(const GraphicsContext& ctx,
                                             const T* data,
                                             size_t count,
                                             VkBufferUsageFlags usage) {
    GPUBuffer<T> buf = gpu_buffer_allocate<T>(ctx, usage, count);

    T* device_data = gpu_buffer_map(ctx, buf);

    memcpy(device_data, data, sizeof(T) * count);

    gpu_buffer_unmap(ctx, buf);

    return buf;
}

template <typename T>
static void gpu_buffer_free(const GraphicsContext& ctx, GPUBuffer<T>& buf) {
    vkFreeMemory(ctx.device, buf.memory, nullptr);
    vkDestroyBuffer(ctx.device, buf.handle, nullptr);
}

struct Mesh {
    GPUBuffer<glm::vec3> pos_buffer;
    GPUBuffer<glm::vec2> uv_buffer;
    GPUBuffer<uint32_t> index_buffer;
    size_t vertex_count;
    size_t triangle_count;
};

static Mesh create_mesh(const GraphicsContext& ctx,
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
                                               VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    mesh.uv_buffer = allocate_and_fill_buffer(ctx,
                                              uvs,
                                              vertex_count,
                                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    mesh.index_buffer = allocate_and_fill_buffer(ctx,
                                                 indices,
                                                 triangle_count * 3,
                                                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    return mesh;
}

static void draw_mesh(VkCommandBuffer command_buffer, const Mesh& mesh) {
    VkBuffer bind_buffers[2] = {
        mesh.pos_buffer.handle,
        mesh.uv_buffer.handle,
    };
    VkDeviceSize bind_offsets[2] = {
        0,
        0,
    };
    
    vkCmdBindVertexBuffers(command_buffer, 0, 2, bind_buffers, bind_offsets);
    vkCmdBindIndexBuffer(command_buffer, mesh.index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);
	
    vkCmdDrawIndexed(command_buffer, mesh.triangle_count * 3, 1, 0, 0, 0);
}

static void destroy_mesh(const GraphicsContext& ctx, Mesh& mesh) {
    gpu_buffer_free(ctx, mesh.index_buffer);
    gpu_buffer_free(ctx, mesh.pos_buffer);
    gpu_buffer_free(ctx, mesh.uv_buffer);
}

struct Model {
    const Mesh* mesh;
    glm::mat4 transform;
};

struct Camera {
    glm::vec3 eye;
    glm::vec3 target;
    float fov;
    float aspect;
    float near;
    float far;
};

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

static void draw_model(VkCommandBuffer command_buffer, VkPipelineLayout layout, const glm::mat4& viewproj, const Model& model) {
    glm::mat4 mvp = viewproj * model.transform;
    vkCmdPushConstants(command_buffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, 16 * sizeof(float), glm::value_ptr(mvp));
    draw_mesh(command_buffer, *model.mesh);
}

static void record_command_buffer(const GraphicsContext& ctx, VkCommandBuffer command_buffer, uint32_t image_index, const std::vector<Model>& models, const Camera& camera) {
    VkCommandBufferBeginInfo command_buffer_bi{};
    command_buffer_bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		
    vkBeginCommandBuffer(command_buffer, &command_buffer_bi);
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.pipeline);

    VkClearValue color_clear_value{};
    color_clear_value.color.float32[0] = .1f;
    color_clear_value.color.float32[1] = .0f;
    color_clear_value.color.float32[2] = .1f;
    color_clear_value.color.float32[3] = 1.0f;

    VkClearValue depth_clear_value{};
    depth_clear_value.depthStencil.depth = 1.0f;

    std::vector<VkClearValue> clear_values = {color_clear_value, depth_clear_value};

    VkRenderPassBeginInfo render_pass_bi{};
    render_pass_bi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_bi.renderPass = ctx.render_pass;
    render_pass_bi.framebuffer = ctx.framebuffers[image_index];
    render_pass_bi.renderArea = {{0, 0}, ctx.swapchain_extent};
    render_pass_bi.clearValueCount = clear_values.size();
    render_pass_bi.pClearValues = clear_values.data();

    VkImageSubresourceRange image_range{};
    image_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_range.levelCount = 1;
    image_range.layerCount = 1;

    vkCmdBeginRenderPass(command_buffer, &render_pass_bi, VK_SUBPASS_CONTENTS_INLINE);

    glm::mat4 viewproj = camera_viewproj(camera);
    for (const Model& model : models) {
        draw_model(command_buffer, ctx.pipeline_layout, viewproj, model);
    }

    vkCmdEndRenderPass(command_buffer);
    vkEndCommandBuffer(command_buffer);
}

static void destroy_swapchain(GraphicsContext& ctx) {
    vkDestroyRenderPass(ctx.device, ctx.render_pass, nullptr);
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyFence(ctx.device, ctx.frame_finished_fences[i], nullptr);
        vkDestroySemaphore(ctx.device, ctx.swapchain_image_ready[i], nullptr);
        vkDestroySemaphore(ctx.device, ctx.swapchain_submit_done[i], nullptr);
    }

    for (size_t i = 0; i < ctx.swapchain_images.size(); i++) {
        vkDestroyFramebuffer(ctx.device, ctx.framebuffers[i], nullptr);
        vkDestroyImageView(ctx.device, ctx.swapchain_image_views[i], nullptr);
    }
    destroy_image(ctx, ctx.depth_image);

    ctx.framebuffers.clear();
    ctx.swapchain_image_views.clear();
    ctx.swapchain_images.clear();
    ctx.swapchain_frames.clear();

    vkDestroySwapchainKHR(ctx.device, ctx.swapchain, nullptr);
}

void vk_finalize(GraphicsContext& ctx) {
    // Pipeline :
    vkFreeCommandBuffers(ctx.device, ctx.command_pool, ctx.command_buffers.size(), ctx.command_buffers.data());

    for (VkShaderModule shader : ctx.shaders) {
        vkDestroyShaderModule(ctx.device, shader, nullptr);
    }
    vkDestroyPipeline(ctx.device, ctx.pipeline, nullptr);
    vkDestroyPipelineLayout(ctx.device, ctx.pipeline_layout, nullptr);

    // Window :
    destroy_swapchain(ctx);
    
    vkDestroyCommandPool(ctx.device, ctx.command_pool, nullptr);
    vkDestroySurfaceKHR(ctx.instance, ctx.surface, nullptr);

    XDestroyWindow(ctx.wm.display, ctx.wm.window);
    XCloseDisplay(ctx.wm.display);

    // Vulkan :
    vkDestroyDevice(ctx.device, nullptr);
    vkDestroyInstance(ctx.instance, nullptr);
}

static void print_fence_states(const GraphicsContext& ctx) {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkResult result = vkGetFenceStatus(ctx.device, ctx.frame_finished_fences[i]);
        std::cout << "Frame #" << i
                  << " (" << ctx.frame_finished_fences[i] 
                  << ") is ";
        if (result == VK_SUCCESS) {
            std::cout << "available\n";
        } else {
            std::cout << "unavailable\n";
        }
    }
}

static void recreate_swapchain(GraphicsContext& ctx) {
    std::cout << "Waiting on all fences before destroying swapchain\n";
    print_fence_states(ctx);
    vkWaitForFences(ctx.device, MAX_FRAMES_IN_FLIGHT, ctx.frame_finished_fences, VK_TRUE, UINT64_MAX);
    destroy_swapchain(ctx);
    create_swapchain(ctx);
    std::cerr << "Recreated swapchain\n";
}

int main(int argc, char** argv) {
    GraphicsContext ctx;
    vk_init(ctx);

    window_init(ctx);

    pipeline_init(ctx);

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

    Camera cam;
    cam.aspect = static_cast<float>(ctx.swapchain_extent.width) / static_cast<float>(ctx.swapchain_extent.height);
    cam.eye = glm::vec3(0, -2, 2);
    cam.target = glm::vec3(0, 0, 0);
    cam.fov = glm::radians(60.0f);
    cam.near = 0.01f;
    cam.far = 100.0f;

    VkQueue queue;
    vkGetDeviceQueue(ctx.device, ctx.queue_family_index, 0, &queue);

    uint32_t frames = 0;
    uint32_t current_frame = 0;
    bool should_close = false;

    auto t0 = std::chrono::system_clock::now();

    while (!should_close) {
        while (XPending(ctx.wm.display)) {
            XEvent event;
            XNextEvent(ctx.wm.display, &event);

            switch (event.type) {
            case ClientMessage:
                if (event.xclient.message_type == XInternAtom(ctx.wm.display, "WM_PROTOCOLS", true)
                    && event.xclient.data.l[0] == XInternAtom(ctx.wm.display, "WM_DELETE_WINDOW", true)) {
                    should_close = true;
                }
                break;
            default:
                std::cerr << "Unhandled event type " << event.type << "\n";
            }
        }

        frames++;
        std::cout << "-- FRAME " << frames << " --\n";
        auto t1 = std::chrono::system_clock::now();
        std::chrono::duration<float> elapsed_duration = t1 - t0;
        float elapsed = elapsed_duration.count();

        models[0].transform =
            glm::translate(glm::vec3(std::sin(elapsed), 0, 0))
            * glm::rotate(elapsed * 6.0f, glm::vec3(0, 0, 1))
            * glm::rotate(glm::radians(90.0f), glm::vec3(0, 1, 0));

        // Wait until the current frame is done rendering.
        std::cout << "Waiting until frame #" << current_frame << " is rendered.\n";
        vkWaitForFences(ctx.device, 1, &ctx.frame_finished_fences[current_frame], VK_TRUE, UINT64_MAX);
        std::cout << "Done waiting.\n";

        uint32_t image_index;
        VkResult acquire_result; 
        do {
            std::cout << "Acquiring an image.\n";
            acquire_result = vkAcquireNextImageKHR(ctx.device,
                                                   ctx.swapchain,
                                                   0,
                                                   ctx.swapchain_image_ready[current_frame],
                                                   VK_NULL_HANDLE,
                                                   &image_index);
            switch (acquire_result) {
            case VK_SUCCESS:
                break;
            case VK_NOT_READY:
                throw std::runtime_error("No swapchain image ready.");
                break;
            case VK_SUBOPTIMAL_KHR:
                std::cerr << "Warning : suboptimal swapchain\n";
                break;
            case VK_ERROR_OUT_OF_DATE_KHR:
                recreate_swapchain(ctx);
                break;
            default:
                throw std::runtime_error("Unexpected error when acquiring swapchain image.");
                break;
            }
        } while (acquire_result != VK_SUCCESS);
        std::cout << "Rendering to image #" << image_index << "\n";

        // Make sure we're not rendering to an image that is being used by another in-flight frame
        if (ctx.swapchain_frames[image_index] >= 0) {
            std::cout << "Image #" << image_index
                      << " in use by frame #" << ctx.swapchain_frames[image_index]
                      << " (" << ctx.frame_finished_fences[ctx.swapchain_frames[image_index]] << ")"
                      << ", waiting.\n";
            vkWaitForFences(ctx.device,
                            1,
                            &ctx.frame_finished_fences[ctx.swapchain_frames[image_index]],
                            VK_TRUE,
                            UINT64_MAX);
            std::cout << "Done waiting.\n";
        }
        ctx.swapchain_frames[image_index] = current_frame;

        record_command_buffer(ctx, ctx.command_buffers[image_index], image_index, models, cam);
        
        VkPipelineStageFlags submit_wait_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &ctx.command_buffers[image_index];
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &ctx.swapchain_image_ready[current_frame];
        submit_info.pWaitDstStageMask = &submit_wait_stage_mask;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &ctx.swapchain_submit_done[current_frame];

        vkResetFences(ctx.device, 1, &ctx.frame_finished_fences[current_frame]);
        if (vkQueueSubmit(queue, 1, &submit_info, ctx.frame_finished_fences[current_frame]) != VK_SUCCESS) {
            throw std::runtime_error("Could not submit commands.");
        }
        std::cout << "Submitted queue, going to signal fence #" << current_frame << " (" << ctx.frame_finished_fences[current_frame] << ")" << "\n";

        VkPresentInfoKHR present_info{};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &ctx.swapchain;
        present_info.pImageIndices = &image_index;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &ctx.swapchain_submit_done[current_frame];

        VkResult present_result = vkQueuePresentKHR(queue, &present_info);
        switch (present_result) {
        case VK_SUCCESS:
            break;
        case VK_SUBOPTIMAL_KHR:
            std::cerr << "Warning : suboptimal swapchain.\n";
            break;
        case VK_ERROR_OUT_OF_DATE_KHR:
            std::cerr << "Warning : swapchain out of date. Did not render frame.\n";
            break;
        default:
            throw std::runtime_error("Unknown error when presenting.");
        }

        current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    auto t1 = std::chrono::system_clock::now();
    std::chrono::duration<float> elapsed_duration = t1 - t0;
    float elapsed = elapsed_duration.count();

    float avg_fps = frames / elapsed;
    std::cout << "Average FPS : " << avg_fps << "\n";

    vkDeviceWaitIdle(ctx.device);

    destroy_mesh(ctx, plane);

    vk_finalize(ctx);
}
