#include <lightnerx/communication/event.hpp>

#include <cstdint>

namespace lightnerx::communication {

Event::Event(std::uint16_t size, std::uint16_t typeId) : protocol(size), mbt(typeId) {}

}

