module;

export module communication.base_protocol;

import std;

export namespace communication {
    #pragma pack(push, 1)
    struct BaseProtocol {
        std::uint16_t size;

        BaseProtocol(std::uint16_t size) : size(size) {}
    };
    #pragma pack(pop)
}
