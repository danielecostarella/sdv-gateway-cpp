#include "SignalDecoder.hpp"

#include <can/ICANReader.hpp>
#include <spdlog/spdlog.h>

#include <chrono>
#include <cstring>

namespace sdvgw::decoder {

namespace {

/// Extract a little-endian (Intel byte order) unsigned integer from CAN data.
/// Covers 1, 2, 3, and 4-byte signals — sufficient for all standard VSS numerics.
uint32_t extract_raw(const uint8_t* data, uint8_t start_byte, uint8_t num_bytes)
{
    uint32_t raw = 0;
    for (uint8_t i = 0; i < num_bytes; ++i) {
        raw |= static_cast<uint32_t>(data[start_byte + i]) << (8u * i);
    }
    return raw;
}

VssSignal::Value apply(uint32_t raw, const SignalMapping& m)
{
    switch (m.type) {
    case SignalType::Float:
        return static_cast<float>(raw * m.factor + m.offset);
    case SignalType::Uint32:
        return static_cast<uint32_t>(raw * m.factor + m.offset);
    case SignalType::Bool:
        return raw != 0u;
    }
    return false;  // unreachable — all enum values handled above
}

} // namespace

SignalDecoder::SignalDecoder(const MappingConfig& config)
    : config_(config)
{
    output_.reserve(8);  // typical maximum signals per CAN frame
}

std::span<const VssSignal> SignalDecoder::decode(const can::CanFrame& frame)
{
    output_.clear();

    const FrameMapping* fm = config_.find(frame.id);
    if (!fm) return {};  // unmapped frame — silently ignored

    const auto now = std::chrono::steady_clock::now();

    for (const auto& sm : fm->signals) {
        if (static_cast<unsigned>(sm.start_byte) + sm.num_bytes > frame.dlc) {
            spdlog::warn("SignalDecoder: frame 0x{:X} — signal '{}' exceeds dlc={}",
                         frame.id, sm.vss_path, frame.dlc);
            continue;
        }

        const uint32_t raw = extract_raw(frame.data, sm.start_byte, sm.num_bytes);

        output_.push_back({
            .vss_path  = sm.vss_path,
            .value     = apply(raw, sm),
            .timestamp = now,
        });
    }

    spdlog::trace("SignalDecoder: 0x{:X} → {} signal(s)", frame.id, output_.size());
    return std::span<const VssSignal>(output_);
}

} // namespace sdvgw::decoder
