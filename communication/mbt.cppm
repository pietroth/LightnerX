module;
export module lightnerx.communication.mbt;

import std;

export namespace lightnerx::communication {
    #pragma pack(push, 1)
    struct MBT {
        std::uint16_t type;

        MBT(std::uint16_t type) : type(type) {}
    };
    #pragma pack(pop)
}
