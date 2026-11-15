#ifndef LIGHTNERX_COMMUNICATION_NOTIFICATION_HPP
#define LIGHTNERX_COMMUNICATION_NOTIFICATION_HPP


#include <cstdint>
#include <unordered_set>

#include <lightnerx/communication/base_protocol.hpp>
#include <lightnerx/communication/mbt.hpp>

namespace lightnerx::communication {
    using namespace lightnerx::communication;

    #pragma pack(push, 1)
    struct Notification {
        BaseProtocol protocol; // 2 bytes
        MBT mbt; // 2 bytes

        Notification(std::uint16_t size, std::uint16_t type);
    };
    #pragma pack(pop)

    class NotificationRegistry {
    private:
        std::unordered_set<std::uint16_t> ids;

    public:
        void registerId(std::uint16_t id);

        void registerIds(std::unordered_set<std::uint16_t>&& newIds);

        bool exist(std::uint16_t id) const noexcept;
    };
}
#endif
