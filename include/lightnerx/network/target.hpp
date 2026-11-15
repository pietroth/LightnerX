#ifndef LIGHTNERX_NETWORK_TARGET_HPP
#define LIGHTNERX_NETWORK_TARGET_HPP

#include "steam/steamnetworkingtypes.h"
#include <variant>
#include <vector>


namespace lightnerx::network {
    struct Target {
        std::variant<std::monostate, std::vector<HSteamNetConnection>, HSteamNetConnection> connection;
        std::vector<HSteamNetConnection> broadcastExceptions;

        private:
        Target(decltype(connection) conn, decltype(broadcastExceptions) exceptions = {});

        public:
        static Target single(HSteamNetConnection conn);

        static Target group(std::vector<HSteamNetConnection> connections);

        static Target broadcast(std::vector<HSteamNetConnection> exceptions = {});

        [[nodiscard]] bool isBroadcast() const noexcept;

        [[nodiscard]] bool isGroup() const noexcept;

        [[nodiscard]] bool isSingle() const noexcept;
    };
}
#endif
