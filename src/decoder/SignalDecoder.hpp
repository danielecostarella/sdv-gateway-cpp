#pragma once

#include "MappingConfig.hpp"
#include "VssSignal.hpp"

#include <span>
#include <vector>

namespace sdvgw::can { struct CanFrame; }

namespace sdvgw::decoder {

/// Stateless CAN→VSS decoder.
///
/// decode() maps a raw CanFrame to zero or more VssSignal values using the
/// loaded MappingConfig. Results are written into a pre-allocated internal
/// buffer and returned as a std::span — no heap allocation on the hot path.
///
/// The returned span is valid only until the next call to decode().
class SignalDecoder {
public:
    explicit SignalDecoder(const MappingConfig& config);

    /// Decode a single CAN frame.
    /// Returns an empty span if the frame's CAN ID has no mapping.
    std::span<const VssSignal> decode(const can::CanFrame& frame);

private:
    const MappingConfig& config_;
    std::vector<VssSignal> output_;  ///< Pre-allocated, reused across calls
};

} // namespace sdvgw::decoder
