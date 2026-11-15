#!/usr/bin/env python3
"""Compile production bodies with deterministic transport/allocation fault injection.

Only dependency includes and the SteamNetworkingUtils accessor are changed.
The normal CMake messages target independently tests the real library and GNS.
"""
import argparse
import pathlib
import shlex
import subprocess
import tempfile
from source_support import body

parser = argparse.ArgumentParser()
parser.add_argument('--compiler', default='g++')
parser.add_argument('--flags', default='')
parser.add_argument('--sanitize', action='store_true')
args = parser.parse_args()
root = pathlib.Path(__file__).resolve().parent.parent

source = r'''
#define LIGHTNERX_SOURCE_TEST
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <exception>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>
#include <lightnerx/compact/span.hpp>
#include <lightnerx/compact/expected.hpp>
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
    lightnerx::compact::expected<void, std::error_code> sendToConnection(lightnerx::compact::span<const std::byte>, HSteamNetConnection) {
        if (throwSend && sendsBeforeThrow-- == 0) throw 23;
        return lightnerx::compact::unexpected(std::make_error_code(std::errc::io_error));
    }
    lightnerx::compact::expected<void, std::error_code> sendToConnections(lightnerx::compact::span<const std::byte> data, std::vector<HSteamNetConnection>) {
        return sendToConnection(data, 0);
    }
    lightnerx::compact::expected<void, std::error_code> broadcast(lightnerx::compact::span<const std::byte> data, const ConnectionManager&, std::vector<HSteamNetConnection>) {
        return sendToConnection(data, 0);
    }
}
'''
for path in ['communication/base_protocol.cpp', 'communication/mbt.cpp',
             'communication/intention.cpp', 'communication/notification.cpp',
             'communication/event.cpp', 'communication/response.cpp',
             'network/target.cpp', 'network/message_batch.cpp', 'communication/message.cpp']:
    source += '\n' + body(path).replace('SteamNetworkingUtils()', 'injectedUtils()')
source += '\n' + (root / 'tests/messages.cpp').read_text()
with tempfile.TemporaryDirectory(prefix='lightnerx-message-test-') as directory:
    cpp = pathlib.Path(directory) / 'messages.cpp'
    binary = pathlib.Path(directory) / 'messages'
    cpp.write_text(source)
    command = [args.compiler, '-std=c++17', *shlex.split(args.flags), '-g', '-O1',
               '-I' + str(root / 'include'), '-I' + str(root / 'external/GameNetworkingSockets/include'), str(cpp), '-o', str(binary)]
    if args.sanitize:
        command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-sanitize-recover=all']
    subprocess.run(command, check=True)
    subprocess.run([str(binary)], check=True)
