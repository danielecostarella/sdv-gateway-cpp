#include <spdlog/spdlog.h>

// Pipeline wiring is added in feat(gateway).
// This stub ensures the build target exists from the first commit.
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    spdlog::info("sdv-gateway starting — pipeline not yet wired");
    return 0;
}
