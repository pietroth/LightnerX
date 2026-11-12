#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingsockets.h>

import std;
import lightnerx.network.network_manager;
import lightnerx.network.connection_manager;

namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    using namespace lightnerx::network;
    try {
        check(networkInit(), "networkInit");
        auto* sockets = SteamNetworkingSockets();
        HSteamNetConnection a, b;
        check(sockets->CreateSocketPair(&a, &b, false, nullptr, nullptr), "CreateSocketPair");

        ConnectionManager manager(1, 4);
        check(!manager.getClientId(a), "LX-03: an unregistered connection has no ID");
        check(!manager.getClientId(k_HSteamNetConnection_Invalid), "LX-03: invalid handle has no ID");
        check(!manager.add(k_HSteamNetConnection_Invalid), "LX-12: invalid add");
        sockets->SetConnectionUserData(a, 12345);
        const auto id = manager.add(a);
        check(id && *id == 1, "LX-04: IDs start at one");
        check(!manager.add(a), "LX-12: duplicate add rejected");
        check(manager.getOnlineCount() == 1, "LX-12: duplicate has no side effects");
        check(sockets->GetConnectionUserData(a) == 12345, "LX-12: application userdata preserved");

        ConnectionManager other(1, 4);
        check(!other.getClientId(a), "LX-12: independent membership");
        const auto otherId = other.add(a);
        check(otherId.has_value(), "LX-12: another manager can register a handle independently");
        other.remove(*otherId);
        check(manager.getClientId(a) == id, "LX-12: another manager cannot erase identity");
        check(sockets->GetConnectionUserData(a) == 12345, "LX-12: remove preserves userdata");

        bool propagated = false;
        try {
            manager.forEach([](uint16_t, HSteamNetConnection) { throw std::runtime_error("callback"); });
        } catch (const std::runtime_error&) { propagated = true; }
        check(propagated, "LX-14: forEach callback exceptions propagate");

        check(sockets->CloseConnection(a, 0, nullptr, false), "CloseConnection");
        check(manager.getClientId(a) == id, "LX-02: disconnect callback can resolve closed handle");
        manager.remove(*manager.getClientId(a));
        check(manager.getOnlineCount() == 0 && !manager.getClientId(a), "LX-02: removal after close");

        ConnectionManager recycled(1, 1);
        for (int cycle = 0; cycle < 70000; ++cycle) {
            auto next = recycled.add(b);
            check(next && *next == 1, "LX-04: latest ID must be recycled without overflow");
            int visits = 0;
            recycled.forEach([&](uint16_t current, HSteamNetConnection handle) {
                check(++visits == 1 && current == 1 && handle == b, "LX-04: bounded iteration");
            });
            check(visits == 1, "LX-04: exactly one registration");
            recycled.remove(*next);
        }
        auto recycledId = recycled.add(b);
        recycled.forEach([&](uint16_t current, HSteamNetConnection) { recycled.remove(current); });
        check(recycledId && recycled.getOnlineCount() == 0, "LX-14: remove during iteration");
        recycled.remove(0);
        recycled.remove(-1);
        recycled.remove(65536);
        check(recycled.getOnlineCount() == 0, "LX-04: invalid removals do not change count");
        ConnectionManager disabled(0, 0);
        check(!disabled.add(b), "LX-04: zero capacity");

        check(sockets->CloseConnection(b, 0, nullptr, false), "CloseConnection peer");
        networkKill();
        std::println("ConnectionManager regressions passed");
        return 0;
    } catch (const std::exception& error) {
        std::println(std::cerr, "{}", error.what());
        networkKill();
        return 1;
    }
}
