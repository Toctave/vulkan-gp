#pragma once

#include <cstring>

enum GPUBufferUsage {
    VERTEX_BUFFER,
    INDEX_BUFFER,
};

#if 1

#include "platform_vulkan.hpp"
using GraphicsContext = VulkanContext;
using GraphicsFrame = VulkanFrame;

template<typename T>
using GPUBuffer = VulkanBuffer<T>;

#endif

void graphics_init(GraphicsContext& ctx);
void graphics_finalize(GraphicsContext& ctx);
void graphics_wait_idle(const GraphicsContext& ctx);

template<typename T>
GPUBuffer<T> allocate_and_fill_buffer(const GraphicsContext& ctx,
                                      const T* data,
                                      size_t count,
                                      GPUBufferUsage usage) {
    GPUBuffer<T> buf = gpu_buffer_allocate<T>(ctx, usage, count);

    T* device_data = gpu_buffer_map(ctx, buf);

    memcpy(device_data, data, sizeof(T) * count);

    gpu_buffer_unmap(ctx, buf);

    return buf;
}

