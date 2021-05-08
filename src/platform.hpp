#pragma once

#include <cstring>

enum GPUBufferUsageFlags {
    VERTEX_BUFFER  = 0x00000001,
    INDEX_BUFFER   = 0x00000002,
    UNIFORM_BUFFER = 0x00000004,
    STORAGE_BUFFER = 0x00000008,
    GRAPHICS       = 0x00000010,
    COMPUTE        = 0x00000020,
};

#if 1

#include "platform_vulkan.hpp"
using GPUContext = VulkanContext;
using GraphicsContext = VulkanGraphicsContext;
using GraphicsFrame = VulkanFrame;

template<typename T>
using GPUBuffer = VulkanBuffer<T>;

#endif

void graphics_init(GraphicsContext& ctx);
void graphics_finalize(GraphicsContext& ctx);
void graphics_wait_idle(const GraphicsContext& ctx);

template<typename T>
GPUBuffer<T> allocate_and_fill_buffer(const GPUContext& ctx,
                                      const T* data,
                                      size_t count,
                                      uint32_t usage) {
    GPUBuffer<T> buf = gpu_buffer_allocate<T>(ctx, usage, count);

    T* device_data = gpu_buffer_map(ctx, buf);

    memcpy(device_data, data, sizeof(T) * count);

    gpu_buffer_unmap(ctx, buf);

    return buf;
}

