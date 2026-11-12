#!/usr/bin/env python3
"""Compile production bodies with deterministic transport/allocation fault injection.

Only module/import declarations and the SteamNetworkingUtils accessor are changed.
The normal CMake messages target independently tests the real modules and GNS.
"""
import argparse
import pathlib
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--compiler', default='g++')
parser.add_argument('--sanitize', action='store_true')
args = parser.parse_args()
root = pathlib.Path(__file__).resolve().parent.parent

def body(path):
    return '\n'.join(line.replace('export ', '') for line in (root / path).read_text().splitlines()
                     if not line.startswith(('module;', 'export module ', 'import ', 'export import ')))

source = r'''
#define LIGHTNERX_SOURCE_TEST
#include <bits/stdc++.h>
#include <steam/isteamnetworkingutils.h>
bool failAllocation = false, throwSend = false;
int liveMessages = 0;
int sendsBeforeThrow = 0;
void (*cleanupHook)() noexcept = nullptr;
struct TestMessage : SteamNetworkingMessage_t {};
struct TestUtils {
    SteamNetworkingMessage_t* AllocateMessage(int size) {
        if (failAllocation) return nullptr;
        auto* message = new TestMessage{};
        message->m_cbSize = size;
        message->m_pData = new std::byte[size];
        message->m_pfnRelease = [](SteamNetworkingMessage_t* value) {
            delete[] static_cast<std::byte*>(value->m_pData);
            delete static_cast<TestMessage*>(value);
            --liveMessages;
        };
        ++liveMessages;
        return message;
    }
};
TestUtils* injectedUtils() { static TestUtils utils; return &utils; }
namespace lightnerx::network {
    struct ConnectionManager {
        ConnectionManager(int, int) {}
        std::optional<uint16_t> add(HSteamNetConnection h) { return getClientId(h); }
        std::optional<uint16_t> getClientId(HSteamNetConnection h) const {
            if (h == 1) return 1;
            return std::nullopt;
        }
    };
    namespace detail { void setNetworkCleanup(void (*hook)() noexcept) { cleanupHook = hook; } }
    std::expected<void, std::error_code> sendToConnection(std::span<const std::byte>, HSteamNetConnection) {
        if (throwSend && sendsBeforeThrow-- == 0) throw 23;
        return std::unexpected(std::make_error_code(std::errc::io_error));
    }
    std::expected<void, std::error_code> sendToConnections(std::span<const std::byte> data, std::vector<HSteamNetConnection>) {
        return sendToConnection(data, 0);
    }
    std::expected<void, std::error_code> broadcast(std::span<const std::byte> data, const ConnectionManager&, std::vector<HSteamNetConnection>) {
        return sendToConnection(data, 0);
    }
}
'''
for path in ['communication/base_protocol.cppm', 'communication/mbt.cppm',
             'communication/intention.cppm', 'communication/notification.cppm',
             'communication/event.cppm', 'communication/response.cppm',
             'network/target.cppm', 'network/message_batch.cppm', 'communication/message.cppm']:
    source += '\n' + body(path).replace('SteamNetworkingUtils()', 'injectedUtils()')
source += '\n' + (root / 'tests/messages.cpp').read_text()
with tempfile.TemporaryDirectory(prefix='lightnerx-message-test-') as directory:
    cpp = pathlib.Path(directory) / 'messages.cpp'
    binary = pathlib.Path(directory) / 'messages'
    cpp.write_text(source)
    command = [args.compiler, '-std=c++26', '-g', '-O1',
               '-I' + str(root / 'external/GameNetworkingSockets/include'), str(cpp), '-o', str(binary)]
    if args.sanitize:
        command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-sanitize-recover=all']
    subprocess.run(command, check=True)
    subprocess.run([str(binary)], check=True)
