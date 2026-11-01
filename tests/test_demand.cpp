#include "demand.hpp"
#include <chrono>
#include <stdexcept>
#include <iostream>

void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main() {
    using namespace std::chrono;
    using kinectcam::DemandState;
    const auto t = steady_clock::time_point{};
    DemandState demand(milliseconds(1000));
    require(!demand.wants_capture(false,t),"idle startup must never activate the Kinect");
    require(demand.wants_capture(true,t+seconds(10)),"first consumer must activate capture");
    require(demand.wants_capture(false,t+seconds(10)+milliseconds(999)),"short gaps must retain session");
    require(!demand.wants_capture(false,t+seconds(11)),"grace expiry must stop capture");
    require(demand.wants_capture(true,t+seconds(12)),"later consumer must wake again");
    require(demand.wants_capture(true,t+seconds(20)),"continuing consumers extend activity");
    require(demand.wants_capture(false,t+seconds(20)+milliseconds(900)),"last use defines idle deadline");
    require(demand.wants_capture(true,t+seconds(20)+milliseconds(950)),"consumer during grace cancels shutdown");
    require(demand.wants_capture(false,t+seconds(21)),"renewed session must retain full grace");
    require(!demand.wants_capture(false,t+seconds(22)),"all consumers leaving eventually stops capture");
    DemandState immediate(milliseconds(0));
    require(immediate.wants_capture(true,t),"zero grace still permits active capture");
    require(!immediate.wants_capture(false,t),"zero grace must stop immediately");
    std::cout << "demand state fixtures passed\n";
}
