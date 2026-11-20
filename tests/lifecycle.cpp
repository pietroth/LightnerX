#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <type_traits>
#include <lightnerx/network/client.hpp>
#include <lightnerx/network/server.hpp>
#include <lightnerx/network/transport.hpp>
#include <lightnerx/network/connection_manager.hpp>
#include <lightnerx/network/network_manager.hpp>

using namespace lightnerx::network;
using namespace std::chrono_literals;

static_assert(!std::is_copy_constructible_v<Client> && !std::is_copy_assignable_v<Client>);
static_assert(!std::is_move_constructible_v<Client> && !std::is_move_assignable_v<Client>);
static_assert(!std::is_copy_constructible_v<Server> && !std::is_move_constructible_v<Server>);
static_assert(k_invalidConnection == k_HSteamNetConnection_Invalid);

void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

void smoke() {
    networkKill();
    check(!isNetworkInitialized(), "initial state");
    for (int i = 0; i < 3; ++i) {
        check(networkInit() && networkInit(), "idempotent initialization");
        networkKill();
        networkKill();
        check(!isNetworkInitialized() && !SteamNetworkingSockets(), "global teardown");
        {
            Client client;
            check(!client.disconnect(false) && !client.update(1), "unstarted client guards");
            check(client.start().has_value(), "client start");
            check(client.start().error() == lightnerx::ErrorCode::OperationInProgress, "client duplicate start");
            auto* interface = SteamNetworkingSockets();
            {
                Client other;
                Server server;
                check(other.start().error() == lightnerx::ErrorCode::DeviceOrResourceBusy, "second client rejected");
                check(server.start(45333, 1000).error() == lightnerx::ErrorCode::DeviceOrResourceBusy, "mixed owners rejected");
                check(!server.update(), "unstarted server guard");
            }
            networkKill();
            check(isNetworkInitialized() && SteamNetworkingSockets() == interface, "active owner protects interface");
            check(client.update(1).has_value(), "idle client remains usable");
        }
        check(!isNetworkInitialized() && !SteamNetworkingSockets(), "client destructor releases network");
    }
    Server server;
    check(server.start(-1, 1000).error() == lightnerx::ErrorCode::InvalidArgument, "invalid port");
    check(server.start(0, 1000).error() == lightnerx::ErrorCode::InvalidArgument, "GNS requires an explicit listen port");
    check(!isNetworkInitialized(), "argument rejection preserves network state");
    {
        Client client;
        check(client.start().has_value(), "client start for literal validation");
        for (const std::string_view ip : {"127.0.0.1abc", "127.0.0.1:1234", "[127.0.0.1]",
                                          "127.0.0.256", "::1xyz", "[::1]:1234"}) {
            const auto result = client.connect(ip, 45333);
            check(!result && result.error() == lightnerx::ErrorCode::InvalidArgument, "invalid IP literal accepted");
        }
        constexpr char withNul[] = "127.0.0.1\0suffix";
        const auto result = client.connect(std::string_view(withNul, sizeof(withNul) - 1), 45333);
        check(!result && result.error() == lightnerx::ErrorCode::InvalidArgument, "embedded NUL accepted");
        check(client.getServerHConn() == k_invalidConnection, "invalid IP created a connection");
        check(client.connect("[::1]", 45333).has_value(), "bracketed IPv6 literal rejected");
        check(client.disconnect(false).has_value(), "IPv6 cleanup");
    }
    std::cout << "Lifecycle and ownership regressions passed" << '\n';
}

