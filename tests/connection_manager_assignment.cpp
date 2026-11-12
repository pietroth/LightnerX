#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingsockets.h>

import std;
import lightnerx.network.network_manager;
import lightnerx.network.connection_manager;

namespace {
// Restrict fault injection to the assigning thread, leaving GNS workers alone.
thread_local int allocationsBeforeFailure = -1;

struct AllocationFailure {
    explicit AllocationFailure(int successfulAllocations) {
        allocationsBeforeFailure = successfulAllocations;
    }
    ~AllocationFailure() { allocationsBeforeFailure = -1; }
};
}

void* operator new(std::size_t size) {
    if (allocationsBeforeFailure >= 0 && allocationsBeforeFailure-- == 0) {
        throw std::bad_alloc{};
    }
    if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc{};
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

namespace {
using lightnerx::network::ConnectionManager;
using Registration = std::pair<std::uint16_t, HSteamNetConnection>;

static_assert(std::is_copy_constructible_v<ConnectionManager>);
static_assert(std::is_copy_assignable_v<ConnectionManager>);
static_assert(std::is_nothrow_move_constructible_v<ConnectionManager>);
static_assert(std::is_nothrow_move_assignable_v<ConnectionManager>);

void check(bool condition, const char* description) {
    if (!condition) throw std::runtime_error(description);
}

void verifyMappings(const ConnectionManager& manager,
                    std::span<const HSteamNetConnection> handles,
                    std::initializer_list<Registration> expected) {
    check(manager.getOnlineCount() == static_cast<int>(expected.size()), "registration count changed");
    for (std::uint16_t id = 0; id <= handles.size(); ++id) {
        HSteamNetConnection handle = k_HSteamNetConnection_Invalid;
        for (const auto& entry : expected) if (entry.first == id) handle = entry.second;
        check(manager.getHSteamNetConnection(id) == handle, "direct mapping changed");
    }
    for (const auto handle : handles) {
        std::optional<std::uint16_t> id;
        for (const auto& entry : expected) if (entry.second == handle) id = entry.first;
        check(manager.getClientId(handle) == id, "reverse mapping changed");
    }
    std::size_t visits = 0;
    manager.forEach([&](std::uint16_t id, HSteamNetConnection handle) {
        ++visits;
        check(std::ranges::find(expected, Registration{id, handle}) != expected.end(),
              "enumeration contains an unexpected registration");
    });
    check(visits == expected.size(), "enumeration count changed");
}

void assignmentRegression(std::span<const HSteamNetConnection> handles) {
    ConnectionManager source(64, 6);
    for (std::uint16_t id = 1; id <= 4; ++id) {
        check(source.add(handles[id - 1]) == id, "source setup");
    }
    source.remove(2); // Exercise copying the free-ID set as well as both maps.
    const auto sourceEntries = {Registration{1, handles[0]}, Registration{3, handles[2]},
                                Registration{4, handles[3]}};

    // Self-assignment must neither allocate nor change the registrations.
    {
        AllocationFailure failure(0);
        source = source;
    }
    verifyMappings(source, handles, sourceEntries);

    int failures = 0;
    bool completed = false;
    // Fail each successive allocation until the entire assignment can succeed.
    for (int failAt = 0; failAt < 64; ++failAt) {
        ConnectionManager destination(64, 4);
        for (std::uint16_t id = 1; id <= 3; ++id) {
            check(destination.add(handles[id + 3]) == id, "destination setup");
        }
        destination.remove(1);
        bool threw = false;
        ConnectionManager* result = nullptr;
        try {
            AllocationFailure failure(failAt);
            result = &(destination = source);
        } catch (const std::bad_alloc&) {
            threw = true;
            ++failures;
        }
        verifyMappings(source, handles, sourceEntries);
        if (threw) {
            verifyMappings(destination, handles, {{2, handles[5]}, {3, handles[6]}});
            check(destination.add(handles[7]) == 1, "failed assignment changed recycled ID");
            check(destination.add(handles[8]) == 4, "failed assignment changed next ID");
            check(!destination.add(handles[9]), "failed assignment changed capacity");
            verifyMappings(destination, handles, {{1, handles[7]}, {2, handles[5]},
                                                  {3, handles[6]}, {4, handles[8]}});
        } else {
            check(result == &destination, "assignment must return its destination");
            verifyMappings(destination, handles, sourceEntries);
            check(destination.add(handles[7]) == 2, "successful assignment lost recycled ID");
            check(destination.add(handles[8]) == 5, "successful assignment lost next ID");
            check(destination.add(handles[9]) == 6, "successful assignment lost capacity");
            check(!destination.add(handles[10]), "successful assignment exceeded capacity");
            verifyMappings(destination, handles, {{1, handles[0]}, {2, handles[7]},
                                                  {3, handles[2]}, {4, handles[3]},
                                                  {5, handles[8]}, {6, handles[9]}});
            completed = true;
        }
        verifyMappings(source, handles, sourceEntries);
        if (completed) break;
    }
    check(completed && failures > 0, "allocation failure sweep did not complete");
    std::println("ConnectionManager assignment passed: {} allocation failures preserved state", failures);
}
}

int main() {
    using namespace lightnerx::network;
    try {
        check(networkInit(), "network initialization");
        std::array<HSteamNetConnection, 12> handles{};
        for (std::size_t i = 0; i < handles.size(); i += 2) {
            check(SteamNetworkingSockets()->CreateSocketPair(&handles[i], &handles[i + 1], false, nullptr, nullptr),
                  "socket pair creation");
        }
        assignmentRegression(handles);
        for (const auto handle : handles) {
            check(SteamNetworkingSockets()->CloseConnection(handle, 0, nullptr, false), "connection cleanup");
        }
        networkKill();
        return 0;
    } catch (const std::exception& error) {
        std::println(std::cerr, "{}", error.what());
        networkKill();
        return 1;
    }
}
