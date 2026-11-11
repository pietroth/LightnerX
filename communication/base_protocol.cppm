module;

export module lightnerx.communication.base_protocol;

import std;

export namespace lightnerx::communication {
    #pragma pack(push, 1)
    struct BaseProtocol {
        std::uint16_t size;

        BaseProtocol(std::uint16_t size) : size(size) {}
    };
    #pragma pack(pop)
}
