#pragma once

#include <cstdint>
#include <functional>

namespace sdvgw::can {

/// Raw CAN frame as received from the kernel.
/// Layout mirrors the Linux SocketCAN struct can_frame.
struct CanFrame {
    uint32_t id;       ///< 11-bit (standard) or 29-bit (extended) CAN identifier
    uint8_t  dlc;      ///< Data Length Code — number of valid bytes in data[] (0-8)
    uint8_t  pad[3];   ///< Alignment padding — reserved, do not use
    uint8_t  data[8];  ///< Payload
};

/// Callback invoked on the reader thread for each received frame.
/// MUST be non-blocking and heap-allocation-free on the hot path.
using FrameCallback = std::function<void(const CanFrame&)>;

/// Abstract CAN reader interface.
///
/// Implementations:
///   - SocketCANReader   Linux SocketCAN (vcan0, can0 — bare metal and Docker)
///   - SimulatedCANReader In-process frame injection (unit tests only)
class ICANReader {
public:
    virtual ~ICANReader() = default;

    /// Open the named CAN network interface (e.g., "vcan0", "can0").
    /// Returns true on success, false if the interface does not exist
    /// or cannot be opened.
    virtual bool open(const char* interface_name) noexcept = 0;

    /// Close the interface and release all resources.
    virtual void close() noexcept = 0;

    /// Blocking read loop — must be called from a dedicated thread.
    /// Invokes callback for each received frame until stop() is called.
    virtual void run(FrameCallback callback) = 0;

    /// Signal the read loop to exit. Thread-safe.
    virtual void stop() noexcept = 0;
};

} // namespace sdvgw::can
