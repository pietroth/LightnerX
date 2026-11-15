#include <lightnerx/communication/notification.hpp>

#include <cstdint>
#include <unordered_set>

namespace lightnerx::communication {

Notification::Notification(std::uint16_t size, std::uint16_t type)
            : protocol(size), mbt(type) {}

void NotificationRegistry::registerId(std::uint16_t id) {
    ids.insert(id);
}

void NotificationRegistry::registerIds(std::unordered_set<std::uint16_t>&& newIds) {
    ids.merge(newIds);
}

bool NotificationRegistry::exist(std::uint16_t id) const noexcept {
    return ids.contains(id);
}

}

