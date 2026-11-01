// The kernel device is unavailable in CI. Substitute only its syscall boundary;
// exercise the real writer, its padding, negotiation, errors and cleanup.
#include "v4l2_output.hpp"
#include <linux/videodev2.h>
#include <sys/stat.h>
#include <poll.h>
#include <cstdarg>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
struct Kernel {
    bool substitute = false, undersize = false, short_write = false;
    bool busy = false, interrupt = false, closed = false, writable = true;
    bool timeout_control = true;
    bool timeout_image_armed = false;
    bool persistent_format_lock = false;
    int timeout_ms = 0;
    std::vector<unsigned char> bytes;
} kernel;
void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
template<class F> void rejects(F f, const char* message) {
    bool failed = false;
    try { f(); } catch (const std::exception&) { failed = true; }
    require(failed, message);
}
}
extern "C" {
int __wrap_open(const char*, int, ...) { return 37; }
int __wrap_close(int) { kernel.closed = true; return 0; }
int __wrap_fstat(int, struct stat* s) {
    std::memset(s, 0, sizeof(*s)); s->st_mode = S_IFCHR; s->st_rdev = 123; return 0;
}
int __wrap_ioctl(int, unsigned long request, ...) {
    va_list args; va_start(args, request); void* ptr = va_arg(args, void*); va_end(args);
    if (request == VIDIOC_QUERYCAP) {
        auto* c = static_cast<v4l2_capability*>(ptr);
        std::strcpy(reinterpret_cast<char*>(c->driver), "v4l2 loopback");
        c->capabilities = V4L2_CAP_DEVICE_CAPS;
        c->device_caps = V4L2_CAP_READWRITE | (kernel.writable ? V4L2_CAP_VIDEO_OUTPUT : V4L2_CAP_VIDEO_CAPTURE);
    } else if (request == VIDIOC_S_FMT) {
        auto* f = static_cast<v4l2_format*>(ptr);
        f->fmt.pix.bytesperline = 8;
        f->fmt.pix.sizeimage = kernel.undersize ? 3 : 20;
        if (kernel.substitute) f->fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
    } else if (request == VIDIOC_QUERYCTRL) {
        auto* c = static_cast<v4l2_queryctrl*>(ptr);
        const auto id = c->id & ~V4L2_CTRL_FLAG_NEXT_CTRL;
        if (id < V4L2_CID_PRIVATE_BASE) {
            c->id = V4L2_CID_PRIVATE_BASE;
            std::strcpy(reinterpret_cast<char*>(c->name), "timeout_image_io");
        } else if (id == V4L2_CID_PRIVATE_BASE && kernel.timeout_control) {
            c->id = V4L2_CID_PRIVATE_BASE+1;
            std::strcpy(reinterpret_cast<char*>(c->name), "timeout");
        } else if (id == V4L2_CID_PRIVATE_BASE+1) {
            c->id = V4L2_CID_PRIVATE_BASE+2;
            std::strcpy(reinterpret_cast<char*>(c->name), "keep_format");
        } else { errno = EINVAL; return -1; }
    } else if (request == VIDIOC_S_CTRL) {
        auto* c = static_cast<v4l2_control*>(ptr);
        if (c->id == V4L2_CID_PRIVATE_BASE) kernel.timeout_image_armed = true;
        else if (c->id == V4L2_CID_PRIVATE_BASE+2) kernel.persistent_format_lock = c->value;
        else kernel.timeout_ms = c->value;
    } else if (request != VIDIOC_S_PARM) {
        errno = EINVAL; return -1;
    }
    return 0;
}
ssize_t __wrap_write(int, const void* data, size_t size) {
    if (kernel.interrupt) { kernel.interrupt = false; errno = EINTR; return -1; }
    if (kernel.busy) { kernel.busy = false; errno = EAGAIN; return -1; }
    kernel.bytes.assign(static_cast<const unsigned char*>(data), static_cast<const unsigned char*>(data)+size);
    return kernel.short_write ? static_cast<ssize_t>(size-1) : static_cast<ssize_t>(size);
}
int __wrap_poll(struct pollfd* fds, nfds_t, int) { fds[0].revents = POLLOUT; return 1; }
}
int main() {
    using kinectcam::V4L2Output;
    {
        V4L2Output out("/fake/video", 2, 2, V4L2_PIX_FMT_YUYV, 2);
        kernel.busy = true; kernel.interrupt = true;
        out.write_frame({1,2,3,4,5,6,7,8});
        require(kernel.bytes == std::vector<unsigned char>({1,2,3,4,0,0,0,0,5,6,7,8,0,0,0,0,0,0,0,0}),
                "writer must honor negotiated row stride and zero trailing padding");
        require(kernel.timeout_ms == 1000, "stale output must blank after 1000ms");
        require(!kernel.timeout_image_armed, "timeout_image_io is a trigger even when set to zero; never arm it");
        require(!kernel.persistent_format_lock, "a killed producer must not leave keep_format hiding OUTPUT on restart");
        rejects([&] { out.write_frame({1,2}); }, "reject incorrect payload length");
        kernel.short_write = true;
        rejects([&] { out.write_frame({1,2,3,4,5,6,7,8}); }, "short frames must fail, never split");
    }
    require(kernel.closed, "writer must close its descriptor");
    kernel = {}; kernel.substitute = true;
    rejects([] { V4L2Output out("/fake/video",2,2,V4L2_PIX_FMT_YUYV,2); }, "reject substituted formats");
    require(kernel.closed, "constructor failures must close descriptor");
    kernel = {}; kernel.undersize = true;
    rejects([] { V4L2Output out("/fake/video",2,2,V4L2_PIX_FMT_YUYV,2); }, "reject undersized driver buffers");
    kernel = {}; kernel.writable = false;
    rejects([] { V4L2Output out("/fake/video",2,2,V4L2_PIX_FMT_YUYV,2); }, "reject capture-only/busy devices");
    kernel = {}; kernel.timeout_control = false;
    rejects([] { V4L2Output out("/fake/video",2,2,V4L2_PIX_FMT_YUYV,2); }, "require stale-frame timeout support");
    std::cout << "V4L2 boundary fixtures passed\n";
}
