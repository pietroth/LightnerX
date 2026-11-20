"""Check routing, error order and queue ownership with the production bodies."""
import argparse
from pathlib import Path
import shlex
import subprocess
import tempfile
from module_source import implementation, ROOT

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
#include <system_error>
#include <utility>
#include <vector>
#include <expected>
#include <span>
#include <stdexcept>
#include <variant>
#include <steam/steamnetworkingsockets.h>
std::vector<HSteamNetConnection> sent;
std::uint32_t lastSendSize = 0;
std::optional<std::size_t> reportedMessageSize;
bool initialized = true;
int released = 0;

// Exercise the transport's size conversion without allocating a large buffer.
std::size_t transportMessageSize(std::span<const std::byte> message) {
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
    constexpr HSteamNetConnection k_invalidConnection = k_HSteamNetConnection_Invalid;
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
for path in ['network/target.cppm', 'network/transport.cppm', 'network/message_batch.cppm']:
    code = implementation((ROOT / path).read_text()).replace('ISteamNetworkingSockets*', 'TestSockets*').replace(
        'SteamNetworkingSockets()', 'injectedSockets()')
    if path == 'network/transport.cppm':
        if code.count('message.size()') != 1:
            raise RuntimeError('Expected one transport size conversion')
        code = code.replace('message.size()', 'transportMessageSize(message)')
    source += '\n' + code
source += r'''
using namespace lightnerx::network;
#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fputs("Transport test failed: " #condition "\n", stderr); \
        std::abort(); \
    } \
} while (false)

struct TestMessage : SteamNetworkingMessage_t {
    TestMessage() : SteamNetworkingMessage_t{} {
        m_pfnRelease = [](SteamNetworkingMessage_t* message) {
            ++released;
            delete static_cast<TestMessage*>(message);
        };
    }
};
OutboundMessage outgoing(Target target) { return {new TestMessage, std::move(target)}; }
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
        if (oversized || oversized.error() != std::errc::message_size || !sent.empty()) {
            std::fputs("Oversized message was not rejected before calling the transport\n", stderr);
            return 1;
        }
    }
    reportedMessageSize.reset();

    ConnectionManager connections{{0, 2, 3, 4}};
    const auto group = sendToConnections({}, {0, 2, 3});
    CHECK(!group && group.error() == std::errc::not_connected);
    CHECK((sent == std::vector<HSteamNetConnection>{2, 3}));
    sent.clear();
    const auto reversed = sendToConnections({}, {2, 0, 3});
    CHECK(!reversed && reversed.error() == std::errc::io_error);
    CHECK((sent == std::vector<HSteamNetConnection>{2, 3}));
    sent.clear();
    const auto broadcastError = broadcast({}, connections, {4});
    CHECK(!broadcastError && broadcastError.error() == std::errc::not_connected);
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

    bool rejectedCapacity = false;
    try {
        initMessagesBatcher(-1);
    } catch (const std::invalid_argument&) {
        rejectedCapacity = true;
    }
    CHECK(rejectedCapacity);
    initMessagesBatcher(3);
    CHECK(queueMessage(outgoing(Target::single(2))));
    CHECK(queueMessage(outgoing(Target::single(0))));
    CHECK(queueMessage(outgoing(Target::single(3))));
    auto flushed = flushAll(connections);
    CHECK(!flushed && flushed.error() == std::errc::not_connected);
    CHECK(released == 3 && (sent == std::vector<HSteamNetConnection>{2, 3}));
    CHECK(flushAll(connections) && released == 3);
    sent.clear();
    CHECK(queueMessage(outgoing(Target::single(0))));
    CHECK(queueMessage(outgoing(Target::single(2))));
    flushed = flushAll(connections);
    CHECK(!flushed && flushed.error() == std::errc::io_error && released == 5);
    sent.clear();
    CHECK(queueMessage(outgoing(Target::group({3, 4}))));
    CHECK(queueMessage(outgoing(Target::broadcast({0, 2, 4}))));
    CHECK(flushAll(connections) && released == 7);
    CHECK((sent == std::vector<HSteamNetConnection>{3, 4, 3}));

    initMessagesBatcher(0);
    // reserve(0) retains prior capacity, just like the original implementation.
    // Fill the actual queue until rejection rather than assuming capacity shrinks.
    int queued = 0;
    while (queueMessage(outgoing(Target::single(3)))) ++queued;
    CHECK(released == 8);
    clearMessagesBatcher();
    CHECK(released == 8 + queued);
    CHECK(flushAll(connections));
    const auto invalid = queueMessage({nullptr, Target::single(3)});
    CHECK(!invalid && invalid.error() == std::errc::invalid_argument);
}
'''
with tempfile.TemporaryDirectory(prefix='lightnerx-transport-') as directory:
    cpp = Path(directory) / 'transport.cpp'
    binary = Path(directory) / 'transport'
    cpp.write_text(source)
    command = [args.compiler, '-std=c++26', *shlex.split(args.flags), '-g', '-O1',
               '-I' + str(ROOT / 'external/GameNetworkingSockets/include'),
               str(cpp), '-o', str(binary)]
    if args.sanitize:
        command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-sanitize-recover=all']
    subprocess.run(command, check=True)
    subprocess.run([str(binary)], check=True, timeout=30)
