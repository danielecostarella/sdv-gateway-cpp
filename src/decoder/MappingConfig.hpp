#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace sdvgw::decoder {

enum class SignalType { Float, Uint32, Bool };

/// Decoded representation of one signal entry in mapping.json.
struct SignalMapping {
    std::string vss_path;
    SignalType  type;
    uint8_t     start_byte;  ///< Byte offset within the CAN frame payload
    uint8_t     num_bytes;   ///< Payload width in bytes (1–4)
    double      factor;      ///< Scaling factor applied after extraction
    double      offset;      ///< Additive offset applied after scaling
};

/// All signal mappings for a single CAN frame identifier.
struct FrameMapping {
    uint32_t                  can_id;
    std::vector<SignalMapping> signals;
};

/// Parsed and indexed representation of config/mapping.json.
///
/// Validated at load time — the gateway exits on any schema violation
/// rather than silently forwarding wrong values (see ADR-003).
class MappingConfig {
public:
    /// Load from a JSON file on disk. Returns nullopt and logs the error
    /// if the file cannot be opened or the schema is invalid.
    static std::optional<MappingConfig> from_file(const std::string& path);

    /// Load from a JSON string. Useful for unit tests — no filesystem access.
    static std::optional<MappingConfig> from_string(const std::string& json);

    /// Look up the mapping for a given CAN ID. Returns nullptr if not found.
    /// O(1) via internal hash index.
    const FrameMapping* find(uint32_t can_id) const noexcept;

    std::size_t frame_count() const noexcept { return mappings_.size(); }

private:
    std::vector<FrameMapping>              mappings_;
    std::unordered_map<uint32_t, uint32_t> index_;  ///< can_id → index in mappings_
};

} // namespace sdvgw::decoder
