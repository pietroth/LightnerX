#include <lightnerx/network/target.hpp>

#include <utility>
#include <variant>
#include <vector>

namespace lightnerx::network {

Target::Target(decltype(connection) conn, decltype(broadcastExceptions) exceptions)
            : connection(std::move(conn)), broadcastExceptions(std::move(exceptions)) {}

Target Target::single(ConnectionHandle conn) {
    return Target(conn);
}

Target Target::group(std::vector<ConnectionHandle> connections) {
    return Target(std::move(connections));
}

Target Target::broadcast(std::vector<ConnectionHandle> exceptions) {
    return Target(std::monostate{}, std::move(exceptions));
}

[[nodiscard]] bool Target::isBroadcast() const noexcept {
    return std::holds_alternative<std::monostate>(connection);
}

[[nodiscard]] bool Target::isGroup() const noexcept {
    return std::holds_alternative<std::vector<ConnectionHandle>>(connection);
}

[[nodiscard]] bool Target::isSingle() const noexcept {
    return std::holds_alternative<ConnectionHandle>(connection);
}

}

