#include <gtest/gtest.h>
#include <decoder/MappingConfig.hpp>
#include <decoder/SignalDecoder.hpp>
#include <can/ICANReader.hpp>

#include <cstring>
#include <variant>

using namespace sdvgw::decoder;
using namespace sdvgw::can;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static MappingConfig make_config()
{
    static constexpr const char* json = R"json({
      "mappings": [
        {
          "can_id": 256,
          "signals": [
            {"vss_path":"Vehicle.Speed",
             "type":"float","start_byte":0,"num_bytes":2,"factor":0.1,"offset":0},
            {"vss_path":"Vehicle.Powertrain.CombustionEngine.Speed",
             "type":"uint16","start_byte":2,"num_bytes":2,"factor":1.0,"offset":0}
          ]
        },
        {
          "can_id": 257,
          "signals": [
            {"vss_path":"Vehicle.Body.Lights.IsHighBeamOn",
             "type":"bool","start_byte":0,"num_bytes":1}
          ]
        }
      ]
    })json";
    auto cfg = MappingConfig::from_string(json);
    return std::move(*cfg);
}

/// Build a CanFrame with the given id, dlc, and raw payload bytes.
static CanFrame make_frame(uint32_t id, uint8_t dlc, const uint8_t* data)
{
    CanFrame f{};
    f.id  = id;
    f.dlc = dlc;
    std::memcpy(f.data, data, dlc);
    return f;
}

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class SignalDecoderTest : public ::testing::Test {
protected:
    MappingConfig  cfg_    = make_config();
    SignalDecoder  decoder_{cfg_};
};

// ---------------------------------------------------------------------------
// Vehicle.Speed (float, factor 0.1)
// ---------------------------------------------------------------------------

TEST_F(SignalDecoderTest, DecodesSpeed30kmh)
{
    // raw = 30.0 / 0.1 = 300 = 0x012C → [0x2C, 0x01] little-endian
    // RPM bytes are zero-filled
    const uint8_t data[8] = {0x2C, 0x01, 0x00, 0x00, 0, 0, 0, 0};
    const auto frame = make_frame(256, 8, data);

    const auto signals = decoder_.decode(frame);
    ASSERT_EQ(signals.size(), 2u);

    const auto& speed = signals[0];
    EXPECT_EQ(speed.vss_path, "Vehicle.Speed");
    ASSERT_TRUE(std::holds_alternative<float>(speed.value));
    EXPECT_NEAR(std::get<float>(speed.value), 30.0f, 0.01f);
}

TEST_F(SignalDecoderTest, DecodesSpeed0kmh)
{
    const uint8_t data[8] = {0x00, 0x00, 0x00, 0x00, 0, 0, 0, 0};
    const auto signals = decoder_.decode(make_frame(256, 8, data));
    ASSERT_GE(signals.size(), 1u);
    EXPECT_NEAR(std::get<float>(signals[0].value), 0.0f, 0.001f);
}

TEST_F(SignalDecoderTest, DecodesSpeedMaxUint16)
{
    // raw = 0xFFFF = 65535 → speed = 65535 * 0.1 = 6553.5 km/h
    const uint8_t data[8] = {0xFF, 0xFF, 0x00, 0x00, 0, 0, 0, 0};
    const auto signals = decoder_.decode(make_frame(256, 8, data));
    ASSERT_GE(signals.size(), 1u);
    EXPECT_NEAR(std::get<float>(signals[0].value), 6553.5f, 0.1f);
}

// ---------------------------------------------------------------------------
// Vehicle.Powertrain.CombustionEngine.Speed (uint32, factor 1.0)
// ---------------------------------------------------------------------------

