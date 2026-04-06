#pragma once

#include <chrono>
#include <string>
#include <variant>

namespace sdvgw::decoder {

/// A decoded, VSS-normalized signal value ready to be forwarded to Kuksa.
///
/// The variant covers the three VSS primitive types used in this gateway:
///   float   — Vehicle.Speed (km/h)
///   uint32_t — Vehicle.Powertrain.CombustionEngine.Speed (rpm)
///   bool    — Vehicle.Body.Lights.IsHighBeamOn
///
/// Additional types (int32_t, double, std::string) can be added here and
/// in SignalDecoder without touching the transport or service layers.
struct VssSignal {
    using Value = std::variant<float, uint32_t, bool>;

    std::string                           vss_path;
    Value                                 value;
    std::chrono::steady_clock::time_point timestamp;
};

} // namespace sdvgw::decoder
