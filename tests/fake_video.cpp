// Linked only into kinectcam_test_io. No fake device support in the shipped
// executable. Store real write() payloads in temporary files for inspection.
#include <linux/videodev2.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <set>
#include <string>

namespace { std::set<int> video_fds; }
extern "C" {
int __real_open(const char*, int, ...);
int __real_close(int);
int __real_fstat(int, struct stat*);
int __real_ioctl(int, unsigned long, ...);
int __wrap_open(const char* path, int flags, ...) {
    if (std::strncmp(path, "/fake/", 6) == 0) {
        const char* directory = std::getenv("KINECTCAM_TEST_DIR");
        if (!directory) { errno = ENOENT; return -1; }
        const std::string file = std::string(directory) + "/" + (path+6);
        const int fd = __real_open(file.c_str(), O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC, 0600);
        if (fd >= 0) video_fds.insert(fd);
        return fd;
    }
    if (flags & O_CREAT) {
        va_list ap; va_start(ap,flags); const auto mode = va_arg(ap,int); va_end(ap);
        return __real_open(path,flags,mode);
    }
    return __real_open(path,flags);
}
int __wrap_fstat(int fd, struct stat* s) {
    const int rc = __real_fstat(fd,s);
    if (rc == 0 && video_fds.count(fd)) { s->st_mode = S_IFCHR; s->st_rdev = static_cast<dev_t>(fd); }
    return rc;
}
int __wrap_close(int fd) { video_fds.erase(fd); return __real_close(fd); }
int __wrap_ioctl(int fd, unsigned long request, ...) {
    va_list ap; va_start(ap,request); void* p = va_arg(ap,void*); va_end(ap);
    if (!video_fds.count(fd)) return __real_ioctl(fd,request,p);
    if (request == VIDIOC_QUERYCAP) {
        auto* c = static_cast<v4l2_capability*>(p);
        std::strcpy(reinterpret_cast<char*>(c->driver),"v4l2 loopback");
        c->capabilities = V4L2_CAP_VIDEO_OUTPUT|V4L2_CAP_READWRITE;
    } else if (request == VIDIOC_QUERYCTRL) {
        auto* q = static_cast<v4l2_queryctrl*>(p);
        if ((q->id & ~V4L2_CTRL_FLAG_NEXT_CTRL) >= V4L2_CID_PRIVATE_BASE) {
            errno = EINVAL; return -1;
        }
        q->id = V4L2_CID_PRIVATE_BASE;
        std::strcpy(reinterpret_cast<char*>(q->name),"timeout");
    } else if (request == VIDIOC_S_CTRL) {
        if (static_cast<v4l2_control*>(p)->value != 1000) { errno = EINVAL; return -1; }
    } else if (request != VIDIOC_S_FMT && request != VIDIOC_S_PARM) {
        errno = EINVAL; return -1;
    }
    return 0;
}
}
