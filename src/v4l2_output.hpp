#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace kinectcam {
class V4L2Output {
public:
    V4L2Output(std::string path, std::uint32_t width, std::uint32_t height,
               std::uint32_t fourcc, std::uint32_t bytes_per_pixel);
    ~V4L2Output();
    V4L2Output(const V4L2Output&) = delete;
    V4L2Output& operator=(const V4L2Output&) = delete;
    void write_frame(const std::vector<std::uint8_t>& pixels);
    void write_blank();
    void enable_client_monitoring();
    bool has_capture_clients();
private:
    void close() noexcept;
    void control(const char* name, int value, bool required);
    std::string path_;
    int fd_ = -1;
    std::size_t row_bytes_ = 0, stride_ = 0, height_ = 0;
    std::uint32_t fourcc_ = 0;
    bool monitoring_ = false, capture_clients_ = false;
    std::vector<std::uint8_t> buffer_;
};
}
