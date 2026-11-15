#ifndef LIGHTNERX_COMMUNICATION_INTENTION_HPP
#define LIGHTNERX_COMMUNICATION_INTENTION_HPP


#include <cstdint>

#include <lightnerx/communication/base_protocol.hpp>
#include <lightnerx/communication/mbt.hpp>

namespace lightnerx::communication {
    #pragma pack(push, 1)
    struct Intention {
        BaseProtocol protocol; // 2 bytes
        MBT mbt; // 2 bytes;
        std::uint16_t correlationId;

        Intention(std::uint16_t size, std::uint16_t type, std::uint16_t correlationId);
    };
    #pragma pack(pop)
}
#endif
