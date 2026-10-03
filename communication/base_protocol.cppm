module;
#include <cstdint>

export module communication.base_protocol;

export namespace communication {
    #pragma pack(push, 1)
    struct BaseProtocol {
        uint16_t size;

        BaseProtocol(uint16_t size) : size(size) {}
    };
    #pragma pack(pop)
}
