#pragma once

#include "platform.hpp"
#include "render.hpp"

struct VulkanComputeContext {
    VulkanContext vk;

    VkCommandPool command_pool;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    std::vector<VkShaderModule> kernels;

    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set;
    VkDescriptorSetLayout descriptor_set_layout;
};

void compute_init(const VulkanContext &vk, VulkanComputeContext &ctx);
void compute_finalize(const VulkanComputeContext& ctx);
void test_compute(const VulkanComputeContext& ctx, const GPUMesh& mesh);



