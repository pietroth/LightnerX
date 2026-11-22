#ifndef LIGHTNERX_NETWORK_CONNECTION_MANAGER_HPP
#define LIGHTNERX_NETWORK_CONNECTION_MANAGER_HPP

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <lightnerx/network/connection_handle.hpp>
#include <lightnerx/network/network_manager.hpp>

namespace lightnerx::network {
    inline constexpr int invalidConnection = 0;

    class ConnectionManager {
    private:
        std::vector<ConnectionHandle> clients;
        std::unordered_map<ConnectionHandle, std::uint16_t> clientIds;
        int maxCapacity = 0;
        int onlineCount = 0;

        std::set<std::uint16_t> freeIds;
        std::uint32_t nextId = 1;
        static constexpr std::uint32_t maxId = std::numeric_limits<std::uint16_t>::max();

    public:
        // Capacities must be nonnegative.
        ConnectionManager(int initialReserve, int maxCapacity);

        ConnectionManager(const ConnectionManager&) = delete;
        ConnectionManager& operator=(const ConnectionManager&) = delete;
        ConnectionManager(ConnectionManager&&) = delete;
        ConnectionManager& operator=(ConnectionManager&&) = delete;

        ConnectionHandle getHSteamNetConnection(std::uint16_t id) const noexcept;

        std::optional<std::uint16_t> getClientId(ConnectionHandle hConn) const noexcept;

        std::optional<std::uint16_t> add(ConnectionHandle hConn);

        void remove(int id);

        int getOnlineCount() const noexcept;

        template<class Callback>
        void forEach(const Callback& callback) const {
            // Reload by index after each callback: add/remove may reallocate clients.
            // The entry-time bound prevents additions from extending this traversal.
            const std::size_t limit = clients.size();
            for (std::size_t id = 1; id < limit && id < clients.size(); ++id) {
                ConnectionHandle hConn = clients[id];

                if (hConn != invalidConnection) {
                    callback(static_cast<std::uint16_t>(id), hConn);
                }
            }
        }
    };
}
#endif
