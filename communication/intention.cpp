#include <lightnerx/communication/intention.hpp>

#include <cstdint>

namespace lightnerx::communication {

Intention::Intention(std::uint16_t size, std::uint16_t type, std::uint16_t correlationId)
            : protocol(size), mbt(type), correlationId(correlationId) {}

}

