#pragma once
#include <cstddef>
#include <cstdint>

namespace kinectcam {
void bgrx_to_yuyv(const std::uint8_t* src, std::size_t pixels,
                  std::uint8_t* dst, bool rgbx);
void ir_to_gray(const float* src, std::size_t pixels, std::uint8_t* dst,
                float gain, float gamma);
void gray_to_yuyv(const std::uint8_t* src, std::size_t pixels, std::uint8_t* dst);
void depth_to_y16(const float* src, std::size_t pixels, std::uint8_t* dst);
void depth_to_gray(const float* src, std::size_t pixels, std::uint8_t* dst,
                   float max_mm);
}
