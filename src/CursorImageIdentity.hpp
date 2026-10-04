#pragma once
#include <cstdint>
#include <cstring>
#include <span>

// Ignore transparent padding and RGB rounding on premultiplied antialiased edges.
// All frames of a theme animation are indexed under the same shape identity.
inline uint64_t cursorImageFingerprint(std::span<const uint8_t> pixels, int width, int height, unsigned stride) {
    if (width <= 0 || height <= 0 || width > 512 || height > 512 ||
        stride < unsigned(width) * 4 || pixels.size() < size_t(stride) * height) return 0;
    auto pixel = [&](int x, int y) {
        uint32_t value;
        std::memcpy(&value, pixels.data() + size_t(y) * stride + x * 4, 4);
        return value;
    };
    int left = width, top = height, right = -1, bottom = -1;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            if (pixel(x, y) >> 24) {
                if (x < left) left = x;
                if (x > right) right = x;
                if (y < top) top = y;
                if (y > bottom) bottom = y;
            }
    if (right < left) return 0;
    uint64_t hash = 14695981039346656037ULL;
    auto add = [&](uint32_t value) { hash = (hash ^ value) * 1099511628211ULL; };
    add(right - left + 1); add(bottom - top + 1);
    for (int y = top; y <= bottom; ++y)
        for (int x = left; x <= right; ++x) {
            auto value = pixel(x, y);
            add(value >> 24 == 255 ? value : value & 0xff000000);
        }
    return hash ? hash : 1;
}
