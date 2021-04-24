#include <iostream>
#include <fstream>
#include <vector>

#include <string.h>

#include <X11/Xlib.h>
#include <vulkan/vulkan.h>

struct DisplayContext {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    uint32_t queue_family_index;

    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkExtent2D swapchain_extent;
    VkCommandPool command_pool;
    VkFormat format;
    VkPipeline pipeline;
    VkRenderPass render_pass;

    std::vector<VkImage> swapchain_images;
    std::vector<VkFramebuffer> framebuffers;
    
    Display* display;
    Window window;
};

static void vk_init(DisplayContext& ctx) {
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

void window_init(DisplayContext& ctx) {
    ctx.display = XOpenDisplay(nullptr);
    ctx.window = XCreateSimpleWindow(ctx.display, XDefaultRootWindow(ctx.display), 0, 0, 1920, 1080, 0, 0, XBlackPixel(ctx.display, XDefaultScreen(ctx.display)));

    Atom protocol = XInternAtom(ctx.display, "WM_DELETE_WINDOW", false);
    XSetWMProtocols(ctx.display, ctx.window, &protocol, 1);

    VkXlibSurfaceCreateInfoKHR surface_ci{};
    surface_ci.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    surface_ci.dpy = ctx.display;
    surface_ci.window = ctx.window;

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
    ctx.format = formats[format_index].format;

    VkSurfaceCapabilitiesKHR surface_capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.physical_device, ctx.surface, &surface_capabilities);
    ctx.swapchain_extent = surface_capabilities.currentExtent;

    VkBool32 surface_support;
    vkGetPhysicalDeviceSurfaceSupportKHR(ctx.physical_device, ctx.queue_family_index, ctx.surface, &surface_support);
    if (!surface_support) {
	throw std::runtime_error("Surface does not support presentation.");
    }

    VkSwapchainKHR swapchain;
    VkSwapchainCreateInfoKHR swapchain_ci{};
    swapchain_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_ci.surface = ctx.surface;
    swapchain_ci.minImageCount = surface_capabilities.minImageCount;
    swapchain_ci.imageFormat = formats[format_index].format;
    swapchain_ci.imageColorSpace = formats[format_index].colorSpace;
    swapchain_ci.imageExtent = ctx.swapchain_extent;
    swapchain_ci.imageArrayLayers = 1;
    swapchain_ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_ci.preTransform = surface_capabilities.currentTransform;
    swapchain_ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_ci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
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

    VkCommandPoolCreateInfo command_pool_ci{};
    command_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_ci.queueFamilyIndex = ctx.queue_family_index;
	
    if (vkCreateCommandPool(ctx.device, &command_pool_ci, nullptr, &ctx.command_pool) != VK_SUCCESS) {
	throw std::runtime_error("Could not create command pool.");
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

static VkImage allocate_image(VkDevice device, VkPhysicalDevice physical_device, VkFormat format, uint32_t width, uint32_t height) {
    VkImage image;

    VkImageCreateInfo image_ci{};
    image_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_ci.imageType = VK_IMAGE_TYPE_2D;
    image_ci.arrayLayers = 1;
    image_ci.format = format;
    image_ci.extent.width = width;
    image_ci.extent.height = height;
    image_ci.extent.depth = 1;
    image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
    image_ci.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    image_ci.mipLevels = 1;
    image_ci.arrayLayers = 1;

    if (vkCreateImage(device, &image_ci, nullptr, &image) != VK_SUCCESS) {
	throw std::runtime_error("Could not create image.");
    }

    VkMemoryRequirements image_req;
    vkGetImageMemoryRequirements(device, image, &image_req);

    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &props);

    int32_t image_mem_type = find_memory_type(&props, image_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkDeviceMemory image_memory;
    VkMemoryAllocateInfo image_memory_ai{};
    image_memory_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    image_memory_ai.allocationSize = image_req.size;
    image_memory_ai.memoryTypeIndex = image_mem_type;
    
    if (vkAllocateMemory(device, &image_memory_ai, nullptr, &image_memory) != VK_SUCCESS) {
	throw std::runtime_error("Could not allocate image memory.");
    }
    
    vkBindImageMemory(device, image, image_memory, 0);

    return image;
}

static void pipeline_init(DisplayContext& ctx) {
    std::vector<VkAttachmentDescription> attachments;
    VkAttachmentDescription color_attachment{};
    color_attachment.format = ctx.format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    attachments.push_back(color_attachment);

    VkAttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    std::vector<VkSubpassDescription> subpasses;
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = nullptr;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    subpass.preserveAttachmentCount = 0;

    subpasses.push_back(subpass);
    
    VkRenderPassCreateInfo render_pass_ci{};
    render_pass_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_ci.attachmentCount = attachments.size();
    render_pass_ci.pAttachments = attachments.data();
    render_pass_ci.subpassCount = subpasses.size();
    render_pass_ci.pSubpasses = subpasses.data();

    if (vkCreateRenderPass(ctx.device, &render_pass_ci, nullptr, &ctx.render_pass) != VK_SUCCESS) {
	throw std::runtime_error("Could not create render pass");
    }
    
    VkPipelineShaderStageCreateInfo vertex_shader_stage{};
    vertex_shader_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertex_shader_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertex_shader_stage.module = create_shader_module(ctx.device, "shaders/triangle.vert.spv");
    vertex_shader_stage.pName = "main";

    VkPipelineShaderStageCreateInfo fragment_shader_stage{};
    fragment_shader_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragment_shader_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragment_shader_stage.module = create_shader_module(ctx.device, "shaders/triangle.frag.spv");
    fragment_shader_stage.pName = "main";

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {
	vertex_shader_stage,
	fragment_shader_stage,
    };

    std::vector<VkVertexInputAttributeDescription> vertex_attributes = {
	{ // position : 
	    0, // location
	    0, // binding
	    VK_FORMAT_R32G32B32_SFLOAT, // format
	    0, // offset
	},
	{ // color :
	    1, // location
	    0, // binding
	    VK_FORMAT_R32G32B32_SFLOAT, // format
	    3 * sizeof(float), // offset
	},
    };

    std::vector<VkVertexInputBindingDescription> vertex_bindings = {
	{ // position :
	    0, // binding
	    sizeof(float) * 6, // stride
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
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterization.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

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

    VkPipelineLayout layout;
    VkPipelineLayoutCreateInfo layout_ci{};
    layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_ci.setLayoutCount = descriptor_set_layouts.size();
    layout_ci.pSetLayouts = descriptor_set_layouts.data();

    if (vkCreatePipelineLayout(ctx.device, &layout_ci, nullptr, &layout) != VK_SUCCESS) {
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
    pipeline_ci.layout = layout;
    pipeline_ci.renderPass = ctx.render_pass;

    if (vkCreateGraphicsPipelines(ctx.device, VK_NULL_HANDLE, 1, &pipeline_ci, nullptr, &ctx.pipeline) != VK_SUCCESS) {
	throw std::runtime_error("Could not create graphics pipeline.");
    }

    ctx.framebuffers.resize(ctx.swapchain_images.size());
    for (size_t i = 0; i < ctx.swapchain_images.size(); i++) {
	VkImageViewCreateInfo fb_color_ci{};
	fb_color_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	fb_color_ci.image = ctx.swapchain_images[i];
	fb_color_ci.subresourceRange.baseArrayLayer = 0;
	fb_color_ci.subresourceRange.layerCount = 1;
	fb_color_ci.subresourceRange.baseMipLevel = 0;
	fb_color_ci.subresourceRange.levelCount = 1;
	fb_color_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	fb_color_ci.format = ctx.format;
	fb_color_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;

	VkImageView fb_color;
	if (vkCreateImageView(ctx.device, &fb_color_ci, nullptr, &fb_color) != VK_SUCCESS) {
	    throw std::runtime_error("Could not create framebuffer color attachment.");
	}

	VkFramebufferCreateInfo framebuffer_ci{};
	framebuffer_ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebuffer_ci.width = ctx.swapchain_extent.width;
	framebuffer_ci.height = ctx.swapchain_extent.height;
	framebuffer_ci.attachmentCount = 1;
	framebuffer_ci.pAttachments = &fb_color;
	framebuffer_ci.renderPass = ctx.render_pass;
	framebuffer_ci.layers = 1;
    
	if (vkCreateFramebuffer(ctx.device, &framebuffer_ci, nullptr, &ctx.framebuffers[i]) != VK_SUCCESS) {
	    throw std::runtime_error("Could not create framebuffer.");
	}
    }
}

template<typename T>
static VkBuffer allocate_and_fill_buffer(VkDevice device,
					 VkPhysicalDevice physical_device,
					 uint32_t queue_family_index,
					 const T* data,
					 size_t count,
					 VkBufferUsageFlags usage) {
    VkBuffer buffer;
    
    VkBufferCreateInfo buffer_ci{};
    buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_ci.size = sizeof(T) * count;
    buffer_ci.usage = usage;
    buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer_ci.queueFamilyIndexCount = 1;
    buffer_ci.pQueueFamilyIndices = &queue_family_index;

    if (vkCreateBuffer(device, &buffer_ci, nullptr, &buffer) != VK_SUCCESS) {
	throw std::runtime_error("Could not create buffer.");
    }

    VkMemoryRequirements buffer_memory_requirements;
    vkGetBufferMemoryRequirements(device, buffer, &buffer_memory_requirements);

    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

    VkDeviceMemory buffer_memory;
    VkMemoryAllocateInfo buffer_memory_ai{};
    buffer_memory_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    buffer_memory_ai.allocationSize = buffer_memory_requirements.size;
    buffer_memory_ai.memoryTypeIndex =
	find_memory_type(&memory_properties,
			 buffer_memory_requirements.memoryTypeBits,
			 (VkMemoryPropertyFlagBits) (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
			  | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    
    if (vkAllocateMemory(device, &buffer_memory_ai, nullptr, &buffer_memory) != VK_SUCCESS) {
	throw std::runtime_error("Could not allocate vertex buffer memory.");
    }

    T* device_data;
    vkMapMemory(device,
		buffer_memory,
		0,
		sizeof(T) * count,
		0,
		reinterpret_cast<void**>(&device_data));

    memcpy(device_data, data, sizeof(T) * count);

    vkUnmapMemory(device, buffer_memory);

    vkBindBufferMemory(device, buffer, buffer_memory, 0);

    return buffer;
}
    
int main(int argc, char** argv) {
    DisplayContext ctx;
    vk_init(ctx);

    window_init(ctx);
    XMapRaised(ctx.display, ctx.window);

    pipeline_init(ctx);

    VkQueue queue;
    vkGetDeviceQueue(ctx.device, ctx.queue_family_index, 0, &queue);

    VkSemaphore image_ready;
    VkSemaphoreCreateInfo image_ready_ci{};
    image_ready_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    vkCreateSemaphore(ctx.device, &image_ready_ci, nullptr, &image_ready);

    std::vector<float> vertex_data = {
	-.5f, -.5f, 0, 1, 0, 0,
	.5f, -.5f, 0, 0, 1, 0,
	.5f, .5f, 0, 0, 0, 1,
	-.5f, .5f, 0, 1, 1, 1,
    };

    std::vector<uint32_t> vertex_indices = {
	0, 1, 3,
	1, 3, 2,
    };

    VkBuffer vertex_buffer = allocate_and_fill_buffer(ctx.device,
						      ctx.physical_device,
						      ctx.queue_family_index,
						      vertex_data.data(),
						      vertex_data.size(),
						      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    VkBuffer index_buffer = allocate_and_fill_buffer(ctx.device,
						     ctx.physical_device,
						     ctx.queue_family_index,
						     vertex_indices.data(),
						     vertex_indices.size(),
						     VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    VkSemaphore submit_done;
    VkSemaphoreCreateInfo submit_done_ci{};
    submit_done_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vkCreateSemaphore(ctx.device, &submit_done_ci, nullptr, &submit_done);

    bool should_close = false;
    while (!should_close) {
	while (XPending(ctx.display)) {
	    XEvent event;
	    XNextEvent(ctx.display, &event);

	    switch (event.type) {
	    case ClientMessage:
		if (event.xclient.message_type == XInternAtom(ctx.display, "WM_PROTOCOLS", true)
		    && event.xclient.data.l[0] == XInternAtom(ctx.display, "WM_DELETE_WINDOW", true)) {
		    should_close = true;
		}
		break;
	    default:
		std::cerr << "Unhandled event type " << event.type << "\n";
	    }
	}

	uint32_t image_index;
	if (vkAcquireNextImageKHR(ctx.device, ctx.swapchain, 0, image_ready, VK_NULL_HANDLE, &image_index) != VK_SUCCESS) {
	    throw std::runtime_error("Failed to acquire swapchain image");
	}

	VkCommandBuffer command_buffer;
	VkCommandBufferAllocateInfo command_buffer_ai{};
	command_buffer_ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	command_buffer_ai.commandPool = ctx.command_pool;
	command_buffer_ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	command_buffer_ai.commandBufferCount = 1;

	vkAllocateCommandBuffers(ctx.device, &command_buffer_ai, &command_buffer);

	VkCommandBufferBeginInfo command_buffer_bi{};
	command_buffer_bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		
	vkBeginCommandBuffer(command_buffer, &command_buffer_bi);
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.pipeline);

	VkClearValue clear_value{};
	clear_value.color.float32[0] = .1f;
	clear_value.color.float32[1] = .0f;
	clear_value.color.float32[2] = .1f;
	clear_value.color.float32[3] = 1.0f;

	VkRenderPassBeginInfo render_pass_bi{};
	render_pass_bi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_bi.renderPass = ctx.render_pass;
	render_pass_bi.framebuffer = ctx.framebuffers[image_index];
	render_pass_bi.renderArea = {{0, 0}, ctx.swapchain_extent};
	render_pass_bi.clearValueCount = 1;
	render_pass_bi.pClearValues = &clear_value;

	VkImageSubresourceRange image_range{};
	image_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_range.levelCount = 1;
	image_range.layerCount = 1;

	vkCmdBeginRenderPass(command_buffer, &render_pass_bi, VK_SUBPASS_CONTENTS_INLINE);

	VkDeviceSize bind_offset = 0;
	vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer, &bind_offset);
	vkCmdBindIndexBuffer(command_buffer, index_buffer, 0, VK_INDEX_TYPE_UINT32);
	
	vkCmdDrawIndexed(command_buffer, vertex_indices.size(), 1, 0, 0, 0);

	vkCmdEndRenderPass(command_buffer);
	vkEndCommandBuffer(command_buffer);

	VkPipelineStageFlags submit_wait_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

	VkSubmitInfo submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = &image_ready;
	submit_info.pWaitDstStageMask = &submit_wait_stage_mask;
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = &submit_done;

	vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);

	VkPresentInfoKHR present_info{};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &ctx.swapchain;
	present_info.pImageIndices = &image_index;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &submit_done;
	
	vkQueuePresentKHR(queue, &present_info);
	
	vkDeviceWaitIdle(ctx.device);
	vkFreeCommandBuffers(ctx.device, ctx.command_pool, 1, &command_buffer);
    }
}
