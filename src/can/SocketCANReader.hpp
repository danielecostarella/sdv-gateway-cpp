#pragma once

#include "ICANReader.hpp"

#include <atomic>

namespace sdvgw::can {

/// Linux SocketCAN implementation of ICANReader.
///
/// Opens a raw CAN socket bound to the named interface (e.g., "vcan0",
/// "can0") and delivers frames to the registered callback.
///
/// Shutdown is immediate: stop() writes to a self-pipe that wakes the
/// poll() call inside run(), so the read loop exits without waiting for
/// the next frame.
class SocketCANReader final : public ICANReader {
public:
    SocketCANReader() = default;
    ~SocketCANReader() override;

    // Non-copyable, non-movable (owns file descriptors)
    SocketCANReader(const SocketCANReader&)            = delete;
    SocketCANReader& operator=(const SocketCANReader&) = delete;

    bool open(const char* interface_name) noexcept override;
    void close() noexcept override;

    /// Blocking read loop. Call from a dedicated thread.
    /// Returns when stop() is called or a fatal socket error occurs.
    void run(FrameCallback callback) override;

    /// Signal run() to exit. Thread-safe, wait-free.
    void stop() noexcept override;

private:
    int sock_fd_{-1};
    int stop_pipe_[2]{-1, -1};  ///< Self-pipe: write end wakes poll() in run()
    std::atomic<bool> running_{false};
};

} // namespace sdvgw::can
