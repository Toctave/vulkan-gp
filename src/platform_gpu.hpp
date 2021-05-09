#pragma once

#include <cstring>

#if 1

#include "vulkan/gpu.hpp"
#include "vulkan/graphics.hpp"
#include "vulkan/compute.hpp"

using GPUContext = VulkanContext;
using GraphicsContext = VulkanGraphicsContext;
using GraphicsFrame = VulkanFrame;

template<typename T>
using GPUBuffer = VulkanBuffer<T>;

#endif

void gpu_init(GPUContext& ctx);
void gpu_finalize(GPUContext& ctx);

void graphics_init(const GPUContext* ctx, const WMContext* wm, GraphicsContext& graphics);
void graphics_finalize(GraphicsContext& graphics);
void graphics_wait_idle(const GraphicsContext& graphics);

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

