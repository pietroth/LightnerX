#ifndef LIGHTNERX_COMMUNICATION_EVENT_HPP
#define LIGHTNERX_COMMUNICATION_EVENT_HPP


#include <cstdint>

#include <lightnerx/communication/base_protocol.hpp>
#include <lightnerx/communication/mbt.hpp>
#include <lightnerx/network/client.hpp>

namespace lightnerx::communication {
    using namespace lightnerx::communication;
    using namespace lightnerx::network;

    #pragma pack(push, 1)
    struct Event {
        BaseProtocol protocol; // 2 bytes
        MBT mbt; // 2 bytes

        Event(std::uint16_t size, std::uint16_t typeId);
    };
    #pragma pack(pop)
}
#endif