TEST_F(SignalDecoderTest, DecodesRpm1500)
{
    // 1500 = 0x05DC → [0xDC, 0x05] at bytes 2-3
    const uint8_t data[8] = {0x00, 0x00, 0xDC, 0x05, 0, 0, 0, 0};
    const auto signals = decoder_.decode(make_frame(256, 8, data));
    ASSERT_EQ(signals.size(), 2u);

    const auto& rpm = signals[1];
    EXPECT_EQ(rpm.vss_path, "Vehicle.Powertrain.CombustionEngine.Speed");
    ASSERT_TRUE(std::holds_alternative<uint32_t>(rpm.value));
    EXPECT_EQ(std::get<uint32_t>(rpm.value), 1500u);
}

TEST_F(SignalDecoderTest, DecodesRpmZero)
{
    const uint8_t data[8] = {};
    const auto signals = decoder_.decode(make_frame(256, 8, data));
    ASSERT_EQ(signals.size(), 2u);
    EXPECT_EQ(std::get<uint32_t>(signals[1].value), 0u);
}

// ---------------------------------------------------------------------------
// Vehicle.Body.Lights.IsHighBeamOn (bool)
// ---------------------------------------------------------------------------

TEST_F(SignalDecoderTest, DecodesHighBeamOn)
{
    const uint8_t data[1] = {0x01};
    const auto signals = decoder_.decode(make_frame(257, 1, data));
    ASSERT_EQ(signals.size(), 1u);

    EXPECT_EQ(signals[0].vss_path, "Vehicle.Body.Lights.IsHighBeamOn");
    ASSERT_TRUE(std::holds_alternative<bool>(signals[0].value));
    EXPECT_TRUE(std::get<bool>(signals[0].value));
}

TEST_F(SignalDecoderTest, DecodesHighBeamOff)
{
    const uint8_t data[1] = {0x00};
    const auto signals = decoder_.decode(make_frame(257, 1, data));
    ASSERT_EQ(signals.size(), 1u);
    EXPECT_FALSE(std::get<bool>(signals[0].value));
}

TEST_F(SignalDecoderTest, AnyNonZeroByteIsTrueForBool)
{
    const uint8_t data[1] = {0xAB};
    const auto signals = decoder_.decode(make_frame(257, 1, data));
    ASSERT_EQ(signals.size(), 1u);
    EXPECT_TRUE(std::get<bool>(signals[0].value));
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

TEST_F(SignalDecoderTest, UnknownCanIdReturnsEmptySpan)
{
    const uint8_t data[8] = {};
    const auto signals = decoder_.decode(make_frame(0xDEAD, 8, data));
    EXPECT_TRUE(signals.empty());
}

TEST_F(SignalDecoderTest, FrameTooShortSkipsSignal)
{
    // dlc=1 — not enough bytes for the 2-byte speed signal at bytes 0-1
    const uint8_t data[1] = {0xFF};
    const auto signals = decoder_.decode(make_frame(256, 1, data));
    // Both signals require dlc >= 4; with dlc=1 all are skipped
    EXPECT_TRUE(signals.empty());
}

TEST_F(SignalDecoderTest, OutputBufferIsReusedAcrossCalls)
{
    // Second call must not bleed values from the first call
    const uint8_t data_a[8] = {0x2C, 0x01, 0xDC, 0x05, 0, 0, 0, 0};
    const uint8_t data_b[8] = {0x00, 0x00, 0x00, 0x00, 0, 0, 0, 0};

    decoder_.decode(make_frame(256, 8, data_a));
    const auto signals = decoder_.decode(make_frame(256, 8, data_b));

    ASSERT_EQ(signals.size(), 2u);
    EXPECT_NEAR(std::get<float>(signals[0].value), 0.0f, 0.001f);
    EXPECT_EQ(std::get<uint32_t>(signals[1].value), 0u);
}

TEST_F(SignalDecoderTest, TimestampIsSet)
{
    const uint8_t data[8] = {};
    const auto signals = decoder_.decode(make_frame(256, 8, data));
    ASSERT_GE(signals.size(), 1u);

    // Timestamp must be a recent time point (within last second)
    const auto age = std::chrono::steady_clock::now() - signals[0].timestamp;
    EXPECT_LT(age, std::chrono::seconds(1));
}
