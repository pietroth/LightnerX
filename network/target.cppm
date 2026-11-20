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
        Target(const Target&);
        Target(Target&&) noexcept;
        Target& operator=(const Target&);
        Target& operator=(Target&&) noexcept;

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

        [[nodiscard]] bool isGroup() const noexcept {
            return std::holds_alternative<std::vector<HSteamNetConnection>>(connection);
        }

        [[nodiscard]] bool isSingle() const noexcept {
            return std::holds_alternative<HSteamNetConnection>(connection);
        }
    };

    // Provide definitions in the library even when all its own calls are inlined.
    Target::Target(const Target&) = default;
    Target::Target(Target&&) noexcept = default;
    Target& Target::operator=(const Target&) = default;
    Target& Target::operator=(Target&&) noexcept = default;
}
