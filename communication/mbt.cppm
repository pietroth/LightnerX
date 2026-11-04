module;

#include <cstdint>

export module communication.mbt;

export namespace communication {
    #pragma pack(push, 1)
    struct MBT {
        uint16_t type;

        MBT(uint16_t type) : type(type) {}
    };
    #pragma pack(pop)
}
