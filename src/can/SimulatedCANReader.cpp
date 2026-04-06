#include "SimulatedCANReader.hpp"

#include <spdlog/spdlog.h>

#include <thread>

namespace sdvgw::can {

void SimulatedCANReader::enqueue(CanFrame frame)
{
    frames_.push_back(frame);
}

void SimulatedCANReader::set_frame_interval(std::chrono::milliseconds interval)
{
    interval_ = interval;
}

void SimulatedCANReader::run(FrameCallback callback)
{
    running_.store(true, std::memory_order_relaxed);
    spdlog::debug("SimulatedCANReader: replaying {} frame(s)", frames_.size());

    for (const auto& frame : frames_) {
        if (!running_.load(std::memory_order_relaxed)) break;

        callback(frame);

        if (interval_.count() > 0) {
            std::this_thread::sleep_for(interval_);
        }
    }

    running_.store(false, std::memory_order_relaxed);
    spdlog::debug("SimulatedCANReader: replay complete");
}

void SimulatedCANReader::stop() noexcept
{
    running_.store(false, std::memory_order_relaxed);
}

} // namespace sdvgw::can
