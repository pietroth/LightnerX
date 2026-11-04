module;
#include <cstdint>

export module communication.intention;

import communication.base_protocol;
import communication.mbt;

export namespace communication {
    #pragma pack(push, 1)
    struct Intention {
        BaseProtocol protocol; // 2 bytes
        MBT mbt; // 2 bytes;
        uint16_t correlationId;

        Intention(uint16_t size, uint16_t type, uint16_t correlationId)
            : protocol(size), mbt(type), correlationId(correlationId) {}
    };
    #pragma pack(pop)
}
