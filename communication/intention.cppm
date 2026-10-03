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
    };
    #pragma pack(pop)
}
