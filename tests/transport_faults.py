"""Check routing, error order and queue ownership with the production bodies."""
import argparse
from pathlib import Path
import shlex
import subprocess
import tempfile
from source_support import INCLUDE_FLAGS, body, replace_once

parser = argparse.ArgumentParser()
parser.add_argument('--compiler', required=True)
parser.add_argument('--flags', default='')
parser.add_argument('--sanitize', action='store_true')
args = parser.parse_args()
source = r'''
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>
#include <steam/steamnetworkingsockets.h>
#include <lightnerx/network/connection_handle.hpp>
std::vector<HSteamNetConnection> sent;
std::uint32_t lastSendSize = 0;
std::optional<std::size_t> reportedMessageSize;
bool initialized = true;
int released = 0;

// Exercise the transport's size conversion without allocating a large buffer.
std::size_t transportMessageSize(std::string_view message) {
    return reportedMessageSize.value_or(message.size());
}

struct TestSockets {
    EResult SendMessageToConnection(HSteamNetConnection handle, const void*, uint32_t size, int, int64*) {
        lastSendSize = size;
        sent.push_back(handle);
        return handle == 2 ? k_EResultFail : k_EResultOK;
    }
};
TestSockets* injectedSockets() { static TestSockets sockets; return &sockets; }
namespace lightnerx::network {
    bool isNetworkInitialized() { return initialized; }
    struct ConnectionManager {
        std::vector<HSteamNetConnection> handles;
        template<class F> void forEach(const F& callback) const {
            uint16_t id = 0;
            for (auto handle : handles) callback(++id, handle);
        }
    };
    namespace detail { void setNetworkCleanup(void (*)() noexcept) noexcept {} }
}
'''
source += r"""
struct TestMessage : SteamNetworkingMessage_t {
    TestMessage() : SteamNetworkingMessage_t{} {
        m_pfnRelease = [](SteamNetworkingMessage_t* message) {
            ++released;
            delete static_cast<TestMessage*>(message);
        };
    }
};
struct TestUtils {
    SteamNetworkingMessage_t* AllocateMessage(int) { return new TestMessage; }
};
TestUtils* injectedUtils() { static TestUtils utils; return &utils; }
"""
for path in ['network/target.cpp', 'network/transport.cpp', 'network/message_batch.cpp']:
    implementation = body(path).replace('ISteamNetworkingSockets*', 'TestSockets*').replace(
        'SteamNetworkingSockets()', 'injectedSockets()').replace(
        'SteamNetworkingUtils()', 'injectedUtils()')
    if path == 'network/transport.cpp':
        implementation = replace_once(implementation, 'message.size()', 'transportMessageSize(message)')
    source += '\n' + implementation
source += r'''
using namespace lightnerx::network;
#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fputs("Transport test failed: " #condition "\n", stderr); \
        std::abort(); \
    } \
} while (false)

OutboundMessage outgoing(Target target) {
    auto message = OutboundMessage::create({}, {}, std::move(target));
    CHECK(message);
    return std::move(message).value();
}
int main() {
    constexpr auto maxTransportSize = std::numeric_limits<std::uint32_t>::max();
    reportedMessageSize = maxTransportSize;
    if (!sendToConnection({}, 3) || lastSendSize != maxTransportSize) {
        std::fputs("Maximum representable message size was not forwarded\n", stderr);
        return 1;
    }
    sent.clear();
    if constexpr (std::numeric_limits<std::size_t>::max() > maxTransportSize) {
        reportedMessageSize = static_cast<std::size_t>(maxTransportSize) + 2;
        const auto oversized = sendToConnection({}, 3);
        if (oversized || oversized.error() != lightnerx::ErrorCode::MessageSize || !sent.empty()) {
            std::fputs("Oversized message was not rejected before calling the transport\n", stderr);
            return 1;
        }
    }
    reportedMessageSize.reset();

    ConnectionManager connections{{0, 2, 3, 4}};
    const auto group = sendToConnections({}, {0, 2, 3});
    CHECK(!group && group.error() == lightnerx::ErrorCode::NotConnected);
    CHECK((sent == std::vector<HSteamNetConnection>{2, 3}));
    sent.clear();
    const auto reversed = sendToConnections({}, {2, 0, 3});
    CHECK(!reversed && reversed.error() == lightnerx::ErrorCode::IoError);
    CHECK((sent == std::vector<HSteamNetConnection>{2, 3}));
    sent.clear();
    const auto broadcastError = broadcast({}, connections, {4});
    CHECK(!broadcastError && broadcastError.error() == lightnerx::ErrorCode::NotConnected);
    CHECK((sent == std::vector<HSteamNetConnection>{2, 3}));
    sent.clear();
    CHECK(broadcast({}, connections, {0, 2}));
    CHECK((sent == std::vector<HSteamNetConnection>{3, 4}));
    sent.clear();
    initialized = false;
    CHECK(sendToConnections({}, {}));
    CHECK(broadcast({}, connections, {0, 2, 3, 4}));
    CHECK(sent.empty());
    initialized = true;

    const auto invalidCapacity = initMessagesBatcher(-1);
    CHECK(!invalidCapacity && invalidCapacity.error() == lightnerx::ErrorCode::InvalidArgument);
    CHECK(initMessagesBatcher(3));
    CHECK(queueMessage(outgoing(Target::single(2))));
    CHECK(queueMessage(outgoing(Target::single(0))));
    CHECK(queueMessage(outgoing(Target::single(3))));
    auto flushed = flushAll(connections);
    CHECK(!flushed && flushed.error() == lightnerx::ErrorCode::NotConnected);
    CHECK(released == 3 && (sent == std::vector<HSteamNetConnection>{2, 3}));
    CHECK(flushAll(connections) && released == 3);
    sent.clear();
    CHECK(queueMessage(outgoing(Target::single(0))));
    CHECK(queueMessage(outgoing(Target::single(2))));
    flushed = flushAll(connections);
    CHECK(!flushed && flushed.error() == lightnerx::ErrorCode::IoError && released == 5);
    sent.clear();
    CHECK(queueMessage(outgoing(Target::group({3, 4}))));
    CHECK(queueMessage(outgoing(Target::broadcast({0, 2, 4}))));
    CHECK(flushAll(connections) && released == 7);
    CHECK((sent == std::vector<HSteamNetConnection>{3, 4, 3}));

    CHECK(initMessagesBatcher(0));
    // reserve(0) retains prior capacity, just like the original implementation.
    // Fill the actual queue until rejection rather than assuming capacity shrinks.
    int queued = 0;
    while (queueMessage(outgoing(Target::single(3)))) ++queued;
    CHECK(released == 8);
    clearMessagesBatcher();
    CHECK(released == 8 + queued);
    CHECK(flushAll(connections));
    auto original = outgoing(Target::single(3));
    auto moved = std::move(original);
    const auto invalid = queueMessage(std::move(original));
    CHECK(!invalid && invalid.error() == lightnerx::ErrorCode::InvalidArgument);
}
'''
with tempfile.TemporaryDirectory(prefix='lightnerx-transport-') as directory:
    cpp = Path(directory) / 'transport.cpp'
    binary = Path(directory) / 'transport'
    cpp.write_text(source)
    command = [args.compiler, '-std=c++17', *shlex.split(args.flags), '-g', '-O1',
               *INCLUDE_FLAGS,
               str(cpp), '-o', str(binary)]
    if args.sanitize:
        command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-sanitize-recover=all']
    subprocess.run(command, check=True)
    subprocess.run([str(binary)], check=True, timeout=30)
