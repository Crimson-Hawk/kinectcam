#include "v4l2_output.hpp"
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace kinectcam {
namespace {
// v4l2loopback private ABI, also used by its upstream client-usage API.
constexpr std::uint32_t client_usage_event = V4L2_EVENT_PRIVATE_START + 0x08E00000 + 1;
int xioctl(int fd, unsigned long request, void* arg) {
    int rc;
    do { rc = ::ioctl(fd, request, arg); } while (rc < 0 && errno == EINTR);
    return rc;
}
std::runtime_error error(const std::string& path, const std::string& operation) {
    return std::runtime_error(path + ": " + operation + ": " + std::strerror(errno));
}
}
V4L2Output::V4L2Output(std::string path, std::uint32_t width, std::uint32_t height,
                       std::uint32_t fourcc, std::uint32_t bytes_per_pixel)
    : path_(std::move(path)), row_bytes_(std::size_t(width) * bytes_per_pixel), height_(height), fourcc_(fourcc) {
    fd_ = ::open(path_.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd_ < 0) throw error(path_, "open (run setup-loopback.sh and check video permissions)");
    try {
        struct stat st{};
        if (::fstat(fd_, &st) < 0) throw error(path_, "fstat");
        if (!S_ISCHR(st.st_mode)) throw std::runtime_error(path_ + ": expected a character video device");
        v4l2_capability caps{};
        if (xioctl(fd_, VIDIOC_QUERYCAP, &caps) < 0) throw error(path_, "VIDIOC_QUERYCAP");
        const auto flags = (caps.capabilities & V4L2_CAP_DEVICE_CAPS) ? caps.device_caps : caps.capabilities;
        if (std::string(reinterpret_cast<char*>(caps.driver), strnlen(reinterpret_cast<char*>(caps.driver), sizeof(caps.driver))) != "v4l2 loopback")
            throw std::runtime_error(path_ + ": expected a v4l2loopback device");
        if (!(flags & V4L2_CAP_VIDEO_OUTPUT) || !(flags & V4L2_CAP_READWRITE))
            throw std::runtime_error(path_ + ": no writable V4L2 output (another producer may be running)");

        v4l2_format fmt{};
        fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        auto& p = fmt.fmt.pix;
        p.width = width; p.height = height; p.pixelformat = fourcc;
        p.field = V4L2_FIELD_NONE;
        p.bytesperline = static_cast<std::uint32_t>(row_bytes_);
        p.sizeimage = static_cast<std::uint32_t>(row_bytes_ * height);
        p.colorspace = fourcc == V4L2_PIX_FMT_YUYV ? V4L2_COLORSPACE_SMPTE170M : V4L2_COLORSPACE_RAW;
        p.ycbcr_enc = V4L2_YCBCR_ENC_601;
        p.quantization = fourcc == V4L2_PIX_FMT_YUYV ? V4L2_QUANTIZATION_LIM_RANGE : V4L2_QUANTIZATION_FULL_RANGE;
        if (xioctl(fd_, VIDIOC_S_FMT, &fmt) < 0) throw error(path_, "VIDIOC_S_FMT");
        if (p.width != width || p.height != height || p.pixelformat != fourcc)
            throw std::runtime_error(path_ + ": driver changed requested dimensions or pixel format");
        stride_ = p.bytesperline;
        if (stride_ < row_bytes_ || !height_ || p.sizeimage < stride_ * height_ || p.sizeimage > 64u*1024u*1024u)
            throw std::runtime_error(path_ + ": invalid negotiated stride/buffer size");
        buffer_.resize(p.sizeimage, 0);
        v4l2_streamparm param{};
        param.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        param.parm.output.timeperframe.numerator = 1;
        param.parm.output.timeperframe.denominator = 30;
        if (xioctl(fd_, VIDIOC_S_PARM, &param) < 0) throw error(path_, "set 30 fps");
        control("timeout", 1000, true);
    } catch (...) { close(); throw; }
}
void V4L2Output::control(const char* name, int value, bool required) {
    v4l2_queryctrl query{};
    query.id = V4L2_CTRL_FLAG_NEXT_CTRL;
    while (xioctl(fd_, VIDIOC_QUERYCTRL, &query) == 0) {
        if (!(query.flags & V4L2_CTRL_FLAG_DISABLED) &&
            std::strncmp(reinterpret_cast<char*>(query.name), name, sizeof(query.name)) == 0) {
            v4l2_control ctrl{};
            ctrl.id = query.id; ctrl.value = value;
            if (xioctl(fd_, VIDIOC_S_CTRL, &ctrl) < 0) throw error(path_, std::string("set ") + name);
            return;
        }
        query.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }
    if (required) throw std::runtime_error(path_ + ": loopback lacks required stale-frame timeout control; update v4l2loopback");
}
void V4L2Output::write_frame(const std::vector<std::uint8_t>& pixels) {
    if (pixels.size() != row_bytes_ * height_)
        throw std::invalid_argument(path_ + ": incorrect frame payload size");
    for (std::size_t y = 0; y < height_; ++y)
        std::memcpy(buffer_.data() + y*stride_, pixels.data() + y*row_bytes_, row_bytes_);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    while (std::chrono::steady_clock::now() < deadline) {
        const ssize_t written = ::write(fd_, buffer_.data(), buffer_.size());
        if (written == static_cast<ssize_t>(buffer_.size())) return;
        if (written >= 0) throw std::runtime_error(path_ + ": short V4L2 frame write");
        if (errno == EINTR) continue;
        if (errno != EAGAIN && errno != EWOULDBLOCK) throw error(path_, "write");
        pollfd pfd{fd_, POLLOUT, 0};
        const int rc = ::poll(&pfd, 1, 50);
        if (rc < 0 && errno != EINTR) throw error(path_, "poll");
        if (rc > 0 && (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)))
            throw std::runtime_error(path_ + ": V4L2 output disconnected");
    }
    throw std::runtime_error(path_ + ": timed out writing V4L2 frame");
}
void V4L2Output::close() noexcept {
    if (fd_ < 0) return;
    ::close(fd_); fd_ = -1;
}
V4L2Output::~V4L2Output() { close(); }
void V4L2Output::write_blank() {
    std::vector<std::uint8_t> black(row_bytes_*height_,0);
    if (fourcc_ == V4L2_PIX_FMT_YUYV) {
        for (std::size_t i=0; i<black.size(); i+=2) { black[i]=16; black[i+1]=128; }
    }
    // Replace every queued image, including for clients which reopen before
    // the kernel timeout expires. No previous face survives the idle transition.
    write_frame(black); // Allocate the producer's file-I/O queue before querying it.
    std::uint32_t count=0;
    for (;count<64;++count) {
        v4l2_buffer buffer{};
        buffer.type=V4L2_BUF_TYPE_VIDEO_OUTPUT;
        buffer.memory=V4L2_MEMORY_MMAP;
        buffer.index=count;
        if (xioctl(fd_,VIDIOC_QUERYBUF,&buffer)<0) {
            if (errno==EINVAL && count>0) break;
            throw error(path_,"query actual output queue for blanking");
        }
    }
    if (count==64) throw std::runtime_error(path_+": unsupported output queue size");
    for (std::uint32_t i=1;i<count;++i) write_frame(black);
}
void V4L2Output::enable_client_monitoring() {
    v4l2_event_subscription sub{};
    sub.type = client_usage_event;
    sub.flags = V4L2_EVENT_SUB_FL_SEND_INITIAL;
    if (xioctl(fd_,VIDIOC_SUBSCRIBE_EVENT,&sub) < 0)
        throw error(path_,"on-demand needs v4l2loopback client-usage events (update the module)");
    monitoring_ = true;
}
bool V4L2Output::has_capture_clients() {
    if (!monitoring_) throw std::logic_error(path_ + ": client monitoring is not enabled");
    v4l2_event event{};
    while (xioctl(fd_,VIDIOC_DQEVENT,&event) == 0) {
        if (event.type == client_usage_event) {
            std::uint32_t count=0;
            std::memcpy(&count,event.u.data,sizeof(count));
            capture_clients_ = count != 0;
        }
    }
    // The V4L2 core returns ENOENT for an empty nonblocking event queue.
    if (errno != ENOENT && errno != EAGAIN) throw error(path_,"dequeue client-usage event");
    return capture_clients_;
}
}
