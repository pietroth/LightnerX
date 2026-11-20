#include <lightnerx/network/connection_manager.hpp>

#include <steam/steamnetworkingsockets.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <utility>

namespace lightnerx::network {

ConnectionManager::ConnectionManager(int initialReserve, int maxCapacity) {
    if (initialReserve < 0 || maxCapacity < 0) {
        std::abort();
    }
    this->maxCapacity = std::min(maxCapacity, static_cast<int>(maxId));
    clients.reserve(std::min(initialReserve, this->maxCapacity) + 1);
}

ConnectionManager::ConnectionManager(ConnectionManager&& other) noexcept(
    std::is_nothrow_default_constructible_v<decltype(clients)> &&
    std::is_nothrow_default_constructible_v<decltype(clientIds)> &&
    std::is_nothrow_default_constructible_v<decltype(freeIds)>) {
    *this = std::move(other);
}

ConnectionManager& ConnectionManager::operator=(ConnectionManager&& other) noexcept {
    if (this != &other) {
        clients = std::move(other.clients);
        clientIds = std::move(other.clientIds);
        maxCapacity = other.maxCapacity;
        onlineCount = std::exchange(other.onlineCount, 0);
        freeIds = std::move(other.freeIds);
        nextId = std::exchange(other.nextId, 1);
        other.clients.clear();
        other.clientIds.clear();
        other.freeIds.clear();
    }
    return *this;
}

ConnectionManager& ConnectionManager::operator=(const ConnectionManager& other) {
    if (this != &other) {
        // Copy every allocating member before committing with noexcept move assignment.
        ConnectionManager replacement(other);
        *this = std::move(replacement);
    }
    return *this;
}

ConnectionHandle ConnectionManager::getHSteamNetConnection(std::uint16_t id) const noexcept {
    if (id >= clients.size()) {
        return invalidConnection;
    }

    return clients[id];
}

std::optional<std::uint16_t> ConnectionManager::getClientId(ConnectionHandle hConn) const noexcept {
    const auto entry = clientIds.find(hConn);
    if (entry == clientIds.end()) return std::nullopt;
    return entry->second;
}

std::optional<std::uint16_t> ConnectionManager::add(ConnectionHandle hConn) {
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
    const std::uint16_t id = freeIds.empty() ? static_cast<std::uint16_t>(nextId) : *freeIds.begin();

    // Complete all allocating operations before committing the new ID.
    if (id >= clients.size()) {
        clients.resize(std::min(static_cast<std::size_t>(id) + 16, static_cast<std::size_t>(maxId) + 1));
    }
    clientIds.emplace(hConn, id);
    if (freeIds.empty()) ++nextId;
    else freeIds.erase(freeIds.begin());

    clients[id] = hConn;
    onlineCount++;

    return id;
}

void ConnectionManager::remove(int id) {
    if (id <= 0 || static_cast<std::size_t>(id) >= clients.size()) {
        return;
    }

    ConnectionHandle hConn = clients[id];

    if (hConn == invalidConnection) {
        return;
    }

    // Insertion may allocate. Leave the registration intact on failure.
    freeIds.insert(static_cast<std::uint16_t>(id));
    clientIds.erase(hConn);
    clients[id] = invalidConnection;
    onlineCount--;
}

int ConnectionManager::getOnlineCount() const noexcept {
    return onlineCount;
}

}

