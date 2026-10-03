export module communication.notification;

import communication.base_protocol;
import communication.mbt;

export namespace communication {
    using namespace communication;

    #pragma pack(push, 1)
    struct Notification {
        BaseProtocol protocol; // 2 bytes
        MBT mbt; // 2 bytes
    };
    #pragma pack(pop)
}
