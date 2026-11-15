#ifndef LIGHTNERX_COMMUNICATION_MBT_HPP
#define LIGHTNERX_COMMUNICATION_MBT_HPP


#include <cstdint>


namespace lightnerx::communication {
    #pragma pack(push, 1)
    struct MBT {
        std::uint16_t type;

        MBT(std::uint16_t type);
    };
    #pragma pack(pop)
}
#endif
