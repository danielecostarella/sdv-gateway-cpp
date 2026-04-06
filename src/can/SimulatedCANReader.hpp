#pragma once

#include "ICANReader.hpp"

#include <atomic>
#include <chrono>
#include <vector>

namespace sdvgw::can {

/// In-process CAN frame source for unit tests.
///
/// Frames are pre-loaded with enqueue() and replayed synchronously when
/// run() is called. No kernel interface, no hardware required.
/// An optional inter-frame interval allows timing-sensitive tests.
class SimulatedCANReader final : public ICANReader {
public:
    /// Add a frame to the replay queue.
    void enqueue(CanFrame frame);

    /// Delay between consecutive frames delivered in run().
    /// Default: 0 ms — all frames delivered without sleeping.
    void set_frame_interval(std::chrono::milliseconds interval);

    bool open(const char* /*interface_name*/) noexcept override { return true; }
    void close() noexcept override {}

    /// Replays all enqueued frames in order, then returns.
    /// Respects stop() calls between frames.
    void run(FrameCallback callback) override;

    void stop() noexcept override;

private:
    std::vector<CanFrame> frames_;
    std::atomic<bool> running_{false};
    std::chrono::milliseconds interval_{0};
};

} // namespace sdvgw::can
