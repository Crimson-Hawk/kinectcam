#pragma once
#include <chrono>

namespace kinectcam {
class DemandState {
public:
    explicit DemandState(std::chrono::milliseconds grace) : grace_(grace) {}
    bool wants_capture(bool consumers, std::chrono::steady_clock::time_point now) {
        if (consumers) { last_use_ = now; active_ = true; }
        else if (active_ && now-last_use_ >= grace_) active_ = false;
        return active_;
    }
private:
    std::chrono::milliseconds grace_;
    std::chrono::steady_clock::time_point last_use_{};
    bool active_ = false;
};
}
