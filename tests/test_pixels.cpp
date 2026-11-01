// Fixtures catch loss of depth precision, invalid float casts, channel order,
// wrong chroma subsampling, and IR brightness regression.
#include "pixels.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

static void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
int main() {
    using namespace kinectcam;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    const std::array<float, 9> depth{0, -1, nan, inf, 1, 1000.4f, 1000.6f, 4500, 70000};
    std::vector<std::uint8_t> out(depth.size() * 2);
    depth_to_y16(depth.data(), depth.size(), out.data());
    require(out == std::vector<std::uint8_t>({0,0,0,0,0,0,0,0,1,0,232,3,233,3,148,17,255,255}),
            "depth must preserve rounded little-endian millimetres and zero invalid data");
    std::array<float, 6> ir{nan, inf, -1, 0, 32767.5f, 65535};
    out.resize(ir.size());
    ir_to_gray(ir.data(), ir.size(), out.data(), 1.0f, 1.0f);
    require(out == std::vector<std::uint8_t>({0,0,0,0,128,255}), "IR clips safely");
    ir_to_gray(ir.data(), ir.size(), out.data(), 2.0f, 1.0f);
    require(out[4] == 255, "IR gain must brighten");
    const float quarter = 65535.0f / 4;
    ir_to_gray(&quarter, 1, out.data(), 1.0f, 2.0f);
    require(out[0] == 128, "gamma 2 must brighten quarter intensity to half");

    const std::array<std::uint8_t, 8> black_white{0,0,0,0,255,255,255,0};
    out.resize(4);
    bgrx_to_yuyv(black_white.data(), 2, out.data(), false);
    require(out == std::vector<std::uint8_t>({16,128,235,128}), "limited range black/white");
    const std::array<std::uint8_t, 8> red_blue{0,0,255,0,255,0,0,0};
    bgrx_to_yuyv(red_blue.data(), 2, out.data(), false);
    require(out == std::vector<std::uint8_t>({82,165,41,175}), "chroma must average both pixels");
    const std::array<std::uint8_t, 8> rgbx_red_blue{255,0,0,0,0,0,255,0};
    bgrx_to_yuyv(rgbx_red_blue.data(), 2, out.data(), true);
    require(out == std::vector<std::uint8_t>({82,165,41,175}), "RGBX channel order");
    const std::array<std::uint8_t, 2> gray{0,255};
    gray_to_yuyv(gray.data(), 2, out.data());
    require(out == std::vector<std::uint8_t>({16,128,235,128}), "IR YUYV maps range");
    bool rejected = false;
    try { bgrx_to_yuyv(black_white.data(), 1, out.data(), false); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "odd YUYV pixel counts must be rejected");
    const std::array<float, 4> preview{nan,0,2250,4500};
    depth_to_gray(preview.data(), 4, out.data(), 4500);
    require(out == std::vector<std::uint8_t>({0,0,128,255}), "depth preview scale");
    std::cout << "pixel fixtures passed\n";
}
