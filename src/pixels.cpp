#include "pixels.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace kinectcam {
namespace {
std::uint8_t byte(int v) { return static_cast<std::uint8_t>(std::clamp(v, 0, 255)); }
void require_even(std::size_t n) {
    if (n % 2) throw std::invalid_argument("YUYV requires an even pixel count");
}
}
void bgrx_to_yuyv(const std::uint8_t* src, std::size_t pixels,
                  std::uint8_t* dst, bool rgbx) {
    require_even(pixels);
    for (std::size_t i = 0; i < pixels; i += 2) {
        const int r0 = src[4*i + (rgbx ? 0 : 2)], g0 = src[4*i+1];
        const int b0 = src[4*i + (rgbx ? 2 : 0)];
        const int r1 = src[4*i+4 + (rgbx ? 0 : 2)], g1 = src[4*i+5];
        const int b1 = src[4*i+4 + (rgbx ? 2 : 0)];
        dst[2*i] = byte(((66*r0 + 129*g0 + 25*b0 + 128) >> 8) + 16);
        dst[2*i+2] = byte(((66*r1 + 129*g1 + 25*b1 + 128) >> 8) + 16);
        // One shared chroma sample per pair, averaged before quantization.
        dst[2*i+1] = byte(((-38*(r0+r1) - 74*(g0+g1) + 112*(b0+b1) + 256) >> 9) + 128);
        dst[2*i+3] = byte(((112*(r0+r1) - 94*(g0+g1) - 18*(b0+b1) + 256) >> 9) + 128);
    }
}
void ir_to_gray(const float* src, std::size_t pixels, std::uint8_t* dst,
                float gain, float gamma) {
    for (std::size_t i = 0; i < pixels; ++i) {
        if (!std::isfinite(src[i]) || src[i] <= 0) { dst[i] = 0; continue; }
        const float v = std::clamp((src[i] / 65535.0f) * gain, 0.0f, 1.0f);
        dst[i] = byte(static_cast<int>(std::lround(std::pow(v, 1.0f/gamma) * 255.0f)));
    }
}
void gray_to_yuyv(const std::uint8_t* src, std::size_t pixels, std::uint8_t* dst) {
    require_even(pixels);
    for (std::size_t i = 0; i < pixels; ++i) {
        dst[2*i] = static_cast<std::uint8_t>(16 + (219 * src[i] + 127) / 255);
        dst[2*i+1] = 128;
    }
}
void depth_to_y16(const float* src, std::size_t pixels, std::uint8_t* dst) {
    for (std::size_t i = 0; i < pixels; ++i) {
        const auto mm = (!std::isfinite(src[i]) || src[i] <= 0) ? 0u :
            static_cast<unsigned>(std::lround(std::min(src[i], 65535.0f)));
        dst[2*i] = static_cast<std::uint8_t>(mm & 255u);
        dst[2*i+1] = static_cast<std::uint8_t>(mm >> 8);
    }
}
void depth_to_gray(const float* src, std::size_t pixels, std::uint8_t* dst,
                   float max_mm) {
    for (std::size_t i = 0; i < pixels; ++i) {
        dst[i] = (!std::isfinite(src[i]) || src[i] <= 0) ? 0 :
            byte(static_cast<int>(std::lround(std::min(src[i] / max_mm, 1.0f) * 255.0f)));
    }
}
}
