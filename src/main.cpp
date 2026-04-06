#include "SpscQueue.hpp"

#include <can/SocketCANReader.hpp>
#include <decoder/MappingConfig.hpp>
#include <decoder/SignalDecoder.hpp>
#include <kuksa/KuksaClient.hpp>

#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <string>
#include <thread>

namespace {

std::atomic<bool> g_shutdown{false};

void on_signal(int) noexcept
{
    g_shutdown.store(true, std::memory_order_relaxed);
}

struct Args {
    std::string can_interface  = "vcan0";
    std::string mapping_path   = "config/mapping.json";
    std::string kuksa_endpoint = "localhost:55555";
    bool        tls_enabled    = false;
    std::string auth_token;
    bool        verbose        = false;
};

Args parse_args(int argc, char* argv[])
{
    Args a;
    for (int i = 1; i + 1 < argc; ++i) {
        const std::string key(argv[i]);
        const std::string val(argv[i + 1]);
        if (key == "--can")      a.can_interface  = val;
        if (key == "--config")   a.mapping_path   = val;
        if (key == "--endpoint") a.kuksa_endpoint = val;
        if (key == "--token")    a.auth_token      = val;
        if (key == "--tls")      a.tls_enabled     = (val == "true");
    }
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--verbose") a.verbose = true;
    }
    return a;
}

} // namespace

// 1 024-slot queue: covers ~100 s of bursts at the 100 ms simulator interval.
// Sized as power-of-two per SpscQueue contract (see ADR-002).
using FrameQueue = sdvgw::SpscQueue<sdvgw::can::CanFrame, 1024>;

int main(int argc, char* argv[])
{
    const auto args = parse_args(argc, argv);

    spdlog::set_level(args.verbose ? spdlog::level::trace : spdlog::level::info);
    spdlog::info("sdv-gateway starting");
    spdlog::info("  can interface : {}", args.can_interface);
    spdlog::info("  config        : {}", args.mapping_path);
    spdlog::info("  kuksa endpoint: {}", args.kuksa_endpoint);
    spdlog::info("  tls           : {}", args.tls_enabled ? "enabled" : "disabled");

    struct sigaction sa{};
    sa.sa_handler = on_signal;
    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    // -----------------------------------------------------------------------
    // Layer 2 — Signal: load mapping config
    // -----------------------------------------------------------------------
    auto cfg = sdvgw::decoder::MappingConfig::from_file(args.mapping_path);
    if (!cfg) {
        spdlog::critical("failed to load mapping config '{}' — aborting",
                         args.mapping_path);
        return 1;
    }

    // -----------------------------------------------------------------------
    // Layer 1 — Transport: open CAN interface
    // -----------------------------------------------------------------------
    sdvgw::can::SocketCANReader reader;
    if (!reader.open(args.can_interface.c_str())) {
        spdlog::critical("failed to open CAN interface '{}' — aborting",
                         args.can_interface);
        return 1;
    }

    // -----------------------------------------------------------------------
    // Layer 3 — Service: connect to Kuksa Databroker
    // -----------------------------------------------------------------------
    sdvgw::kuksa::KuksaClient publisher(sdvgw::kuksa::KuksaConfig{
        .endpoint    = args.kuksa_endpoint,
        .tls_enabled = args.tls_enabled,
        .auth_token  = args.auth_token,
    });

    if (!publisher.connect()) {
        spdlog::critical("failed to connect to Kuksa Databroker at '{}' — aborting",
                         args.kuksa_endpoint);
        reader.close();
        return 1;
    }

    // -----------------------------------------------------------------------
    // SPSC queue between Transport and Signal threads (ADR-002)
    // -----------------------------------------------------------------------
    FrameQueue queue;
    std::atomic<uint64_t> frames_dropped{0};

    // -----------------------------------------------------------------------
    // Thread 1 — Transport: CAN read loop
    // Pushes raw frames into the queue. Never blocks on downstream pressure.
    // -----------------------------------------------------------------------
    std::thread reader_thread([&] {
        reader.run([&](const sdvgw::can::CanFrame& frame) {
            if (!queue.push(frame)) {
                frames_dropped.fetch_add(1, std::memory_order_relaxed);
            }
        });
    });

    // -----------------------------------------------------------------------
    // Thread 2 — Signal + Service: decode and publish
    // Drains the queue, decodes frames, forwards VSS signals to Kuksa.
    // -----------------------------------------------------------------------
    sdvgw::decoder::SignalDecoder decoder(*cfg);

    std::thread pipeline_thread([&] {
        sdvgw::can::CanFrame frame;
        while (!g_shutdown.load(std::memory_order_relaxed)) {
            if (!queue.pop(frame)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            const auto signals = decoder.decode(frame);
            if (!signals.empty()) {
                publisher.publish(signals);
            }
        }
        // Drain remaining frames before exit
        while (queue.pop(frame)) {
            const auto signals = decoder.decode(frame);
            if (!signals.empty()) publisher.publish(signals);
        }
    });

    spdlog::info("sdv-gateway running — send SIGTERM or press Ctrl+C to stop");

    while (!g_shutdown.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // -----------------------------------------------------------------------
    // Graceful shutdown
    // -----------------------------------------------------------------------
    spdlog::info("shutting down...");
    reader.stop();          // unblocks poll() in SocketCANReader::run()
    reader_thread.join();
    pipeline_thread.join(); // completes queue drain first

    const uint64_t dropped = frames_dropped.load(std::memory_order_relaxed);
    if (dropped > 0) {
        spdlog::warn("{} frame(s) dropped — consider increasing queue capacity",
                     dropped);
    }

    spdlog::info("sdv-gateway stopped");
    return 0;
}
