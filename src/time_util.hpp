#pragma once

#include <chrono>

static inline uint64_t now() {
    static auto t0 = std::chrono::high_resolution_clock::now();
    auto t1 = std::chrono::high_resolution_clock::now();

    return std::chrono::duration<uint64_t, std::nano>(t1 - t0).count();
}

static inline double now_ms() {
    return now() / (1000.0 * 1000.0);
}

static inline double now_seconds() {
    return now_ms() / 1000.0;
}

