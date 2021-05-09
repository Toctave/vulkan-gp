#include "graphics.hpp"

#include "../platform_wm.hpp"

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

static void destroy_image(VkDevice device, VulkanImage& image) {
    vkDestroyImageView(device, image.view, nullptr);
    vkFreeMemory(device, image.memory, nullptr);
    vkDestroyImage(device, image.handle, nullptr);
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


static void destroy_swapchain(VkDevice device, Swapchain& swapchain) {
    vkDestroyRenderPass(device, swapchain.render_pass, nullptr);
    
    for (size_t i = 0; i < swapchain.images.size(); i++) {
        vkDestroyFramebuffer(device, swapchain.framebuffers[i], nullptr);
        vkDestroyImageView(device, swapchain.image_views[i], nullptr);
    }
    destroy_image(device, swapchain.depth_image);

    vkDestroySwapchainKHR(device, swapchain.handle, nullptr);
}

void recreate_swapchain(VulkanGraphicsContext& ctx) {
    vkWaitForFences(ctx.vk->device, MAX_FRAMES_IN_FLIGHT, ctx.frame_finished, VK_TRUE, UINT64_MAX);
    Swapchain new_swapchain = create_swapchain(ctx.vk->device, ctx.vk->physical_device, ctx.surface, ctx.swapchain.handle);
    destroy_swapchain(ctx.vk->device, ctx.swapchain);
    ctx.swapchain = new_swapchain;
}

static void window_init(VulkanGraphicsContext& ctx, const WMContext* wm) {
    ctx.wm = wm;
    
    XSelectInput(ctx.wm->display,
                 ctx.wm->window,
                 ButtonPressMask
                 | ButtonReleaseMask
                 | KeyPressMask
                 | KeyReleaseMask
                 | PointerMotionMask);
    
    XStoreName(ctx.wm->display, ctx.wm->window, "Vulkan-gp");

    Atom protocol = XInternAtom(ctx.wm->display, "WM_DELETE_WINDOW", false);
    XSetWMProtocols(ctx.wm->display, ctx.wm->window, &protocol, 1);

    XMapRaised(ctx.wm->display, ctx.wm->window);

    VkXlibSurfaceCreateInfoKHR surface_ci{};
    surface_ci.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    surface_ci.dpy = ctx.wm->display;
    surface_ci.window = ctx.wm->window;

    if (vkCreateXlibSurfaceKHR(ctx.vk->instance, &surface_ci, nullptr, &ctx.surface) != VK_SUCCESS) {
        throw std::runtime_error("Could not create X11 surface.");
    }

    VkBool32 surface_support;
    vkGetPhysicalDeviceSurfaceSupportKHR(ctx.vk->physical_device, ctx.vk->graphics_queue_idx, ctx.surface, &surface_support);
    if (!surface_support) {
        throw std::runtime_error("Surface does not support presentation.");
    }

    VkCommandPoolCreateInfo command_pool_ci{};
    command_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    command_pool_ci.queueFamilyIndex = ctx.vk->graphics_queue_idx;
	
    if (vkCreateCommandPool(ctx.vk->device, &command_pool_ci, nullptr, &ctx.command_pool) != VK_SUCCESS) {
        throw std::runtime_error("Could not create command pool.");
    }

    ctx.swapchain = create_swapchain(ctx.vk->device, ctx.vk->physical_device, ctx.surface);

    ctx.command_buffers.resize(ctx.swapchain.images.size());
    
    VkCommandBufferAllocateInfo command_buffer_ai{};
    command_buffer_ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_ai.commandPool = ctx.command_pool;
    command_buffer_ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_ai.commandBufferCount = ctx.command_buffers.size();

    vkAllocateCommandBuffers(ctx.vk->device, &command_buffer_ai, ctx.command_buffers.data());

    VkSemaphoreCreateInfo semaphore_ci{};
    semaphore_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fence_ci{};
    fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(ctx.vk->device, &semaphore_ci, nullptr, &ctx.swapchain_image_ready[i]) != VK_SUCCESS
            || vkCreateSemaphore(ctx.vk->device, &semaphore_ci, nullptr, &ctx.swapchain_submit_done[i]) != VK_SUCCESS
            || vkCreateFence(ctx.vk->device, &fence_ci, nullptr, &ctx.frame_finished[i]) != VK_SUCCESS) {
            throw std::runtime_error("Could not create sync objects for frame.");
        }
    }
}

