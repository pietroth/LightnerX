module;

#include "steam/steamnetworkingtypes.h"
#include <steam/isteamnetworkingsockets.h>

#include <optional>
#include <vector>
#include <set>
#include <cstdint>

export module network.connection_manager;

import network.network_manager;

export namespace network {
    constexpr int invalidConnection = 0;

    class ConnectionManager {
    private:
        std::vector<HSteamNetConnection> clients;
        int maxCapacity;
        int onlineCount;

        std::set<uint16_t> freeIds;
        uint16_t nextId = 0;

        uint16_t getId() {
            if (!freeIds.empty()) {
                uint16_t reusedId = *freeIds.begin();
                freeIds.erase(freeIds.begin());
                return reusedId;
            }

            return nextId += 1;
        }

        void freeId(uint16_t id) {
            if (id < nextId) {
                freeIds.insert(id);
            }
        }

    public:
        ConnectionManager(int initialReserve, int maxCapacity) {
            clients.reserve(initialReserve);
            this->maxCapacity = maxCapacity;
            onlineCount = 0;
        }

        HSteamNetConnection getHSteamNetConnection(uint16_t id) const noexcept {
            if (id >= clients.size()) {
                return invalidConnection;
            }

            return clients[id];
        }

        std::optional<uint16_t> getClientId(HSteamNetConnection hConn) const noexcept {
            if (!isNetworkInitialized()) {
                return std::nullopt;
            }

            ISteamNetworkingSockets* sockets = SteamNetworkingSockets();

            int64 connectionId = sockets->GetConnectionUserData(hConn);
            if (connectionId < 0 || connectionId > 65535) {
                return false;
            }
            return static_cast<uint16_t>(connectionId);
        }

        std::optional<uint16_t> add(HSteamNetConnection hConn) {
            if (!isNetworkInitialized()) {
                return {};
            }

            if (onlineCount >= maxCapacity) {
                return {};
            }

            ISteamNetworkingSockets* sockets = SteamNetworkingSockets();

            uint16_t id = getId();

            if (!sockets->SetConnectionUserData(hConn, id)) {
                freeId(id);
                return std::nullopt;
            }

            if (id >= clients.size()) {
                clients.resize(id + 16);
            }

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

            if (isNetworkInitialized()) {
                SteamNetworkingSockets()->SetConnectionUserData(hConn, -1);
            }

            clients[id] = invalidConnection;
            freeId(id);
            onlineCount--;
        }

        int getOnlineCount() const noexcept {
            return onlineCount;
        }

        void forEach(const auto& callback) const noexcept {
            for (uint16_t id = 0; id < clients.size(); ++id) {
                HSteamNetConnection hConn = clients[id];

                if (hConn != invalidConnection) {
                    callback(id, hConn);
                }
            }
        }
    };
}
