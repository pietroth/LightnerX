module;

export module lightnerx.communication.notification;

import std;

import lightnerx.communication.base_protocol;
import lightnerx.communication.mbt;

export namespace lightnerx::communication {
    using namespace lightnerx::communication;

    #pragma pack(push, 1)
    struct Notification {
        BaseProtocol protocol; // 2 bytes
        MBT mbt; // 2 bytes

        Notification(std::uint16_t size, std::uint16_t type)
            : protocol(size), mbt(type) {}
    };
    #pragma pack(pop)

    class NotificationRegistry {
    private:
        std::unordered_set<std::uint16_t> ids;

    public:
        void registerId(std::uint16_t id) {
            ids.insert(id);
        }

        void registerIds(std::unordered_set<std::uint16_t>&& newIds) {
            ids.merge(newIds);
        }

        bool exist(std::uint16_t id) const noexcept {
            return ids.contains(id);
        }
    };
}