static void pipeline_init(VulkanGraphicsContext& ctx) {
    VkPipelineShaderStageCreateInfo vertex_shader_stage{};
    vertex_shader_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertex_shader_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertex_shader_stage.module = create_shader_module(ctx.vk->device, "shaders/triangle.vert.spv");
    vertex_shader_stage.pName = "main";
    ctx.shaders.push_back(vertex_shader_stage.module);

    VkPipelineShaderStageCreateInfo fragment_shader_stage{};
    fragment_shader_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragment_shader_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragment_shader_stage.module = create_shader_module(ctx.vk->device, "shaders/triangle.frag.spv");
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
            0,                       // binding
            VK_FORMAT_R32G32_SFLOAT, // format
            sizeof(float) * 3,       // offset
        },
        {
            // Normal :
            2,                          // location
            0,                          // binding
            VK_FORMAT_R32G32B32_SFLOAT, // format
            sizeof(float) * 5,          // offset
        },
    };

    std::vector<VkVertexInputBindingDescription> vertex_bindings = {
        {
            // position :
            0,                           // binding
            sizeof(float) * 8,           // stride
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
    VkPushConstantRange push_matrices;
    push_matrices.offset = 0;
    push_matrices.size = sizeof(PushMatrices);
    push_matrices.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    
    push_constants.push_back(push_matrices);

    VkPipelineLayoutCreateInfo layout_ci{};
    layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_ci.setLayoutCount = descriptor_set_layouts.size();
    layout_ci.pSetLayouts = descriptor_set_layouts.data();
    layout_ci.pushConstantRangeCount = push_constants.size();
    layout_ci.pPushConstantRanges = push_constants.data();

    if (vkCreatePipelineLayout(ctx.vk->device, &layout_ci, nullptr, &ctx.pipeline_layout) != VK_SUCCESS) {
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

    if (vkCreateGraphicsPipelines(ctx.vk->device, VK_NULL_HANDLE, 1, &pipeline_ci, nullptr, &ctx.pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Could not create graphics pipeline.");
    }
}

void graphics_init(const VulkanContext* ctx, const WMContext* wm, VulkanGraphicsContext& graphics) {
    graphics.vk = ctx;
    
    window_init(graphics, wm);
    pipeline_init(graphics);
    graphics.next_frame = 0;
}

void graphics_wait_idle(const VulkanGraphicsContext &ctx) {
    vkDeviceWaitIdle(ctx.vk->device);
}

void graphics_finalize(VulkanGraphicsContext& ctx) {
    vkDeviceWaitIdle(ctx.vk->device);
    
    // Pipeline :
    for (VkShaderModule shader : ctx.shaders) {
        vkDestroyShaderModule(ctx.vk->device, shader, nullptr);
    }
    vkDestroyPipeline(ctx.vk->device, ctx.pipeline, nullptr);
    vkDestroyPipelineLayout(ctx.vk->device, ctx.pipeline_layout, nullptr);

    // Window :
    destroy_swapchain(ctx.vk->device, ctx.swapchain);
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyFence(ctx.vk->device, ctx.frame_finished[i], nullptr);
        vkDestroySemaphore(ctx.vk->device, ctx.swapchain_image_ready[i], nullptr);
        vkDestroySemaphore(ctx.vk->device, ctx.swapchain_submit_done[i], nullptr);
    }
    
    vkDestroyCommandPool(ctx.vk->device, ctx.command_pool, nullptr);
    vkDestroySurfaceKHR(ctx.vk->instance, ctx.surface, nullptr);

}

