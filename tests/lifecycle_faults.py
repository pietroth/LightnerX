"""Supplement module tests with faults in actual wrapper bodies and real GNS.

Strip module declarations, expose private test state, and replace transport calls
at specific failure boundaries. No production testing API or third-party edits.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--compiler', default='c++')
parser.add_argument('--library', type=Path, required=True)
parser.add_argument('--sanitize', action='store_true')
parser.add_argument('--baseline', action='store_true')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]

source = r'''
#include <bits/stdc++.h>
#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
int liveMessages = 0, clientReceive = 0, serverReceive = 0;
bool failListener = false, failPoll = false;
std::function<void()> serverCallbacks;
std::vector<SteamNetworkingMessage_t*> allocatedMessages;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct TrackedMessage : SteamNetworkingMessage_t {};
SteamNetworkingMessage_t* trackedMessage(HSteamNetConnection handle) {
    auto* message = new TrackedMessage{};
    message->m_pData = new std::byte[1]{std::byte{0x61}};
    message->m_cbSize = 1;
    message->m_conn = handle;
    message->m_pfnRelease = [](auto* value) {
        delete[] static_cast<std::byte*>(value->m_pData);
        std::erase(allocatedMessages, value);
        delete static_cast<TrackedMessage*>(value);
        --liveMessages;
    };
    ++liveMessages;
    allocatedMessages.push_back(message);
    return message;
}
int receiveClient(HSteamNetConnection handle, SteamNetworkingMessage_t** messages, int count) {
    const int result = std::exchange(clientReceive, 0);
    for (int i = 0; i < std::min(result, count); ++i) messages[i] = trackedMessage(handle);
    return std::min(result, count);
}
int receiveServer(HSteamNetPollGroup, SteamNetworkingMessage_t** messages, int) {
    const int result = std::exchange(serverReceive, 0);
    if (result > 0) *messages = trackedMessage(1);
    return result;
}
HSteamListenSocket createListener(const SteamNetworkingIPAddr&, int, const SteamNetworkingConfigValue_t*) {
    return failListener ? 0 : 99991;
}
HSteamNetPollGroup createPoll() {
    return failPoll ? 0 : SteamNetworkingSockets()->CreatePollGroup();
}
void runServerCallbacks() {
    if (serverCallbacks) std::exchange(serverCallbacks, {})();
}
EResult acceptConnection(HSteamNetConnection) { return k_EResultOK; }
'''
for name in ['network_manager', 'client', 'server']:
    path = 'network/' + name + '.cppm'
    code = (subprocess.check_output(['git', 'show', 'HEAD:' + path], cwd=root, text=True)
            if args.baseline else (root / path).read_text())
    code = '\n'.join(line.replace('export ', '') for line in code.splitlines()
                     if not line.startswith(('module;', 'export module ', 'import ', 'export import ')))
    code = code.replace('private:', 'public:')
    for original, replacement in [
        ('m_sockets->ReceiveMessagesOnConnection(', 'receiveClient('),
        ('m_pInterface->ReceiveMessagesOnPollGroup(', 'receiveServer('),
        ('m_pInterface->CreateListenSocketIP(', 'createListener('),
        ('m_pInterface->CreatePollGroup()', 'createPoll()'),
        ('m_pInterface->RunCallbacks()', 'runServerCallbacks()'),
        ('m_pInterface->AcceptConnection(', 'acceptConnection('),
    ]:
        code = code.replace(original, replacement)
    source += '\n' + code

if args.baseline:
    source += r'''
int main() {
    using namespace lightnerx::network;
    std::cout << "LX-05 copyable=" << std::is_copy_constructible_v<Client> << '\n';
    { Client client; check(client.start().has_value(), "start");
      HSteamNetConnection a, b;
      check(SteamNetworkingSockets()->CreateSocketPair(&a, &b, false, nullptr, nullptr), "pair");
      client.m_connection = a; client.connected = true;
      client.addServerReceivedMessageCallback([](auto) { throw 42; });
      clientReceive = 3;
      try { client.update(3); } catch (int) {}
      std::cout << "LX-14 messages leaked after callback exception=" << liveMessages << '\n';
      while (!allocatedMessages.empty()) allocatedMessages.back()->Release();
      check(client.disconnect(false).has_value(), "disconnect");
      clientReceive = -1;
      std::cout << "LX-06 connected=" << client.isConnected() << " handle=" << client.getServerHConn()
                << " LX-07 receive-error-reported-as-success=" << bool(client.update(1)) << '\n';
      Server server;
      std::cout << "LX-11 mixed owners accepted=" << bool(server.start(45333, 1000))
                << " repeat server start accepted=" << bool(server.start(45333, 1000)) << '\n';
    }
    std::cout << "LX-01 flag=" << isNetworkInitialized() << " interface=" << bool(SteamNetworkingSockets())
              << " reinit=" << networkInit() << " recreated=" << bool(SteamNetworkingSockets()) << '\n';
    networkKill();
    { Server server; failListener = true;
      check(!server.start(45333, 1000), "injected listener failure");
      std::cout << "LX-01 failed server flag=" << isNetworkInitialized()
                << " cached interface=" << bool(server.m_pInterface)
                << " real interface=" << bool(SteamNetworkingSockets()) << '\n';
      networkKill();
    }
}
'''
else:
    source += r'''
using namespace lightnerx::network;
void attachPair(Client& client) {
    HSteamNetConnection a, b;
    check(SteamNetworkingSockets()->CreateSocketPair(&a, &b, false, nullptr, nullptr), "socketpair");
    client.m_connection = a;
    client.connected = true;
}
int main() {
    try {
        { Client client; check(client.start().has_value(), "start"); attachPair(client);
          check(client.disconnect(false).has_value(), "disconnect");
          check(!client.isConnected() && !client.getServerHConn(), "LX-06 local reset");
          check(client.connect("invalid", 1234).error() == std::errc::invalid_argument, "reconnect reaches address validation");
        }
        check(!isNetworkInitialized() && !SteamNetworkingSockets(), "LX-01 destructor state");
        check(networkInit() && SteamNetworkingSockets(), "LX-01 restart"); networkKill();
        { Client client; check(client.start().has_value(), "start"); attachPair(client);
          clientReceive = -1;
          check(client.update(2).error() == std::errc::io_error, "LX-07 client receive failure");
          check(!client.isConnected() && !client.getServerHConn(), "receive failure resets invalid handle");
        }
        { Client client; check(client.start().has_value(), "start"); attachPair(client);
          bool throws = true;
          int messageIndex = 0;
          client.addServerReceivedMessageCallback([&](auto) { if (throws && ++messageIndex == 2) throw 42; });
          clientReceive = 3;
          try { client.update(3); check(false, "callback exception lost"); } catch (int value) { check(value == 42, "exception identity"); }
          check(liveMessages == 0, "LX-14 entire receive batch released");
          throws = false;
          int calls = 0, added = 0;
          client.addServerReceivedMessageCallback([&](auto) {
              ++calls;
              check(client.update(1).error() == std::errc::operation_in_progress, "recursive update rejected");
              for (int i = 0; i < 1000; ++i) client.addServerReceivedMessageCallback([&](auto) { ++added; });
              check(client.disconnect(false).has_value(), "disconnect inside callback");
          });
          clientReceive = 3;
          check(client.update(3).has_value(), "update after callback exception");
          check(calls == 1 && added == 0 && liveMessages == 0, "LX-14 reentrant disconnect releases pending batch");
        }
        { Server server;
          failListener = true;
          check(!server.start(45333, 1000), "listener fault");
          check(!isNetworkInitialized() && !server.m_pInterface && !server.m_hListenSocket, "LX-01 listener rollback");
          failListener = false; failPoll = true;
          check(!server.start(45333, 1000), "poll group fault");
          check(!isNetworkInitialized() && !server.m_pInterface && !server.m_hListenSocket && !server.m_hPollGroup, "LX-01 poll rollback");
          failPoll = false;
          check(server.start(45333, 1000).has_value(), "server restart after start failure");
          serverReceive = -1;
          check(server.update().error() == std::errc::io_error, "LX-07 server receive failure");
          server.addClientReceivedMessageCallback([&](auto, auto) {
              server.close();
              check(isNetworkInitialized(), "close deferred until received message released");
              throw 43;
          });
          serverReceive = 1;
          try { server.update(); check(false, "server exception lost"); } catch (int value) { check(value == 43, "server exception identity"); }
          check(liveMessages == 0 && !isNetworkInitialized(), "server exception cleanup precedes shutdown");
        }
        { Server server; check(server.start(45333, 1000).has_value(), "server start");
          HSteamNetConnection a, b;
          check(SteamNetworkingSockets()->CreateSocketPair(&a, &b, false, nullptr, nullptr), "pair");
          server.addClientConnectedCallback([&](auto) {
              server.addClientConnectedCallback([](auto) {});
              throw 44;
          });
          SteamNetConnectionStatusChangedCallback_t event{};
          event.m_hConn = a;
          event.m_info.m_hListenSocket = server.m_hListenSocket;
          event.m_info.m_eState = k_ESteamNetworkingConnectionState_Connecting;
          serverCallbacks = [&] { Server::SteamNetConnectionStatusChangedCallback(&event); };
          try { server.update(); check(false, "status exception lost"); } catch (int value) { check(value == 44, "deferred status exception"); }
          check(!server.callbackException && server.update().has_value(), "callback exception cleared");
          server.addClientDisconnectedCallback([&](auto) {
              server.close();
              check(isNetworkInitialized(), "close deferred in direct disconnect callback");
              throw 45;
          });
          try { server.disconnectConnection(a, false); check(false, "disconnect exception lost"); } catch (int value) { check(value == 45, "disconnect exception identity"); }
          check(!isNetworkInitialized(), "direct disconnect unwind closes server");
        }
        check(liveMessages == 0, "all messages released");
        std::puts("Lifecycle fault regressions passed");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what()); return 1;
    }
}
'''

with tempfile.TemporaryDirectory(prefix='lightnerx-lifecycle-') as directory:
    cpp = Path(directory) / 'lifecycle.cpp'
    executable = Path(directory) / 'lifecycle'
    cpp.write_text(source)
    command = [args.compiler, '-std=c++26', '-g', '-O1',
               '-I' + str(root / 'external/GameNetworkingSockets/include'), str(cpp),
               str(args.library.resolve()), '-Wl,-rpath,' + str(args.library.resolve().parent), '-o', str(executable)]
    if args.sanitize:
        # The pinned GNS build uses -fno-rtti. UBSan vptr diagnoses even the
        # first virtual call after successful raw GNS initialization, without
        # LightnerX involved. Keep all compatible checks, including ASan UAF.
        command += ['-fsanitize=address,undefined', '-fno-sanitize=vptr',
                    '-fno-omit-frame-pointer', '-fno-sanitize-recover=all']
    subprocess.run(command, check=True)
    subprocess.run([str(executable)], check=True, timeout=30)
