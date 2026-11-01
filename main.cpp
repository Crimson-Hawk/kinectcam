#include "options.hpp"
#include "pixels.hpp"
#include "v4l2_output.hpp"
#include "demand.hpp"
#include <libfreenect2/libfreenect2.hpp>
#include <libfreenect2/frame_listener_impl.h>
#include <libfreenect2/packet_pipeline.h>
#include <linux/videodev2.h>
#include <sys/stat.h>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>
#include <functional>

namespace {
volatile std::sig_atomic_t running = 1;
void stop(int) { running = 0; }
constexpr std::size_t color_pixels = 1920*1080, ir_pixels = 512*424;

struct DeviceGuard {
    libfreenect2::Freenect2Device* dev;
    bool started = false;
    ~DeviceGuard() {
        if (started) dev->stop();
        dev->close();
    }
};
struct FrameGuard {
    libfreenect2::SyncMultiFrameListener& listener;
    libfreenect2::FrameMap& frames;
    ~FrameGuard() { listener.release(frames); }
};
const libfreenect2::Frame& checked_frame(const libfreenect2::FrameMap& frames,
                                       libfreenect2::Frame::Type type,
                                       std::size_t width, std::size_t height) {
    auto it = frames.find(type);
    if (it == frames.end() || !it->second || !it->second->data)
        throw std::runtime_error("missing sensor frame");
    const auto& f = *it->second;
    if (f.width != width || f.height != height || f.bytes_per_pixel != 4 || f.status != 0)
        throw std::runtime_error("sensor returned an invalid frame size or status");
    if (type == libfreenect2::Frame::Color) {
        if (f.format != libfreenect2::Frame::BGRX && f.format != libfreenect2::Frame::RGBX)
            throw std::runtime_error("unsupported sensor color format");
    } else if (f.format != libfreenect2::Frame::Float) {
        throw std::runtime_error("expected floating point sensor IR/depth");
    }
    return f;
}

class Outputs {
public:
    explicit Outputs(const kinectcam::Options& o) : options_(o) {
        // Detect aliases to the same node before configuring either writer.
        std::set<dev_t> devices;
        for (const auto& entry : std::vector<std::pair<bool,std::string>>{
                 {o.rgb,o.rgb_dev},{o.ir,o.ir_dev},{o.depth,o.depth_dev}}) {
            if (!entry.first) continue;
            struct stat st{};
            if (::stat(entry.second.c_str(), &st) == 0 && S_ISCHR(st.st_mode) &&
                !devices.insert(st.st_rdev).second)
                throw std::invalid_argument("enabled output devices must be distinct (including aliases)");
        }
        if (o.rgb) {
            rgb_ = std::make_unique<kinectcam::V4L2Output>(o.rgb_dev,1920,1080,V4L2_PIX_FMT_YUYV,2);
            rgb_buffer_.resize(color_pixels*2);
            std::cout << "Color: " << o.rgb_dev << " 1920x1080 YUYV\n";
        }
        if (o.ir) {
            const bool yuyv = o.ir_format == "yuyv";
            ir_ = std::make_unique<kinectcam::V4L2Output>(o.ir_dev,512,424,
                    yuyv ? V4L2_PIX_FMT_YUYV : V4L2_PIX_FMT_GREY, yuyv ? 2 : 1);
            ir_gray_.resize(ir_pixels);
            ir_buffer_.resize(ir_pixels*(yuyv ? 2 : 1));
            std::cout << "IR: " << o.ir_dev << " 512x424 " << o.ir_format << "\n";
        }
        if (o.depth) {
            const bool metric = o.depth_format == "y16";
            depth_ = std::make_unique<kinectcam::V4L2Output>(o.depth_dev,512,424,
                    metric ? V4L2_PIX_FMT_Y16 : V4L2_PIX_FMT_GREY, metric ? 2 : 1);
            depth_buffer_.resize(ir_pixels*(metric ? 2 : 1));
            std::cout << "Depth: " << o.depth_dev << " 512x424 " << o.depth_format
                      << (metric ? " (little-endian millimetres, 0 invalid)\n" : " (preview)\n");
        }
    }
    void publish(const unsigned char* rgb, bool rgbx, const float* ir, const float* depth) {
        using namespace kinectcam;
        if (rgb_) {
            bgrx_to_yuyv(rgb, color_pixels, rgb_buffer_.data(), rgbx);
            rgb_->write_frame(rgb_buffer_);
        }
        if (ir_) {
            ir_to_gray(ir,ir_pixels,ir_gray_.data(),options_.ir_gain,options_.ir_gamma);
            if (options_.ir_format == "yuyv") {
                gray_to_yuyv(ir_gray_.data(),ir_pixels,ir_buffer_.data());
                ir_->write_frame(ir_buffer_);
            } else ir_->write_frame(ir_gray_);
        }
        if (depth_) {
            if (options_.depth_format == "y16") depth_to_y16(depth,ir_pixels,depth_buffer_.data());
            else depth_to_gray(depth,ir_pixels,depth_buffer_.data(),options_.depth_max_mm);
            depth_->write_frame(depth_buffer_);
        }
    }
    void monitor_clients() {
        for (auto* output : {rgb_.get(),ir_.get(),depth_.get()})
            if (output) output->enable_client_monitoring();
        blank(); // Activate loopback producers without activating the Kinect.
    }
    bool has_clients() {
        bool result=false;
        for (auto* output : {rgb_.get(),ir_.get(),depth_.get()})
            if (output) result=output->has_capture_clients() || result;
        return result;
    }
    void blank() {
        for (auto* output : {rgb_.get(),ir_.get(),depth_.get()})
            if (output) output->write_blank();
    }
private:
    const kinectcam::Options& options_;
    std::unique_ptr<kinectcam::V4L2Output> rgb_, ir_, depth_;
    std::vector<unsigned char> rgb_buffer_, ir_gray_, ir_buffer_, depth_buffer_;
};

void test_pattern(const kinectcam::Options& o) {
    std::cerr << "SYNTHETIC TEST PATTERN: these frames are not from a Kinect.\n";
    Outputs outputs(o);
    if (o.on_demand) outputs.monitor_clients();
    kinectcam::DemandState demand(std::chrono::milliseconds(o.idle_timeout_ms));
    bool active=false;
    std::vector<unsigned char> rgb(o.rgb ? color_pixels*4 : 0);
    std::vector<float> ir(o.ir ? ir_pixels : 0), depth(o.depth ? ir_pixels : 0);
    auto next = std::chrono::steady_clock::now();
    std::uint64_t frame=0;
    while (running && (!o.frames || frame < o.frames)) {
        if (o.on_demand && !demand.wants_capture(outputs.has_clients(),std::chrono::steady_clock::now())) {
            if (active) { outputs.blank(); active=false; }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            next=std::chrono::steady_clock::now();
            continue;
        }
        active=true;
        if (o.rgb) {
            for (std::size_t y = 0; y < 1080; ++y)
                for (std::size_t x = 0; x < 1920; ++x) {
                    const auto i = 4*(y*1920+x);
                    rgb[i] = static_cast<unsigned char>((x+frame)%256);
                    rgb[i+1] = static_cast<unsigned char>(y%256);
                    rgb[i+2] = static_cast<unsigned char>(frame%256);
                }
        }
        for (std::size_t i = 0; i < ir_pixels; ++i) {
            if (o.ir) ir[i] = static_cast<float>((i%512)*128);
            if (o.depth) depth[i] = 500.0f + static_cast<float>((i%512)*8);
        }
        outputs.publish(rgb.data(),false,ir.data(),depth.data());
        ++frame;
        next += std::chrono::nanoseconds(1000000000/30);
        std::this_thread::sleep_until(next);
    }
    if (o.on_demand) outputs.blank();
}

void stream_session(const kinectcam::Options& o, Outputs& outputs, std::uint64_t& frames_sent,
                    const std::function<bool()>& wanted) {
    libfreenect2::Freenect2 context;
    const int count = context.enumerateDevices();
    if (count == 0) throw std::runtime_error("No Kinect v2 found; check USB 3, powered adapter and udev permissions");
    const std::string serial = o.serial.empty() ? context.getDefaultDeviceSerialNumber() : o.serial;
    std::unique_ptr<libfreenect2::PacketPipeline> pipeline;
    if (o.pipeline == "cpu") pipeline = std::make_unique<libfreenect2::CpuPacketPipeline>();
#ifdef LIBFREENECT2_WITH_OPENGL_SUPPORT
    else if (o.pipeline == "gl") pipeline = std::make_unique<libfreenect2::OpenGLPacketPipeline>();
#endif
#ifdef LIBFREENECT2_WITH_OPENCL_SUPPORT
    else if (o.pipeline == "opencl") pipeline = std::make_unique<libfreenect2::OpenCLPacketPipeline>();
#endif
#ifdef LIBFREENECT2_WITH_CUDA_SUPPORT
    else if (o.pipeline == "cuda") pipeline = std::make_unique<libfreenect2::CudaPacketPipeline>();
#endif
    int types = 0;
    if (o.rgb) types |= libfreenect2::Frame::Color;
    if (o.ir) types |= libfreenect2::Frame::Ir;
    if (o.depth) types |= libfreenect2::Frame::Depth;
    // Listener must outlive the device guard, which stops the producer first.
    libfreenect2::SyncMultiFrameListener listener(types);
    auto* dev = pipeline ? context.openDevice(serial, pipeline.release()) : context.openDevice(serial);
    if (!dev) throw std::runtime_error("failed to open Kinect " + serial);
    DeviceGuard guard{dev};
    if (o.rgb) dev->setColorFrameListener(&listener);
    if (o.ir || o.depth) dev->setIrAndDepthFrameListener(&listener);
    if (!dev->startStreams(o.rgb, o.ir || o.depth)) throw std::runtime_error("failed to start Kinect streams");
    guard.started = true;
    std::cout << "Streaming Kinect " << serial << " using " << o.pipeline << "; Ctrl+C to stop\n";
    auto last_frame = std::chrono::steady_clock::now();
    while (running && (!o.frames || frames_sent < o.frames) && wanted()) {
        libfreenect2::FrameMap frames;
        if (!listener.waitForNewFrame(frames, 250)) {
            if (std::chrono::steady_clock::now() - last_frame > std::chrono::seconds(5))
                throw std::runtime_error("Kinect delivered no frames for 5 seconds; reconnect the sensor");
            continue;
        }
        FrameGuard release{listener,frames};
        if (!running || !wanted()) break;
        const libfreenect2::Frame *rgb = nullptr, *ir = nullptr, *depth = nullptr;
        if (o.rgb) rgb = &checked_frame(frames,libfreenect2::Frame::Color,1920,1080);
        if (o.ir) ir = &checked_frame(frames,libfreenect2::Frame::Ir,512,424);
        if (o.depth) depth = &checked_frame(frames,libfreenect2::Frame::Depth,512,424);
        outputs.publish(rgb ? rgb->data : nullptr, rgb && rgb->format == libfreenect2::Frame::RGBX,
                        ir ? reinterpret_cast<const float*>(ir->data) : nullptr,
                        depth ? reinterpret_cast<const float*>(depth->data) : nullptr);
        last_frame = std::chrono::steady_clock::now();
        ++frames_sent;
    }
    // Clear before DeviceGuard stops/closes USB: a client can reopen during
    // that shutdown, and must never receive the previous session's image.
    if (o.on_demand) outputs.blank();
    std::cout << "Stopped after " << frames_sent << " frame sets\n";
}
int capture(const kinectcam::Options& o) {
    if (o.list) {
        libfreenect2::Freenect2 context;
        const int count=context.enumerateDevices();
        for (int i=0; i<count; ++i) std::cout << context.getDeviceSerialNumber(i) << '\n';
        return 0;
    }
    Outputs outputs(o);
    std::uint64_t frames_sent=0;
    if (!o.on_demand) { stream_session(o,outputs,frames_sent,[]{return true;}); return 0; }
    outputs.monitor_clients();
    kinectcam::DemandState demand(std::chrono::milliseconds(o.idle_timeout_ms));
    auto wanted=[&] { return demand.wants_capture(outputs.has_clients(),std::chrono::steady_clock::now()); };
    std::cout << "On-demand: sensor idle; waiting for capture clients\n" << std::flush;
    while (running && (!o.frames || frames_sent<o.frames)) {
        if (!wanted()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        std::cout << "Capture client detected; starting sensor\n" << std::flush;
        stream_session(o,outputs,frames_sent,wanted);
        outputs.blank();
        std::cout << "Sensor stopped and closed; loopbacks remain available\n" << std::flush;
    }
    return 0;
}
}
int main(int argc, char** argv) {
    try {
        const auto options = kinectcam::parse_options(argc,argv);
        if (options.help) { kinectcam::print_usage(std::cout,argv[0]); return 0; }
        std::signal(SIGINT,stop); std::signal(SIGTERM,stop);
        if (options.test_pattern && !options.list) { test_pattern(options); return 0; }
        return capture(options);
    } catch (const std::exception& e) {
        std::cerr << "kinectcam: " << e.what() << '\n';
        return 1;
    }
}
