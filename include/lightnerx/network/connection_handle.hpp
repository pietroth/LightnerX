#ifndef LIGHTNERX_NETWORK_CONNECTION_HANDLE_HPP
#define LIGHTNERX_NETWORK_CONNECTION_HANDLE_HPP

#include <cstdint>

namespace lightnerx::network {
    using ConnectionHandle = std::uint32_t;

    inline constexpr ConnectionHandle k_invalidConnection = 0;
}

#endif
