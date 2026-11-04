module;

#include <unordered_set>
#include <cstdint>

export module communication.notification;

import communication.base_protocol;
import communication.mbt;

export namespace communication {
    using namespace communication;

    #pragma pack(push, 1)
    struct Notification {
        BaseProtocol protocol; // 2 bytes
        MBT mbt; // 2 bytes

        Notification(uint16_t size, uint16_t type)
            : protocol(size), mbt(type) {}
    };
    #pragma pack(pop)

    class NotificationRegistry {
    private:
        std::unordered_set<uint16_t> ids;

    public:
        void registerId(uint16_t id) {
            ids.insert(id);
        }

        void registerIds(std::unordered_set<uint16_t>&& newIds) {
            ids.merge(newIds);
        }

        bool exist(uint16_t id) const noexcept {
            return ids.contains(id);
        }
    };
}
