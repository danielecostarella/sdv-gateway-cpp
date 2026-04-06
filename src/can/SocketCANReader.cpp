#include "SocketCANReader.hpp"

#include <spdlog/spdlog.h>

#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace sdvgw::can {

SocketCANReader::~SocketCANReader()
{
    close();
}

bool SocketCANReader::open(const char* interface_name) noexcept
{
    // Self-pipe used by stop() to wake poll() without a signal or timeout
    if (::pipe(stop_pipe_) < 0) {
        spdlog::error("SocketCANReader: pipe() failed: {}", std::strerror(errno));
        return false;
    }

    sock_fd_ = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (sock_fd_ < 0) {
        spdlog::error("SocketCANReader: socket() failed: {}", std::strerror(errno));
        return false;
    }

    struct ifreq ifr{};
    std::strncpy(ifr.ifr_name, interface_name, IFNAMSIZ - 1);
    if (::ioctl(sock_fd_, SIOCGIFINDEX, &ifr) < 0) {
        spdlog::error("SocketCANReader: interface '{}' not found: {}",
                      interface_name, std::strerror(errno));
        ::close(sock_fd_);
        sock_fd_ = -1;
        return false;
    }

    struct sockaddr_can addr{};
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (::bind(sock_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        spdlog::error("SocketCANReader: bind() failed: {}", std::strerror(errno));
        ::close(sock_fd_);
        sock_fd_ = -1;
        return false;
    }

    spdlog::info("SocketCANReader: opened '{}'", interface_name);
    return true;
}

void SocketCANReader::close() noexcept
{
    stop();

    if (sock_fd_ >= 0) {
        ::close(sock_fd_);
        sock_fd_ = -1;
    }
    if (stop_pipe_[0] >= 0) {
        ::close(stop_pipe_[0]);
        ::close(stop_pipe_[1]);
        stop_pipe_[0] = stop_pipe_[1] = -1;
    }
}

void SocketCANReader::run(FrameCallback callback)
{
    if (sock_fd_ < 0) {
        spdlog::error("SocketCANReader: run() called before open()");
        return;
    }

    running_.store(true, std::memory_order_relaxed);
    spdlog::debug("SocketCANReader: read loop started");

    struct pollfd fds[2]{};
    fds[0].fd     = sock_fd_;
    fds[0].events = POLLIN;
    fds[1].fd     = stop_pipe_[0];
    fds[1].events = POLLIN;

    while (running_.load(std::memory_order_relaxed)) {
        const int ret = ::poll(fds, 2, /*timeout_ms=*/-1);
        if (ret < 0) {
            if (errno == EINTR) continue;
            spdlog::error("SocketCANReader: poll() failed: {}", std::strerror(errno));
            break;
        }

        if (fds[1].revents & POLLIN) break;   // stop() was called
        if (!(fds[0].revents & POLLIN)) continue;

        struct can_frame raw{};
        const ssize_t nbytes = ::read(sock_fd_, &raw, sizeof(raw));
        if (nbytes < 0) {
            if (errno == EINTR) continue;
            spdlog::error("SocketCANReader: read() failed: {}", std::strerror(errno));
            break;
        }
        if (static_cast<size_t>(nbytes) < sizeof(raw)) continue;

        // Map kernel struct to CanFrame — layout is compatible
        CanFrame frame{};
        frame.id  = raw.can_id & CAN_EFF_MASK;  // strip flags, keep identifier
        frame.dlc = raw.can_dlc;
        std::memcpy(frame.data, raw.data, raw.can_dlc);

        callback(frame);  // must be non-blocking — see ICANReader.hpp
    }

    running_.store(false, std::memory_order_relaxed);
    spdlog::debug("SocketCANReader: read loop stopped");
}

void SocketCANReader::stop() noexcept
{
    const bool was_running = running_.exchange(false, std::memory_order_relaxed);
    if (was_running && stop_pipe_[1] >= 0) {
        const char byte = 1;
        (void)::write(stop_pipe_[1], &byte, sizeof(byte));
    }
}

} // namespace sdvgw::can
