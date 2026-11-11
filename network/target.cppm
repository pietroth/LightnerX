module;

#include "steam/steamnetworkingtypes.h"

export module lightnerx.network.target;

import std;

export namespace lightnerx::network {
    struct Target {
        std::variant<std::monostate, std::vector<HSteamNetConnection>, HSteamNetConnection> connection;
        std::vector<HSteamNetConnection> broadcastExceptions;

        private:
        Target(decltype(connection) conn, decltype(broadcastExceptions) exceptions = {})
            : connection(std::move(conn)), broadcastExceptions(std::move(exceptions)) {}

        public:
        static Target single(HSteamNetConnection conn) {
            return Target(conn);
        }

        static Target group(std::vector<HSteamNetConnection> connections) {
            return Target(std::move(connections));
        }

        static Target broadcast(std::vector<HSteamNetConnection> exceptions = {}) {
            return Target(std::monostate{}, std::move(exceptions));
        }

        [[nodiscard]] bool isBroadcast() const noexcept {
            return std::holds_alternative<std::monostate>(connection);
        }

        [[nodiscard]] bool isSingle() const noexcept {
            return std::holds_alternative<HSteamNetConnection>(connection);
        }
    };
}
