#pragma once

#include "platform.hpp"
#include "render.hpp"

#include <iostream>

struct VulkanComputeContext {
    VulkanContext vk;

    VkCommandPool command_pool;
};

template<typename... Args>
struct VulkanComputeKernel {
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;

    VkShaderModule module;

    VkDescriptorPool descriptor_pool;
    VkDescriptorSetLayout descriptor_set_layout;

    VkDescriptorSet descriptor_set;    
};

template<typename Arg>
struct DescriptorType {
    static const VkDescriptorType value;
};

template<typename T>
struct DescriptorType<GPUBuffer<T>> {
    static const VkDescriptorType value = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
};


template<typename Arg0>
void setup_bindings_base(VkDescriptorSetLayoutBinding* bindings, uint32_t idx) {
    bindings[idx].binding = idx;
    bindings[idx].descriptorType = DescriptorType<Arg0>::value;
    bindings[idx].descriptorCount = 1;
    bindings[idx].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[idx].pImmutableSamplers = nullptr;
}

template<typename... Args>
void setup_bindings(VkDescriptorSetLayoutBinding* bindings) {
    using dummy = int[];

    uint32_t idx = 0;
    dummy{ (setup_bindings_base<Args>(bindings, idx++), 0)... };
    // setup_bindings<Args...>(bindings, idx + 1);
}

template<typename Arg0>
void setup_sizes_base(std::vector<VkDescriptorPoolSize>& sizes) {
    VkDescriptorType type = DescriptorType<Arg0>::value;

    for (VkDescriptorPoolSize& size : sizes) {
        if (size.type == type) {
            size.descriptorCount++;
            return;
        }
    }

    VkDescriptorPoolSize size;
    size.type = type;
    size.descriptorCount = 1;
    sizes.push_back(size);
}

template<typename... Args>
void setup_sizes(std::vector<VkDescriptorPoolSize>& sizes) {
    using dummy = int[];
    
    dummy{ (setup_sizes_base<Args>(sizes), 0)... };
}


template<typename... Args>
VulkanComputeKernel<Args...> compute_kernel_create(const VulkanComputeContext& ctx,
                                                   const std::string& source_filename) {
    VulkanComputeKernel<Args...> kernel;
    
    VkDescriptorSetLayoutBinding bindings[sizeof...(Args)];
    setup_bindings<Args...>(bindings);

    VkDescriptorSetLayoutCreateInfo set_layout_ci{};
    set_layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set_layout_ci.bindingCount = sizeof...(Args);
    set_layout_ci.pBindings = bindings;

    if (vkCreateDescriptorSetLayout(ctx.vk.device,
                                    &set_layout_ci,
                                    nullptr,
                                    &kernel.descriptor_set_layout) != VK_SUCCESS) {
        throw std::runtime_error("Could not create descriptor set layout.");
    }
    
    VkPipelineLayoutCreateInfo layout_ci{};
    layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_ci.setLayoutCount = 1;
    layout_ci.pSetLayouts = &kernel.descriptor_set_layout;

    if (vkCreatePipelineLayout(ctx.vk.device, &layout_ci, nullptr, &kernel.pipeline_layout) != VK_SUCCESS) {
        throw std::runtime_error("Could not create pipeline layout.");
    }

    kernel.module = create_shader_module(ctx.vk.device, source_filename);

    VkComputePipelineCreateInfo pipeline_ci{};
    pipeline_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_ci.layout = kernel.pipeline_layout;
    
    pipeline_ci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline_ci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipeline_ci.stage.module = kernel.module;
    pipeline_ci.stage.pName = "main";

    if (vkCreateComputePipelines(ctx.vk.device,
                                 VK_NULL_HANDLE,
                                 1,
                                 &pipeline_ci,
                                 nullptr,
                                 &kernel.pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Could not create pipeline.");
    }

    std::vector<VkDescriptorPoolSize> sizes;
    setup_sizes<Args...>(sizes);
        
    VkDescriptorPoolCreateInfo descriptor_pool_ci{};
    descriptor_pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptor_pool_ci.maxSets = 1;
    descriptor_pool_ci.poolSizeCount = sizes.size();
    descriptor_pool_ci.pPoolSizes = sizes.data();

    if (vkCreateDescriptorPool(ctx.vk.device,
                               &descriptor_pool_ci,
                               nullptr,
                               &kernel.descriptor_pool) != VK_SUCCESS) {
        throw std::runtime_error("Could not create compute descriptor pool.");
    }

    return kernel;
}

template<typename... Args>
void compute_kernel_destroy(const VulkanComputeContext& ctx,
                            VulkanComputeKernel<Args...>& kernel) {
    vkDestroyDescriptorPool(ctx.vk.device, kernel.descriptor_pool, nullptr);
    vkDestroyPipeline(ctx.vk.device, kernel.pipeline, nullptr);
    vkDestroyPipelineLayout(ctx.vk.device, kernel.pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(ctx.vk.device, kernel.descriptor_set_layout, nullptr);
    vkDestroyShaderModule(ctx.vk.device, kernel.module, nullptr);
}

template<typename Arg>
struct UpdateDescriptorSet {
    static void f(const VulkanComputeContext& ctx,
                  VkDescriptorSet set,
                  uint32_t binding,
                  Arg arg);
};

template<typename T>
struct UpdateDescriptorSet<GPUBuffer<T>> {
    static void f(const VulkanComputeContext& ctx,
                  VkDescriptorSet set,
                  uint32_t binding,
                  GPUBuffer<T> buf) {
        
        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = buf.handle;
        buffer_info.offset = 0;
        buffer_info.range = sizeof(T) * buf.count;
    
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.pBufferInfo = &buffer_info;
        write.dstSet = set;
        write.dstBinding = binding;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        
        vkUpdateDescriptorSets(ctx.vk.device, 1, &write, 0, nullptr);
    }
};

template<typename... Args>
void update_descriptor_set(const VulkanComputeContext& ctx,
                           VkDescriptorSet set,
                           Args... args) {
    using dummy = int[];

    uint32_t binding = 0;
    dummy{ (UpdateDescriptorSet<Args>::f(ctx, set, binding++, args), 0)... };
}

template<typename... Args>
void compute_kernel_invoke(const VulkanComputeContext& ctx,
                           const VulkanComputeKernel<Args...>& kernel,
                           uint32_t group_count_x,
                           uint32_t group_count_y,
                           uint32_t group_count_z,
                           Args... args) {
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
    descriptor_set_ai.descriptorPool = kernel.descriptor_pool;
    descriptor_set_ai.descriptorSetCount = 1;
    descriptor_set_ai.pSetLayouts = &kernel.descriptor_set_layout;
    
    if (vkAllocateDescriptorSets(ctx.vk.device, &descriptor_set_ai, &descriptor_set) != VK_SUCCESS) {
        throw std::runtime_error("Could not allocate compute descriptor set.");
    }

    update_descriptor_set(ctx, descriptor_set, args...);

    
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    
    vkBeginCommandBuffer(command_buffer, &begin_info);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, kernel.pipeline);

    vkCmdBindDescriptorSets(command_buffer,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            kernel.pipeline_layout,
                            0,
                            1,
                            &descriptor_set,
                            0,
                            nullptr);
    
    vkCmdDispatch(command_buffer, group_count_x, group_count_y, group_count_z);

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

void compute_init(const VulkanContext &vk, VulkanComputeContext &ctx);
void compute_finalize(const VulkanComputeContext& ctx);
void test_compute(const VulkanComputeContext& ctx, const GPUMesh& mesh);

