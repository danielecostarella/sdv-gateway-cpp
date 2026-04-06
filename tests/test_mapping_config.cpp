#include <gtest/gtest.h>
#include <decoder/MappingConfig.hpp>

using namespace sdvgw::decoder;

// ---------------------------------------------------------------------------
// Fixture: the same schema as config/mapping.json
// ---------------------------------------------------------------------------
static constexpr const char* kValidJson = R"json(
{
  "version": "1.0",
  "mappings": [
    {
      "can_id": 256,
      "signals": [
        {
          "vss_path": "Vehicle.Speed",
          "type": "float",
          "start_byte": 0,
          "num_bytes": 2,
          "factor": 0.1,
          "offset": 0
        },
        {
          "vss_path": "Vehicle.Powertrain.CombustionEngine.Speed",
          "type": "uint16",
          "start_byte": 2,
          "num_bytes": 2,
          "factor": 1.0,
          "offset": 0
        }
      ]
    },
    {
      "can_id": 257,
      "signals": [
        {
          "vss_path": "Vehicle.Body.Lights.IsHighBeamOn",
          "type": "bool",
          "start_byte": 0,
          "num_bytes": 1,
          "factor": 1.0,
          "offset": 0
        }
      ]
    }
  ]
}
)json";

// ---------------------------------------------------------------------------
// Load / parse
// ---------------------------------------------------------------------------

TEST(MappingConfig, LoadsValidJson)
{
    const auto cfg = MappingConfig::from_string(kValidJson);
    ASSERT_TRUE(cfg.has_value());
    EXPECT_EQ(cfg->frame_count(), 2u);
}

TEST(MappingConfig, ReturnsNulloptOnEmptyString)
{
    EXPECT_FALSE(MappingConfig::from_string("").has_value());
}

TEST(MappingConfig, ReturnsNulloptOnInvalidJson)
{
    EXPECT_FALSE(MappingConfig::from_string("{ not json }").has_value());
}

TEST(MappingConfig, ReturnsNulloptWhenMappingsKeyMissing)
{
    EXPECT_FALSE(MappingConfig::from_string(R"({"version":"1.0"})").has_value());
}

TEST(MappingConfig, RejectsUnknownSignalType)
{
    const std::string bad = R"json({
      "mappings": [{
        "can_id": 1,
        "signals": [{"vss_path":"X","type":"int64","start_byte":0,"num_bytes":1}]
      }]
    })json";
    EXPECT_FALSE(MappingConfig::from_string(bad).has_value());
}

TEST(MappingConfig, RejectsZeroNumBytes)
{
    const std::string bad = R"json({
      "mappings": [{
        "can_id": 1,
        "signals": [{"vss_path":"X","type":"float","start_byte":0,"num_bytes":0}]
      }]
    })json";
    EXPECT_FALSE(MappingConfig::from_string(bad).has_value());
}

TEST(MappingConfig, RejectsSignalExceedingFrameBoundary)
{
    // start_byte=7, num_bytes=2 → bytes 7-8, but CAN frame is only 0-7
    const std::string bad = R"json({
      "mappings": [{
        "can_id": 1,
        "signals": [{"vss_path":"X","type":"float","start_byte":7,"num_bytes":2}]
      }]
    })json";
    EXPECT_FALSE(MappingConfig::from_string(bad).has_value());
}

// ---------------------------------------------------------------------------
// Lookup
// ---------------------------------------------------------------------------

TEST(MappingConfig, FindsKnownCanId)
{
    const auto cfg = MappingConfig::from_string(kValidJson);
    ASSERT_TRUE(cfg.has_value());

    const FrameMapping* fm = cfg->find(256);
    ASSERT_NE(fm, nullptr);
    EXPECT_EQ(fm->can_id, 256u);
    EXPECT_EQ(fm->signals.size(), 2u);
}

TEST(MappingConfig, ReturnsNullptrForUnknownCanId)
{
    const auto cfg = MappingConfig::from_string(kValidJson);
    ASSERT_TRUE(cfg.has_value());
    EXPECT_EQ(cfg->find(0xDEAD), nullptr);
}

TEST(MappingConfig, SignalFieldsAreCorrect)
{
    const auto cfg = MappingConfig::from_string(kValidJson);
    ASSERT_TRUE(cfg.has_value());

    const FrameMapping* fm = cfg->find(256);
    ASSERT_NE(fm, nullptr);

    const SignalMapping& speed = fm->signals[0];
    EXPECT_EQ(speed.vss_path,   "Vehicle.Speed");
    EXPECT_EQ(speed.type,       SignalType::Float);
    EXPECT_EQ(speed.start_byte, 0);
    EXPECT_EQ(speed.num_bytes,  2);
    EXPECT_DOUBLE_EQ(speed.factor, 0.1);
    EXPECT_DOUBLE_EQ(speed.offset, 0.0);
}

TEST(MappingConfig, DefaultFactorAndOffsetAreOneAndZero)
{
    // factor and offset are optional — defaults: factor=1.0, offset=0.0
    const std::string json = R"json({
      "mappings": [{
        "can_id": 1,
        "signals": [{"vss_path":"X","type":"uint16","start_byte":0,"num_bytes":2}]
      }]
    })json";
    const auto cfg = MappingConfig::from_string(json);
    ASSERT_TRUE(cfg.has_value());

    const FrameMapping* fm = cfg->find(1);
    ASSERT_NE(fm, nullptr);
    EXPECT_DOUBLE_EQ(fm->signals[0].factor, 1.0);
    EXPECT_DOUBLE_EQ(fm->signals[0].offset, 0.0);
}
