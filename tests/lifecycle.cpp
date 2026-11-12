#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>

import std;
import lightnerx.network.client;
import lightnerx.network.server;
import lightnerx.network.transport;
import lightnerx.network.connection_manager;
import lightnerx.network.network_manager;

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
            check(client.start().error() == std::errc::operation_in_progress, "client duplicate start");
            auto* interface = SteamNetworkingSockets();
            {
                Client other;
                Server server;
                check(other.start().error() == std::errc::device_or_resource_busy, "second client rejected");
                check(server.start(45333, 1000).error() == std::errc::device_or_resource_busy, "mixed owners rejected");
                check(!server.update(), "unstarted server guard");
            }
            networkKill();
            check(isNetworkInitialized() && SteamNetworkingSockets() == interface, "active owner protects interface");
            check(client.update(1).has_value(), "idle client remains usable");
        }
        check(!isNetworkInitialized() && !SteamNetworkingSockets(), "client destructor releases network");
    }
    Server server;
    check(server.start(-1, 1000).error() == std::errc::invalid_argument, "invalid port");
    check(server.start(0, 1000).error() == std::errc::invalid_argument, "GNS requires an explicit listen port");
    check(!isNetworkInitialized(), "argument rejection preserves network state");
    std::println("Lifecycle and ownership regressions passed");
}

void runServer(int port) {
    Server server;
    ConnectionManager connections(2, 2);
    int accepted = 0, disconnected = 0, received = 0;
    int addedConnectCalls = 0, addedReceiveCalls = 0, caught = 0;
    server.addClientConnectedCallback([&](HSteamNetConnection handle) {
        check(connections.add(handle).has_value(), "register peer");
        ++accepted;
        if (accepted == 1) {
            server.addClientConnectedCallback([&](auto) { ++addedConnectCalls; });
            throw std::runtime_error("expected admission exception");
        }
    });
    server.addClientDisconnectedCallback([&](HSteamNetConnection handle) {
        auto id = connections.getClientId(handle);
        check(id.has_value(), "closed connection retains identity");
        connections.remove(*id);
        if (++disconnected == 3) server.close();
    });
    server.addClientReceivedMessageCallback([&](std::span<const std::byte> data, auto handle) {
        check(server.update().error() == std::errc::operation_in_progress, "recursive server update");
        check(data.size() == 1 && (data[0] == std::byte{0x61} || data[0] == std::byte{0x62}), "server received payload");
        if (++received == 1) server.addClientReceivedMessageCallback([&](auto, auto) { ++addedReceiveCalls; });
        if (data[0] == std::byte{0x62}) {
            check(server.disconnectConnection(handle, false).has_value(), "server initiated disconnect");
        } else {
            check(sendToConnection(data, handle).has_value(), "echo send");
        }
    });
    check(server.start(port, 3000).has_value(), "IP listener start");
    check(server.start(port, 3000).error() == std::errc::operation_in_progress, "server duplicate start");
    {
        Server second;
        check(second.start(port, 3000).error() == std::errc::device_or_resource_busy, "second server rejected");
    }
    std::cout << "READY\n" << std::flush;
    const auto deadline = std::chrono::steady_clock::now() + 15s;
    while (disconnected < 3 && std::chrono::steady_clock::now() < deadline) {
        try {
            check(server.update().has_value(), "server update");
        } catch (const std::runtime_error& error) {
            if (std::string_view(error.what()) != "expected admission exception") throw;
            ++caught;
        }
        std::this_thread::sleep_for(1ms);
    }
    check(accepted == 3 && disconnected == 3 && received == 3, "three complete sessions");
    check(caught == 1 && addedConnectCalls == 2 && addedReceiveCalls == 1, "callback contracts");
    check(connections.getOnlineCount() == 0 && !isNetworkInitialized(), "callback close completes teardown");
    check(server.start(port, 3000).has_value(), "server restart after deferred close");
    server.close();
    server.close();
    check(!isNetworkInitialized(), "server repeated close");
    std::println("Server integration passed");
}

void runClient(int port) {
    Client client;
    int received = 0, addedCalls = 0;
    client.addServerReceivedMessageCallback([&](std::span<const std::byte> data) {
        check(client.update(1).error() == std::errc::operation_in_progress, "recursive client update");
        check(data.size() == 1 && data[0] == std::byte{0x61}, "client received payload");
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
                const std::array data{std::byte{0x61}};
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
            const std::array data{std::byte{0x62}};
            check(sendToConnection(data, client.getServerHConn()).has_value(), "request server disconnect");
            sent = true;
        }
        std::this_thread::sleep_for(1ms);
    }
    check(sent && !client.isConnected() && !client.getServerHConn(), "remote disconnect resets client state");
    std::println("Client integration passed");
}

int main(int argc, char** argv) {
    try {
        if (argc == 3 && std::string_view(argv[1]) == "--server") runServer(std::stoi(argv[2]));
        else if (argc == 3 && std::string_view(argv[1]) == "--client") runClient(std::stoi(argv[2]));
        else smoke();
    } catch (const std::exception& error) {
        std::println(std::cerr, "{}", error.what());
        return 1;
    }
}