void runServer(int port) {
    Server server;
    ConnectionManager connections(2, 2);
    int accepted = 0, disconnected = 0, received = 0;
    int addedConnectCalls = 0, addedReceiveCalls = 0;
    server.addClientConnectedCallback([&](HSteamNetConnection handle) {
        check(connections.add(handle).has_value(), "register peer");
        ++accepted;
        if (accepted == 1) {
            server.addClientConnectedCallback([&](auto) { ++addedConnectCalls; });
        }
    });
    server.addClientDisconnectedCallback([&](HSteamNetConnection handle) {
        auto id = connections.getClientId(handle);
        check(id.has_value(), "closed connection retains identity");
        connections.remove(*id);
        if (++disconnected == 3) server.close();
    });
    server.addClientReceivedMessageCallback([&](std::string_view data, auto handle) {
        check(server.update().error() == lightnerx::ErrorCode::OperationInProgress, "recursive server update");
        check(data.size() == 1 && (data[0] == 'a' || data[0] == 'b'), "server received payload");
        if (++received == 1) server.addClientReceivedMessageCallback([&](auto, auto) { ++addedReceiveCalls; });
        if (data[0] == 'b') {
            check(server.disconnectConnection(handle, false).has_value(), "server initiated disconnect");
        } else {
            check(sendToConnection(data, handle).has_value(), "echo send");
        }
    });
    check(server.start(port, 3000).has_value(), "IP listener start");
    check(server.start(port, 3000).error() == lightnerx::ErrorCode::OperationInProgress, "server duplicate start");
    {
        Server second;
        check(second.start(port, 3000).error() == lightnerx::ErrorCode::DeviceOrResourceBusy, "second server rejected");
    }
    std::cout << "READY\n" << std::flush;
    const auto deadline = std::chrono::steady_clock::now() + 15s;
    while (disconnected < 3 && std::chrono::steady_clock::now() < deadline) {
        check(server.update().has_value(), "server update");
        std::this_thread::sleep_for(1ms);
    }
    check(accepted == 3 && disconnected == 3 && received == 3, "three complete sessions");
    check(addedConnectCalls == 2 && addedReceiveCalls == 1, "callback contracts");
    check(connections.getOnlineCount() == 0 && !isNetworkInitialized(), "callback close completes teardown");
    check(server.start(port, 3000).has_value(), "server restart after deferred close");
    server.close();
    server.close();
    check(!isNetworkInitialized(), "server repeated close");
    std::cout << "Server integration passed" << '\n';
}

void runClient(int port) {
    Client client;
    int received = 0, addedCalls = 0;
    client.addServerReceivedMessageCallback([&](std::string_view data) {
        check(client.update(1).error() == lightnerx::ErrorCode::OperationInProgress, "recursive client update");
        check(data.size() == 1 && data[0] == 'a', "client received payload");
        if (++received == 1) client.addServerReceivedMessageCallback([&](auto) { ++addedCalls; });
    });
    check(client.start().has_value(), "client start");
    for (int round = 1; round <= 2; ++round) {
        check(client.connect("127.0.0.1", port).has_value(), "connect/reconnect");
        bool sent = false;
        const auto deadline = std::chrono::steady_clock::now() + 10s;
        while (received < round && std::chrono::steady_clock::now() < deadline) {
            check(client.update(8).has_value(), "client update");
            if (client.isConnected() && !sent) {
                const std::string_view data = "a";
                check(sendToConnection(data, client.getServerHConn()).has_value(), "client send");
                sent = true;
            }
            std::this_thread::sleep_for(1ms);
        }
        check(received == round, "echo timeout");
        check(client.disconnect(false).has_value(), "local disconnect");
        check(!client.isConnected() && client.getServerHConn() == k_invalidConnection, "local state reset");
        check(client.update(8).has_value(), "idle update after disconnect");
        std::this_thread::sleep_for(50ms);
    }
    check(addedCalls == 1, "client callback registration applies next message");
    check(client.connect("127.0.0.1", port).has_value(), "connect for remote disconnect");
    bool sent = false;
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    while (client.getServerHConn() != k_invalidConnection && std::chrono::steady_clock::now() < deadline) {
        check(client.update(8).has_value(), "remote disconnect update");
        if (client.isConnected() && !sent) {
            const std::string_view data = "b";
            check(sendToConnection(data, client.getServerHConn()).has_value(), "request server disconnect");
            sent = true;
        }
        std::this_thread::sleep_for(1ms);
    }
    check(sent && !client.isConnected() && !client.getServerHConn(), "remote disconnect resets client state");
    std::cout << "Client integration passed" << '\n';
}

int main(int argc, char** argv) {
    try {
        if (argc == 3 && std::string_view(argv[1]) == "--server") runServer(std::stoi(argv[2]));
        else if (argc == 3 && std::string_view(argv[1]) == "--client") runClient(std::stoi(argv[2]));
        else smoke();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
