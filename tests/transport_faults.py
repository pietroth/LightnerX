"""Check routing, error order and queue ownership with the production bodies."""
import argparse
from pathlib import Path
import shlex
import subprocess
import tempfile
from source_support import body

parser = argparse.ArgumentParser()
parser.add_argument('--compiler', required=True)
parser.add_argument('--flags', default='')
parser.add_argument('--sanitize', action='store_true')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
source = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <optional>
#include <system_error>
#include <utility>
#include <vector>
#include <lightnerx/compact/span.hpp>
#include <lightnerx/compact/expected.hpp>
#include <steam/steamnetworkingsockets.h>
std::vector<HSteamNetConnection> sent;
bool initialized = true;
int released = 0;
struct TestSockets {
    EResult SendMessageToConnection(HSteamNetConnection handle, const void*, uint32_t, int, int64*) {
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
for path in ['network/target.cpp', 'network/transport.cpp', 'network/message_batch.cpp']:
    source += '\n' + body(path).replace('ISteamNetworkingSockets*', 'TestSockets*').replace(
        'SteamNetworkingSockets()', 'injectedSockets()')
source += r'''
using namespace lightnerx::network;
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
    ConnectionManager connections{{0, 2, 3, 4}};
    const auto group = sendToConnections({}, {0, 2, 3});
    assert(!group && group.error() == std::errc::not_connected);
    assert((sent == std::vector<HSteamNetConnection>{2, 3}));
    sent.clear();
    const auto reversed = sendToConnections({}, {2, 0, 3});
    assert(!reversed && reversed.error() == std::errc::io_error);
    assert((sent == std::vector<HSteamNetConnection>{2, 3}));
    sent.clear();
    const auto broadcastError = broadcast({}, connections, {4});
    assert(!broadcastError && broadcastError.error() == std::errc::not_connected);
    assert((sent == std::vector<HSteamNetConnection>{2, 3}));
    sent.clear();
    assert(broadcast({}, connections, {0, 2}));
    assert((sent == std::vector<HSteamNetConnection>{3, 4}));
    sent.clear();
    initialized = false;
    assert(sendToConnections({}, {}));
    assert(broadcast({}, connections, {0, 2, 3, 4}));
    assert(sent.empty());
    initialized = true;

    initMessagesBatcher(3);
    assert(queueMessage(outgoing(Target::single(2))));
    assert(queueMessage(outgoing(Target::single(0))));
    assert(queueMessage(outgoing(Target::single(3))));
    auto flushed = flushAll(connections);
    assert(!flushed && flushed.error() == std::errc::not_connected);
    assert(released == 3 && (sent == std::vector<HSteamNetConnection>{2, 3}));
    assert(flushAll(connections) && released == 3);
    sent.clear();
    assert(queueMessage(outgoing(Target::single(0))));
    assert(queueMessage(outgoing(Target::single(2))));
    flushed = flushAll(connections);
    assert(!flushed && flushed.error() == std::errc::io_error && released == 5);
    sent.clear();
    assert(queueMessage(outgoing(Target::group({3, 4}))));
    assert(queueMessage(outgoing(Target::broadcast({0, 2, 4}))));
    assert(flushAll(connections) && released == 7);
    assert((sent == std::vector<HSteamNetConnection>{3, 4, 3}));

    initMessagesBatcher(0);
    // reserve(0) retains prior capacity, just like the original implementation.
    // Fill the actual queue until rejection rather than assuming capacity shrinks.
    int queued = 0;
    while (queueMessage(outgoing(Target::single(3)))) ++queued;
    assert(released == 8);
    clearMessagesBatcher();
    assert(released == 8 + queued);
    assert(flushAll(connections));
    const auto invalid = queueMessage({nullptr, Target::single(3)});
    assert(!invalid && invalid.error() == std::errc::invalid_argument);
}
'''
with tempfile.TemporaryDirectory(prefix='lightnerx-transport-') as directory:
    cpp = Path(directory) / 'transport.cpp'
    binary = Path(directory) / 'transport'
    cpp.write_text(source)
    command = [args.compiler, '-std=c++17', *shlex.split(args.flags), '-g', '-O1',
               '-I' + str(root / 'include'), '-I' + str(root / 'external/GameNetworkingSockets/include'),
               str(cpp), '-o', str(binary)]
    if args.sanitize:
        command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-sanitize-recover=all']
    subprocess.run(command, check=True)
    subprocess.run([str(binary)], check=True, timeout=30)
