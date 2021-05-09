#include "compute.hpp"

void compute_init(const VulkanContext* vk, VulkanComputeContext& ctx) {
    ctx.vk = vk;
    
    VkCommandPoolCreateInfo command_pool_ci{};
    command_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_ci.queueFamilyIndex = ctx.vk->compute_queue_idx;

    if (vkCreateCommandPool(ctx.vk->device, &command_pool_ci, nullptr, &ctx.command_pool) != VK_SUCCESS) {
        throw std::runtime_error("Could not create compute command pool.");
    }
}

void compute_finalize(const VulkanComputeContext& ctx) {
    vkDestroyCommandPool(ctx.vk->device, ctx.command_pool, nullptr);
}
