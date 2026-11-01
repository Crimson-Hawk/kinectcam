#include "options.hpp"
#include <libfreenect2/config.h>
#include <algorithm>
#include <cmath>
#include <ostream>
#include <stdexcept>
#include <vector>

namespace kinectcam {
Options parse_options(int argc, char** argv) {
    Options o;
    auto number = [](const std::string& text, const std::string& name) {
        std::size_t end = 0;
        float value;
        try { value = std::stof(text, &end); }
        catch (...) { throw std::invalid_argument(name + " requires a positive finite number"); }
        if (end != text.size() || !std::isfinite(value) || value <= 0 || value > 1000000)
            throw std::invalid_argument(name + " requires a positive finite number <= 1000000");
        return value;
    };
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto value = [&]() -> std::string {
            if (i+1 == argc || argv[i+1][0] == '\0' || argv[i+1][0] == '-')
                throw std::invalid_argument(arg + " requires a value");
            return argv[++i];
        };
        if (arg == "--rgb-dev") o.rgb_dev = value();
        else if (arg == "--ir-dev") o.ir_dev = value();
        else if (arg == "--depth-dev") o.depth_dev = value();
        else if (arg == "--pipeline") o.pipeline = value();
        else if (arg == "--serial") o.serial = value();
        else if (arg == "--ir-format") o.ir_format = value();
        else if (arg == "--depth-format") o.depth_format = value();
        else if (arg == "--ir-gain") o.ir_gain = number(value(), arg);
        else if (arg == "--ir-gamma") o.ir_gamma = number(value(), arg);
        else if (arg == "--depth-max-mm") o.depth_max_mm = number(value(), arg);
        else if (arg == "--frames") {
            const auto text = value();
            if (!std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; }))
                throw std::invalid_argument("--frames requires a nonnegative integer");
            try { o.frames = std::stoull(text); }
            catch (...) { throw std::invalid_argument("--frames is out of range"); }
        }
        else if (arg == "--idle-timeout-ms") {
            const auto text = value();
            std::size_t end = 0;
            unsigned long n;
            try { n=std::stoul(text,&end); }
            catch (...) { throw std::invalid_argument("--idle-timeout-ms requires an integer in 0..60000"); }
            if (end!=text.size() || n>60000 || !std::all_of(text.begin(),text.end(),[](char c){return c>='0'&&c<='9';}))
                throw std::invalid_argument("--idle-timeout-ms requires an integer in 0..60000");
            o.idle_timeout_ms=static_cast<std::uint32_t>(n);
        }
        else if (arg == "--on-demand") o.on_demand = true;
        else if (arg == "--no-rgb") o.rgb = false;
        else if (arg == "--no-ir") o.ir = false;
        else if (arg == "--no-depth") o.depth = false;
        else if (arg == "--test-pattern") o.test_pattern = true;
        else if (arg == "--list") o.list = true;
        else if (arg == "--help" || arg == "-h") o.help = true;
        else throw std::invalid_argument("unknown option: " + arg);
    }
    if (o.help) return o;
    if (o.pipeline != "auto" && o.pipeline != "cpu" && o.pipeline != "gl" &&
        o.pipeline != "opencl" && o.pipeline != "cuda")
        throw std::invalid_argument("--pipeline must be cpu, auto, gl, opencl, or cuda");
#ifndef LIBFREENECT2_WITH_OPENGL_SUPPORT
    if (o.pipeline == "gl") throw std::invalid_argument("gl pipeline unavailable in libfreenect2 build");
#endif
#ifndef LIBFREENECT2_WITH_OPENCL_SUPPORT
    if (o.pipeline == "opencl") throw std::invalid_argument("opencl pipeline unavailable in libfreenect2 build");
#endif
#ifndef LIBFREENECT2_WITH_CUDA_SUPPORT
    if (o.pipeline == "cuda") throw std::invalid_argument("cuda pipeline unavailable in libfreenect2 build");
#endif
    if (o.ir_format != "grey" && o.ir_format != "yuyv")
        throw std::invalid_argument("--ir-format must be grey or yuyv");
    if (o.depth_format != "y16" && o.depth_format != "grey")
        throw std::invalid_argument("--depth-format must be y16 or grey");
    if (o.list) return o;
    if (!o.rgb && !o.ir && !o.depth)
        throw std::invalid_argument("enable at least one output");
    std::vector<std::string> devices;
    if (o.rgb) devices.push_back(o.rgb_dev);
    if (o.ir) devices.push_back(o.ir_dev);
    if (o.depth) devices.push_back(o.depth_dev);
    std::sort(devices.begin(), devices.end());
    if (std::adjacent_find(devices.begin(), devices.end()) != devices.end())
        throw std::invalid_argument("enabled output devices must be distinct");
    return o;
}
void print_usage(std::ostream& out, const char* program) {
    out << "Usage: " << program << " [options]\n"
        "Kinect v2 RGB/IR/depth to v4l2loopback; 30 fps, CPU by default.\n"
        "  --rgb-dev PATH       Color output (default /dev/video10, 1920x1080 YUYV)\n"
        "  --ir-dev PATH        IR output (default /dev/video11, 512x424 GREY)\n"
        "  --depth-dev PATH     Depth output (default /dev/video12, 512x424 Y16)\n"
        "  --no-rgb             Disable color output\n"
        "  --no-ir              Disable infrared output\n"
        "  --no-depth           Disable depth output\n"
        "  --ir-format FORMAT   grey or yuyv (OpenCV compatibility)\n"
        "  --ir-gain NUMBER     Positive intensity multiplier (default 4)\n"
        "  --ir-gamma NUMBER    Positive gamma; >1 brightens (default 1)\n"
        "  --depth-format FMT   y16: LE uint16 millimetres, zero invalid; grey: preview\n"
        "  --depth-max-mm NUM   White distance for grey preview (default 4500)\n"
        "  --pipeline TYPE      cpu, auto, gl, opencl, cuda (compiled features only)\n"
        "  --serial SERIAL      Select a sensor\n"
        "  --list               List connected sensor serials and exit\n"
        "  --frames N           Stop after N frame sets (0: unlimited)\n"
        "  --on-demand          Capture only while loopback clients stream\n"
        "  --idle-timeout-ms N  Grace after last client (default 1000; 0..60000)\n"
        "  --test-pattern       Synthetic frames instead of USB (testing only)\n"
        "  -h, --help           Show this help\n";
}
}
