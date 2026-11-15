#include <lightnerx/communication/base_protocol.hpp>

#include <cstdint>

namespace lightnerx::communication {

BaseProtocol::BaseProtocol(std::uint16_t size) : size(size) {}

}

