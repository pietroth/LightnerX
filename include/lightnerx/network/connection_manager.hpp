#ifndef LIGHTNERX_NETWORK_CONNECTION_MANAGER_HPP
#define LIGHTNERX_NETWORK_CONNECTION_MANAGER_HPP

#include "steam/steamnetworkingtypes.h"
#include <steam/isteamnetworkingsockets.h>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

#include <lightnerx/network/network_manager.hpp>

namespace lightnerx::network {
    inline constexpr int invalidConnection = 0;

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
        ConnectionManager(int initialReserve, int maxCapacity);

        ConnectionManager(const ConnectionManager&) = default;
        ConnectionManager(ConnectionManager&&) = default;
        ConnectionManager& operator=(ConnectionManager&&) = default;

        ConnectionManager& operator=(const ConnectionManager& other);

        HSteamNetConnection getHSteamNetConnection(uint16_t id) const noexcept;

        std::optional<uint16_t> getClientId(HSteamNetConnection hConn) const noexcept;

        std::optional<uint16_t> add(HSteamNetConnection hConn);

        void remove(int id);

        int getOnlineCount() const noexcept;

        template<class Callback>
        void forEach(const Callback& callback) const {
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
#endif
