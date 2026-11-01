#pragma once
#include <cstdint>
#include <iosfwd>
#include <string>

namespace kinectcam {
struct Options {
    std::string rgb_dev = "/dev/video10", ir_dev = "/dev/video11", depth_dev = "/dev/video12";
    std::string pipeline = "cpu", serial;
    std::string ir_format = "grey", depth_format = "y16";
    bool rgb = true, ir = true, depth = true;
    bool help = false, list = false, test_pattern = false;
    float ir_gain = 4.0f, ir_gamma = 1.0f, depth_max_mm = 4500.0f;
    std::uint64_t frames = 0;
};
Options parse_options(int argc, char** argv);
void print_usage(std::ostream& out, const char* program);
}
