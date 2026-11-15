#include <lightnerx/communication/response.hpp>

#include <cstdint>

namespace lightnerx::communication {

Response::Response(std::uint16_t size, std::uint16_t correlationId, std::uint8_t status)
        :   protocol(size), correlationId(correlationId), status(status) {}

UnsuccessfulResponse::UnsuccessfulResponse(std::uint16_t size, std::uint16_t correlationId, std::uint8_t status, std::uint16_t reasonCode)
        : protocol(size), correlationId(correlationId), status(status), reasonCode(reasonCode) {}

}

