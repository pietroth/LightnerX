#ifndef LIGHTNERX_COMMUNICATION_BASE_PROTOCOL_HPP
#define LIGHTNERX_COMMUNICATION_BASE_PROTOCOL_HPP


#include <cstdint>


namespace lightnerx::communication {
    #pragma pack(push, 1)
    struct BaseProtocol {
        std::uint16_t size;

        BaseProtocol(std::uint16_t size);
    };
    #pragma pack(pop)
}
#endif
