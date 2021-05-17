#include "../render.hpp"
#include "../memory_util.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <iostream>

VulkanFrame begin_frame(VulkanGraphicsContext& ctx) {
    VulkanFrame frame;
    frame.frame_index = ctx.next_frame;
    frame.command_buffer = ctx.command_buffers[frame.frame_index % ctx.swapchain.images.size()];
    frame.pipeline_layout = ctx.pipeline_layout;
    
    ctx.next_frame++;
    
    uint32_t current_frame_in_flight = frame.frame_index % MAX_FRAMES_IN_FLIGHT;
        
    // Wait until the current frame is done rendering.
    vkWaitForFences(ctx.vk->device, 1, &ctx.frame_finished[current_frame_in_flight], VK_TRUE, UINT64_MAX);

    VkResult acquire_result; 
    do {
        acquire_result = vkAcquireNextImageKHR(ctx.vk->device,
                                               ctx.swapchain.handle,
                                               0,
                                               ctx.swapchain_image_ready[current_frame_in_flight],
                                               VK_NULL_HANDLE,
                                               &frame.image_index);
            
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

    // Make sure we're not rendering to an image that is being used by another in-flight frame
    if (ctx.swapchain.frames[frame.image_index] >= 0) {
        vkWaitForFences(ctx.vk->device,
                        1,
                        &ctx.frame_finished[ctx.swapchain.frames[frame.image_index] % MAX_FRAMES_IN_FLIGHT],
                        VK_TRUE,
                        UINT64_MAX);
    }
    ctx.swapchain.frames[frame.image_index] = frame.frame_index;
    
    // Begin recording command buffer
    VkCommandBufferBeginInfo command_buffer_bi{};
    command_buffer_bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		
    vkBeginCommandBuffer(frame.command_buffer, &command_buffer_bi);

    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = ctx.swapchain.extent.height;
    viewport.width = ctx.swapchain.extent.width;
    viewport.height = -static_cast<float>(ctx.swapchain.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissors;
    scissors.offset = { 0, 0 };
    scissors.extent = ctx.swapchain.extent;
    
    vkCmdSetViewport(frame.command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(frame.command_buffer, 0, 1, &scissors);
    
    vkCmdBindPipeline(frame.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.pipeline);

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
    render_pass_bi.renderPass = ctx.swapchain.render_pass;
    render_pass_bi.framebuffer = ctx.swapchain.framebuffers[frame.image_index];
    render_pass_bi.renderArea = {{0, 0}, ctx.swapchain.extent};
    render_pass_bi.clearValueCount = clear_values.size();
    render_pass_bi.pClearValues = clear_values.data();

    VkImageSubresourceRange image_range{};
    image_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_range.levelCount = 1;
    image_range.layerCount = 1;

    vkCmdBeginRenderPass(frame.command_buffer, &render_pass_bi, VK_SUBPASS_CONTENTS_INLINE);

    return frame;
}

void end_frame(const VulkanGraphicsContext &ctx, GraphicsFrame &frame) {
    // Finish recording command buffer
    vkCmdEndRenderPass(frame.command_buffer);
    vkEndCommandBuffer(frame.command_buffer);

    // Submit command buffer
    VkQueue queue;
    vkGetDeviceQueue(ctx.vk->device, ctx.vk->graphics_queue_idx, 0, &queue);

    uint32_t current_frame_in_flight = frame.frame_index % MAX_FRAMES_IN_FLIGHT;    
    
    VkPipelineStageFlags submit_wait_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &frame.command_buffer;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &ctx.swapchain_image_ready[current_frame_in_flight];
    submit_info.pWaitDstStageMask = &submit_wait_stage_mask;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &ctx.swapchain_submit_done[current_frame_in_flight];

    vkResetFences(ctx.vk->device, 1, &ctx.frame_finished[current_frame_in_flight]);
    if (vkQueueSubmit(queue, 1, &submit_info, ctx.frame_finished[current_frame_in_flight]) != VK_SUCCESS) {
        throw std::runtime_error("Could not submit commands.");
    }

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &ctx.swapchain.handle;
    present_info.pImageIndices = &frame.image_index;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &ctx.swapchain_submit_done[current_frame_in_flight];

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
}

void draw_mesh(const GraphicsFrame& frame,
               const GPUMesh& mesh) {
    VkBuffer bind_buffers[] = {
        mesh.vertex_buffer.handle,
        mesh.color_buffer.handle,
    };
    VkDeviceSize bind_offsets[] = {
        0,
        0,
    };
    
    vkCmdBindVertexBuffers(frame.command_buffer,
                           0,
                           ARRAY_SIZE(bind_buffers),
                           bind_buffers,
                           bind_offsets);
    vkCmdBindIndexBuffer(frame.command_buffer, mesh.index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);
	
    vkCmdDrawIndexed(frame.command_buffer, mesh.index_buffer.count, 1, 0, 0, 0);
}

void draw_model(const GraphicsFrame& frame,
                const glm::mat4& view,
                const glm::mat4& proj,
                const GPUModel& model) {
    PushMatrices push;
    push.model_view = view * model.transform;
    push.mvp = proj * push.model_view;

    vkCmdPushConstants(frame.command_buffer,
                       frame.pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT,
                       0,
                       sizeof(PushMatrices),
                       &push);
    
    draw_mesh(frame, *model.mesh);
}
