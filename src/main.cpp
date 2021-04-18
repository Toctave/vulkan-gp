#include <iostream>
#include <vector>

#include <string.h>

#include <X11/Xlib.h>
#include <vulkan/vulkan.h>

struct VulkanContext {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;

    uint32_t queue_family_index;
};

struct DisplayContext {
    VulkanContext vk_context;
    
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    
    Display* display;
    Window window;
};

static void vk_init(VulkanContext& vulkan) {
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


    VkInstanceCreateInfo instance_ci{};
    instance_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_ci.enabledLayerCount = required_layers.size();
    instance_ci.ppEnabledLayerNames = required_layers.data();
    instance_ci.enabledExtensionCount = required_extensions.size();
    instance_ci.ppEnabledExtensionNames = required_extensions.data();

    if (vkCreateInstance(&instance_ci, nullptr, &vulkan.instance) != VK_SUCCESS) {
	throw std::runtime_error("Failed to create vulkan instance.");
    }

    uint32_t physical_device_count;
    vkEnumeratePhysicalDevices(vulkan.instance, &physical_device_count, nullptr);

    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    vkEnumeratePhysicalDevices(vulkan.instance, &physical_device_count, physical_devices.data());


    std::vector<const char*> required_device_extensions = {
	"VK_KHR_swapchain",
    };
    
    vulkan.physical_device = VK_NULL_HANDLE;
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
	    vulkan.physical_device = candidate;
	    break;
	}
    }

    if (vulkan.physical_device == VK_NULL_HANDLE) {
	throw std::runtime_error("Could not find a GPU.");
    }

    uint32_t family_count;
    vkGetPhysicalDeviceQueueFamilyProperties(vulkan.physical_device, &family_count, nullptr);
    std::vector<VkQueueFamilyProperties> families(family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(vulkan.physical_device, &family_count, families.data());

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
    vulkan.queue_family_index = family_index;

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
    
    if (vkCreateDevice(vulkan.physical_device, &device_ci, nullptr, &vulkan.device) != VK_SUCCESS) {
	throw std::runtime_error("Could not create Vulkan device.");
    }
}

void window_init(DisplayContext& ctx) {
    ctx.display = XOpenDisplay(nullptr);
    ctx.window = XCreateSimpleWindow(ctx.display, XDefaultRootWindow(ctx.display), 0, 0, 640, 480, 0, 0, XBlackPixel(ctx.display, XDefaultScreen(ctx.display)));

    VkXlibSurfaceCreateInfoKHR surface_ci{};
    surface_ci.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    surface_ci.dpy = ctx.display;
    surface_ci.window = ctx.window;

    if (vkCreateXlibSurfaceKHR(ctx.vk_context.instance, &surface_ci, nullptr, &ctx.surface) != VK_SUCCESS) {
	throw std::runtime_error("Could not create X11 surface.");
    }

    // Pick a format
    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.vk_context.physical_device, ctx.surface, &format_count, nullptr);

    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.vk_context.physical_device, ctx.surface, &format_count, formats.data());

    size_t format_index = 0;
    for (size_t i = 0; i < formats.size(); i++) {
	if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
	    format_index = i;
	}
    }

    VkSurfaceCapabilitiesKHR surface_capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.vk_context.physical_device, ctx.surface, &surface_capabilities);

    VkBool32 surface_support;
    vkGetPhysicalDeviceSurfaceSupportKHR(ctx.vk_context.physical_device, ctx.vk_context.queue_family_index, ctx.surface, &surface_support);
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
    swapchain_ci.imageExtent = surface_capabilities.currentExtent;
    swapchain_ci.imageArrayLayers = 1;
    swapchain_ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_ci.preTransform = surface_capabilities.currentTransform;
    swapchain_ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_ci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchain_ci.clipped = VK_TRUE;
    swapchain_ci.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(ctx.vk_context.device, &swapchain_ci, nullptr, &ctx.swapchain) != VK_SUCCESS) {
	throw std::runtime_error("Could not create swapchain.");
    }
    
}
    
int main(int argc, char** argv) {
    DisplayContext ctx;
    vk_init(ctx.vk_context);

    window_init(ctx);

    XMapRaised(ctx.display, ctx.window);
    while (true) {
	while (XPending(ctx.display)) {
	    XEvent event;
	    XNextEvent(ctx.display, &event);
	}
    }
}
