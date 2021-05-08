#include "platform.hpp"

#include "compute_vulkan.hpp"

#include "memory_util.hpp"

void compute_init(const VulkanContext &vk, VulkanComputeContext &ctx) {
    ctx.vk = vk;

    VkDescriptorSetLayoutBinding bindings[] = {
        {
            0,                                 // binding
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // type
            1,                                 // count
            VK_SHADER_STAGE_COMPUTE_BIT,       // stage
            nullptr,                           // immutable samplers
        }
    };

    VkDescriptorSetLayoutCreateInfo set_layout_ci{};
    set_layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set_layout_ci.bindingCount = ARRAY_SIZE(bindings);
    set_layout_ci.pBindings = bindings;
    
    if (vkCreateDescriptorSetLayout(vk.device, &set_layout_ci, nullptr, &ctx.descriptor_set_layout) != VK_SUCCESS) {
        throw std::runtime_error("Could not create descriptor set layout.");
    }
    
    VkPipelineLayoutCreateInfo layout_ci{};
    layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_ci.setLayoutCount = 1;
    layout_ci.pSetLayouts = &ctx.descriptor_set_layout;

    if (vkCreatePipelineLayout(vk.device, &layout_ci, nullptr, &ctx.pipeline_layout) != VK_SUCCESS) {
        throw std::runtime_error("Could not create pipeline layout.");
    }

    VkComputePipelineCreateInfo pipeline_ci{};
    pipeline_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_ci.layout = ctx.pipeline_layout;
    
    pipeline_ci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline_ci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipeline_ci.stage.module = create_shader_module(vk.device, "shaders/test.comp.spv");
    pipeline_ci.stage.pName = "main";

    ctx.kernels.push_back(pipeline_ci.stage.module);

    if (vkCreateComputePipelines(vk.device,
                                 VK_NULL_HANDLE,
                                 1,
                                 &pipeline_ci,
                                 nullptr,
                                 &ctx.pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Could not create pipeline layout.");
    }

    VkCommandPoolCreateInfo command_pool_ci{};
    command_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_ci.queueFamilyIndex = vk.compute_queue_idx;

    if (vkCreateCommandPool(vk.device, &command_pool_ci, nullptr, &ctx.command_pool) != VK_SUCCESS) {
        throw std::runtime_error("Could not create compute command pool.");
    }

    VkDescriptorPoolSize sizes[] = {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
    };
    VkDescriptorPoolCreateInfo descriptor_pool_ci{};
    descriptor_pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptor_pool_ci.maxSets = 1;
    descriptor_pool_ci.poolSizeCount = ARRAY_SIZE(sizes);
    descriptor_pool_ci.pPoolSizes = sizes;

    if (vkCreateDescriptorPool(vk.device, &descriptor_pool_ci, nullptr, &ctx.descriptor_pool) != VK_SUCCESS) {
        throw std::runtime_error("Could not create compute descriptor pool.");
    }
}

void compute_finalize(const VulkanComputeContext& ctx) {
    for (VkShaderModule kernel : ctx.kernels) {
        vkDestroyShaderModule(ctx.vk.device, kernel, nullptr);
    }
    vkDestroyPipeline(ctx.vk.device, ctx.pipeline, nullptr);
    vkDestroyPipelineLayout(ctx.vk.device, ctx.pipeline_layout, nullptr);

    vkDestroyCommandPool(ctx.vk.device, ctx.command_pool, nullptr);

    vkDestroyDescriptorPool(ctx.vk.device, ctx.descriptor_pool, nullptr);
    vkDestroyDescriptorSetLayout(ctx.vk.device, ctx.descriptor_set_layout, nullptr);
}

void test_compute(const VulkanComputeContext& ctx, const GPUMesh& mesh) {
    VkCommandBufferAllocateInfo command_buffer_ai{};
    command_buffer_ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_ai.commandPool = ctx.command_pool;
    command_buffer_ai.commandBufferCount = 1;
    command_buffer_ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    VkCommandBuffer command_buffer;

    if (vkAllocateCommandBuffers(ctx.vk.device, &command_buffer_ai, &command_buffer) != VK_SUCCESS) {
        throw std::runtime_error("Could not allocate compute command buffer.");
    }

    VkDescriptorSet descriptor_set;
    VkDescriptorSetAllocateInfo descriptor_set_ai{};
    descriptor_set_ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptor_set_ai.descriptorPool = ctx.descriptor_pool;
    descriptor_set_ai.descriptorSetCount = 1;
    descriptor_set_ai.pSetLayouts = &ctx.descriptor_set_layout;
    
    if (vkAllocateDescriptorSets(ctx.vk.device, &descriptor_set_ai, &descriptor_set) != VK_SUCCESS) {
        throw std::runtime_error("Could not allocate compute descriptor set.");
    }
    
    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = mesh.vertex_buffer.handle;
    buffer_info.offset = 0;
    buffer_info.range = sizeof(Vertex) * mesh.vertex_buffer.count;
    
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pBufferInfo = &buffer_info;
    write.dstSet = descriptor_set;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        
    vkUpdateDescriptorSets(ctx.vk.device, 1, &write, 0, nullptr);

    
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    
    vkBeginCommandBuffer(command_buffer, &begin_info);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, ctx.pipeline);

    vkCmdBindDescriptorSets(command_buffer,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            ctx.pipeline_layout,
                            0,
                            1,
                            &descriptor_set,
                            0,
                            nullptr);
    
    vkCmdDispatch(command_buffer, mesh.vertex_buffer.count, 1, 1);

    vkEndCommandBuffer(command_buffer);

    VkQueue queue;
    vkGetDeviceQueue(ctx.vk.device, ctx.vk.compute_queue_idx, 0, &queue);

    VkFence submit_done;
    VkFenceCreateInfo submit_done_ci{};
    submit_done_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(ctx.vk.device, &submit_done_ci, nullptr, &submit_done);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    
    vkQueueSubmit(queue, 1, &submit_info, submit_done);

    vkWaitForFences(ctx.vk.device, 1, &submit_done, VK_TRUE, UINT64_MAX);
    vkDestroyFence(ctx.vk.device, submit_done, nullptr);
}
