#include <lightnerx/network/connection_manager.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>

namespace lightnerx::network {

ConnectionManager::ConnectionManager(int initialReserve, int maxCapacity) {
    if (initialReserve < 0 || maxCapacity < 0) {
        throw std::invalid_argument("ConnectionManager capacities must be nonnegative");
    }
    this->maxCapacity = std::min(maxCapacity, static_cast<int>(maxId));
    clients.reserve(std::min(initialReserve, this->maxCapacity) + 1);
    onlineCount = 0;
}

ConnectionManager& ConnectionManager::operator=(const ConnectionManager& other) {
    if (this != &other) {
        // Copy every allocating member before committing with noexcept move assignment.
        ConnectionManager replacement(other);
        *this = std::move(replacement);
    }
    return *this;
}

HSteamNetConnection ConnectionManager::getHSteamNetConnection(uint16_t id) const noexcept {
    if (id >= clients.size()) {
        return invalidConnection;
    }

    return clients[id];
}

std::optional<uint16_t> ConnectionManager::getClientId(HSteamNetConnection hConn) const noexcept {
    const auto entry = clientIds.find(hConn);
    if (entry == clientIds.end()) return std::nullopt;
    return entry->second;
}

std::optional<uint16_t> ConnectionManager::add(HSteamNetConnection hConn) {
    if (!isNetworkInitialized()) {
        return {};
    }

    if (onlineCount >= maxCapacity || hConn == invalidConnection || clientIds.find(hConn) != clientIds.end()) {
        return {};
    }

    ISteamNetworkingSockets* sockets = SteamNetworkingSockets();
    SteamNetConnectionInfo_t info;
    if (!sockets || !sockets->GetConnectionInfo(hConn, &info)) {
        return std::nullopt;
    }

    if (freeIds.empty() && nextId > maxId) return std::nullopt;
    const uint16_t id = freeIds.empty() ? static_cast<uint16_t>(nextId) : *freeIds.begin();

    // Complete all allocating operations before committing the new ID.
    if (id >= clients.size()) {
        clients.resize(std::min(static_cast<size_t>(id) + 16, static_cast<size_t>(maxId) + 1));
    }
    clientIds.emplace(hConn, id);
    if (freeIds.empty()) ++nextId;
    else freeIds.erase(freeIds.begin());

    clients[id] = hConn;
    onlineCount++;

    return id;
}

void ConnectionManager::remove(int id) {
    if (id <= 0 || id >= clients.size()) {
        return;
    }

    HSteamNetConnection hConn = clients[id];

    if (hConn == invalidConnection) {
        return;
    }

    // Insertion may allocate. Leave the registration intact on failure.
    freeIds.insert(static_cast<uint16_t>(id));
    clientIds.erase(hConn);
    clients[id] = invalidConnection;
    onlineCount--;
}

int ConnectionManager::getOnlineCount() const noexcept {
    return onlineCount;
}

}

