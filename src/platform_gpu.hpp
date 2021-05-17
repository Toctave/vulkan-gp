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
void gpu_buffer_copy(const GPUContext& ctx, GPUBuffer<T>& buffer, const T* data, size_t offset, size_t count) {
    T* device_ptr = gpu_buffer_map(ctx, buffer, offset, count);

    memcpy(device_ptr, data, sizeof(T) * count);

    gpu_buffer_unmap(ctx, buffer);
}
