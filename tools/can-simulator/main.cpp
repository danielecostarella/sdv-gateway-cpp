// CAN frame simulator for sdv-gateway-cpp.
//
// Injects synthetic frames on a SocketCAN interface at a configurable
// interval, simulating three VSS signals:
//
//   CAN ID 0x100  bytes 0-1  Vehicle.Speed                      (km/h, factor 0.1)
//                 bytes 2-3  Vehicle.Powertrain.Engine.Speed     (rpm, factor 1.0)
//   CAN ID 0x101  byte  0    Vehicle.Body.Lights.IsHighBeamOn   (bool)
//
// The mapping matches config/mapping.json exactly.
//
// Usage:
//   can-simulator [--interface <iface>] [--interval <ms>]
//   can-simulator --interface vcan0 --interval 100

#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <numbers>
#include <string>
#include <thread>

namespace {

std::atomic<bool> g_running{true};

void on_signal(int) noexcept
{
    g_running.store(false, std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// Frame builders — encoding matches config/mapping.json (little-endian)
// ---------------------------------------------------------------------------

/// CAN ID 0x100: speed in bytes [0-1], rpm in bytes [2-3].
can_frame make_speed_frame(float speed_kmh, uint16_t rpm) noexcept
{
    can_frame f{};
    f.can_id  = 0x100;
    f.can_dlc = 8;

    // speed: raw = km/h / 0.1 → stored as uint16 little-endian
    const auto raw_speed = static_cast<uint16_t>(speed_kmh * 10.0f);
    std::memcpy(&f.data[0], &raw_speed, sizeof(raw_speed));
    std::memcpy(&f.data[2], &rpm,       sizeof(rpm));
    return f;
}

/// CAN ID 0x101: high-beam flag in byte [0].
can_frame make_lights_frame(bool high_beam) noexcept
{
    can_frame f{};
    f.can_id  = 0x101;
    f.can_dlc = 1;
    f.data[0] = high_beam ? 0x01 : 0x00;
    return f;
}

// ---------------------------------------------------------------------------
// Vehicle dynamics — simple sine-wave simulation
// ---------------------------------------------------------------------------

struct VehicleState {
    float    speed_kmh{0.0f};
    uint16_t rpm{800};
    bool     high_beam{false};
    double   phase{0.0};
};

/// Advance the vehicle state by dt_s seconds.
void step(VehicleState& v, double dt_s) noexcept
{
    // Speed: sine wave 20–120 km/h, full cycle ≈ 63 s
    v.phase     += dt_s * 0.1;
    v.speed_kmh  = 70.0f + 50.0f * static_cast<float>(std::sin(v.phase));

    // RPM: linear with speed — idle 800, ~4 000 at 120 km/h
    v.rpm = static_cast<uint16_t>(800.0f + v.speed_kmh * 26.67f);

    // High beam: toggles every quarter-cycle (~16 s)
    const int quarter = static_cast<int>(v.phase / (std::numbers::pi * 0.5));
    v.high_beam = (quarter % 2) == 0;
}

} // namespace

int main(int argc, char* argv[])
{
    std::string iface       = "vcan0";
    int         interval_ms = 100;

    for (int i = 1; i + 1 < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--interface") iface       = argv[i + 1];
        if (arg == "--interval")  interval_ms = std::stoi(argv[i + 1]);
    }

    struct sigaction sa{};
    sa.sa_handler = on_signal;
    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    // Open raw CAN socket
    const int sock = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (sock < 0) {
        std::cerr << "can-simulator: socket() failed: " << std::strerror(errno) << '\n';
        return 1;
    }

    struct ifreq ifr{};
    std::strncpy(ifr.ifr_name, iface.c_str(), IFNAMSIZ - 1);
    if (::ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
        std::cerr << "can-simulator: interface '" << iface
                  << "' not found: " << std::strerror(errno) << '\n';
        ::close(sock);
        return 1;
    }

    struct sockaddr_can addr{};
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "can-simulator: bind() failed: " << std::strerror(errno) << '\n';
        ::close(sock);
        return 1;
    }

    std::cout << "can-simulator: running on '" << iface
              << "' — interval " << interval_ms << " ms\n";

    VehicleState state;
    const double dt_s = interval_ms / 1000.0;

    while (g_running.load(std::memory_order_relaxed)) {
        step(state, dt_s);

        const auto f_speed  = make_speed_frame(state.speed_kmh, state.rpm);
        const auto f_lights = make_lights_frame(state.high_beam);

        (void)::write(sock, &f_speed,  sizeof(f_speed));
        (void)::write(sock, &f_lights, sizeof(f_lights));

        std::cout << "  speed=" << state.speed_kmh
                  << " km/h  rpm=" << state.rpm
                  << "  high_beam=" << (state.high_beam ? "ON" : "OFF") << '\n';

        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }

    ::close(sock);
    std::cout << "can-simulator: stopped\n";
    return 0;
}
