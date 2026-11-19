#include <lightnerx/network/client.hpp>

#include <cerrno>
#include <memory>
#include <utility>

namespace {
std::unique_ptr<lightnerx::network::Client> client;
}

extern "C" int startClient() {
    auto candidate = std::make_unique<lightnerx::network::Client>();
    const auto result = candidate->start();
    if (!result) return result.error() == std::errc::device_or_resource_busy ? EBUSY : EIO;
    client = std::move(candidate);
    return 0;
}

extern "C" void stopClient() {
    client.reset();
}

extern "C" bool networkRunning() {
    return lightnerx::network::isNetworkInitialized();
}
