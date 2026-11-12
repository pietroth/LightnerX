module;

#include "steam/steamnetworkingtypes.h"
#include <steam/isteamnetworkingsockets.h>

export module lightnerx.network.connection_manager;

import std;

import lightnerx.network.network_manager;

export namespace lightnerx::network {
    constexpr int invalidConnection = 0;

    class ConnectionManager {
    private:
        std::vector<HSteamNetConnection> clients;
        std::unordered_map<HSteamNetConnection, uint16_t> clientIds;
        int maxCapacity;
        int onlineCount;

        std::set<uint16_t> freeIds;
        uint32_t nextId = 1;
        static constexpr uint32_t maxId = std::numeric_limits<uint16_t>::max();

    public:
        ConnectionManager(int initialReserve, int maxCapacity) {
            if (initialReserve < 0 || maxCapacity < 0) {
                throw std::invalid_argument("ConnectionManager capacities must be nonnegative");
            }
            this->maxCapacity = std::min(maxCapacity, static_cast<int>(maxId));
            clients.reserve(std::min(initialReserve, this->maxCapacity) + 1);
            onlineCount = 0;
        }

        ConnectionManager(const ConnectionManager&) = default;
        ConnectionManager(ConnectionManager&&) = default;
        ConnectionManager& operator=(ConnectionManager&&) = default;

        ConnectionManager& operator=(const ConnectionManager& other) {
            if (this != &other) {
                // Copy every allocating member before committing with noexcept move assignment.
                ConnectionManager replacement(other);
                *this = std::move(replacement);
            }
            return *this;
        }

        HSteamNetConnection getHSteamNetConnection(uint16_t id) const noexcept {
            if (id >= clients.size()) {
                return invalidConnection;
            }

            return clients[id];
        }

        std::optional<uint16_t> getClientId(HSteamNetConnection hConn) const noexcept {
            const auto entry = clientIds.find(hConn);
            if (entry == clientIds.end()) return std::nullopt;
            return entry->second;
        }

        std::optional<uint16_t> add(HSteamNetConnection hConn) {
            if (!isNetworkInitialized()) {
                return {};
            }

            if (onlineCount >= maxCapacity || hConn == invalidConnection || clientIds.contains(hConn)) {
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

        void remove(int id) {
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

        int getOnlineCount() const noexcept {
            return onlineCount;
        }

        void forEach(const auto& callback) const {
            // Reload by index after each callback: add/remove may reallocate clients.
            // The entry-time bound prevents additions from extending this traversal.
            const size_t limit = clients.size();
            for (size_t id = 1; id < limit && id < clients.size(); ++id) {
                HSteamNetConnection hConn = clients[id];

                if (hConn != invalidConnection) {
                    callback(static_cast<uint16_t>(id), hConn);
                }
            }
        }
    };
}
