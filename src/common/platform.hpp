#pragma once

#include <cstdint>

enum GPUBufferUsageFlags : uint32_t {
    VERTEX_BUFFER  = 0x00000001,
    INDEX_BUFFER   = 0x00000002,
    UNIFORM_BUFFER = 0x00000004,
    STORAGE_BUFFER = 0x00000008,
    GRAPHICS       = 0x00000010,
    COMPUTE        = 0x00000020,
};

