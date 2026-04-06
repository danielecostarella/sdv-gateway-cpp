#include "KuksaClient.hpp"

#include <spdlog/spdlog.h>

#include <fstream>
#include <sstream>
#include <type_traits>
#include <variant>

namespace sdvgw::kuksa {

namespace {

/// Read a PEM file from disk and return its contents as a string.
std::string read_pem(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("cannot open PEM file: " + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

/// Build the gRPC channel credentials from the KuksaConfig.
std::shared_ptr<grpc::ChannelCredentials> make_credentials(const KuksaConfig& cfg)
{
    if (!cfg.tls_enabled) {
        spdlog::warn("KuksaClient: TLS disabled — use only in local development");
        return grpc::InsecureChannelCredentials();
    }

    grpc::SslCredentialsOptions ssl_opts;
    if (!cfg.ca_cert_path.empty()) {
        ssl_opts.pem_root_certs = read_pem(cfg.ca_cert_path);
    }
    return grpc::SslCredentials(ssl_opts);
}

/// Set the typed value on a Datapoint from a VssSignal::Value variant.
///
/// Proto structure (types.proto):
///   Datapoint { Value value = 2; }
///   Value     { oneof typed_value { float float=17; uint32 uint32=15; bool bool=12; ... } }
///
/// C++ field name rules: protobuf appends '_' to names that clash with C++ keywords,
/// so proto field "float" → set_float_(), proto field "bool" → set_bool_().
void set_datapoint_value(::kuksa::val::v2::Datapoint& dp,
                         const decoder::VssSignal::Value& value)
{
    auto* v = dp.mutable_value();
    std::visit([v](const auto& val) {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, float>)      v->set_float_(val);
        else if constexpr (std::is_same_v<T, uint32_t>) v->set_uint32(val);
        else if constexpr (std::is_same_v<T, bool>)     v->set_bool_(val);
    }, value);
}

} // namespace

KuksaClient::KuksaClient(KuksaConfig config)
    : config_(std::move(config))
{}

bool KuksaClient::connect()
{
    try {
        auto creds = make_credentials(config_);

        grpc::ChannelArguments args;
        args.SetMaxReceiveMessageSize(4 * 1024 * 1024);  // 4 MB

        channel_ = grpc::CreateCustomChannel(config_.endpoint, creds, args);
        stub_    = ::kuksa::val::v2::VAL::NewStub(channel_);

        spdlog::info("KuksaClient: channel created to '{}'", config_.endpoint);
        return true;
    } catch (const std::exception& ex) {
        spdlog::error("KuksaClient: connect failed: {}", ex.what());
        return false;
    }
}

bool KuksaClient::is_connected() const noexcept
{
    if (!channel_) return false;
    const auto state = channel_->GetState(/*try_to_connect=*/false);
    return state == GRPC_CHANNEL_READY || state == GRPC_CHANNEL_IDLE;
}

bool KuksaClient::publish(std::span<const decoder::VssSignal> signals)
{
    if (!stub_) {
        spdlog::error("KuksaClient: publish() called before connect()");
        return false;
    }

    bool all_ok = true;
    for (const auto& signal : signals) {
        if (!publish_one(signal)) all_ok = false;
    }
    return all_ok;
}

bool KuksaClient::publish_one(const decoder::VssSignal& signal)
{
    ::kuksa::val::v2::PublishValueRequest request;
    // SignalID is a message with oneof { int32 id; string path; }
    request.mutable_signal_id()->set_path(signal.vss_path);

    auto* dp = request.mutable_data_point();
    set_datapoint_value(*dp, signal.value);

    // Attach JWT token as bearer if configured (UN R155 authorization)
    grpc::ClientContext context;
    if (!config_.auth_token.empty()) {
        context.AddMetadata("authorization", "Bearer " + config_.auth_token);
    }

    ::kuksa::val::v2::PublishValueResponse response;
    const grpc::Status status = stub_->PublishValue(&context, request, &response);

    if (!status.ok()) {
        spdlog::error("KuksaClient: PublishValue('{}') failed [{}]: {}",
                      signal.vss_path,
                      static_cast<int>(status.error_code()),
                      status.error_message());
        return false;
    }

    spdlog::trace("KuksaClient: published '{}'", signal.vss_path);
    return true;
}

} // namespace sdvgw::kuksa
