#ifndef LIGHTNERX_NETWORK_TARGET_HPP
#define LIGHTNERX_NETWORK_TARGET_HPP

#include <variant>
#include <vector>

#include <lightnerx/network/connection_handle.hpp>

namespace lightnerx::network {
    struct Target {
        std::variant<std::monostate, std::vector<ConnectionHandle>, ConnectionHandle> connection;
        std::vector<ConnectionHandle> broadcastExceptions;

        private:
        Target(decltype(connection) conn, decltype(broadcastExceptions) exceptions = {});

        public:
        static Target single(ConnectionHandle conn);

        static Target group(std::vector<ConnectionHandle> connections);

        static Target broadcast(std::vector<ConnectionHandle> exceptions = {});

        [[nodiscard]] bool isBroadcast() const noexcept;

        [[nodiscard]] bool isGroup() const noexcept;

        [[nodiscard]] bool isSingle() const noexcept;
    };
}
#endif
