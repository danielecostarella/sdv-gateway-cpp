#include "MappingConfig.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <fstream>
#include <stdexcept>

namespace sdvgw::decoder {

namespace {

SignalType parse_type(const std::string& s)
{
    if (s == "float")                return SignalType::Float;
    if (s == "uint16" || s == "uint32") return SignalType::Uint32;
    if (s == "bool")                 return SignalType::Bool;
    throw std::invalid_argument("unknown signal type '" + s +
                                "' (expected: float, uint16, uint32, bool)");
}

MappingConfig parse(const nlohmann::json& root)
{
    MappingConfig cfg;

    if (!root.contains("mappings") || !root.at("mappings").is_array()) {
        throw std::invalid_argument("top-level 'mappings' array is missing or not an array");
    }

    for (const auto& entry : root.at("mappings")) {
        FrameMapping fm;
        fm.can_id = entry.at("can_id").get<uint32_t>();

        for (const auto& sig : entry.at("signals")) {
            SignalMapping sm;
            sm.vss_path   = sig.at("vss_path").get<std::string>();
            sm.type       = parse_type(sig.at("type").get<std::string>());
            sm.start_byte = sig.at("start_byte").get<uint8_t>();
            sm.num_bytes  = sig.at("num_bytes").get<uint8_t>();
            sm.factor     = sig.value("factor", 1.0);
            sm.offset     = sig.value("offset", 0.0);

            if (sm.num_bytes == 0 || sm.num_bytes > 4) {
                throw std::invalid_argument(
                    "signal '" + sm.vss_path + "': num_bytes must be 1–4");
            }
            if (static_cast<unsigned>(sm.start_byte) + sm.num_bytes > 8) {
                throw std::invalid_argument(
                    "signal '" + sm.vss_path + "': extends beyond CAN frame boundary (8 bytes)");
            }

            fm.signals.push_back(std::move(sm));
        }

        cfg.index_[fm.can_id] = static_cast<uint32_t>(cfg.mappings_.size());
        cfg.mappings_.push_back(std::move(fm));
    }

    spdlog::info("MappingConfig: loaded {} frame mapping(s)", cfg.mappings_.size());
    return cfg;
}

} // namespace

std::optional<MappingConfig> MappingConfig::from_file(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        spdlog::error("MappingConfig: cannot open '{}'", path);
        return std::nullopt;
    }

    try {
        return parse(nlohmann::json::parse(file, /*cb=*/nullptr, /*exceptions=*/true));
    } catch (const std::exception& ex) {
        spdlog::error("MappingConfig: error in '{}': {}", path, ex.what());
        return std::nullopt;
    }
}

std::optional<MappingConfig> MappingConfig::from_string(const std::string& json_str)
{
    try {
        return parse(nlohmann::json::parse(json_str, nullptr, true));
    } catch (const std::exception& ex) {
        spdlog::error("MappingConfig: parse error: {}", ex.what());
        return std::nullopt;
    }
}

const FrameMapping* MappingConfig::find(uint32_t can_id) const noexcept
{
    const auto it = index_.find(can_id);
    if (it == index_.end()) return nullptr;
    return &mappings_[it->second];
}

} // namespace sdvgw::decoder
