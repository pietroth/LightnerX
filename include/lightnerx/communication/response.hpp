#ifndef LIGHTNERX_COMMUNICATION_RESPONSE_HPP
#define LIGHTNERX_COMMUNICATION_RESPONSE_HPP


#include <cstdint>

#include <lightnerx/communication/base_protocol.hpp>

namespace lightnerx::communication {
    using namespace lightnerx::communication;

    inline constexpr std::uint8_t APPROVED_RESPONSE = 0;
    inline constexpr std::uint8_t DENIED_RESPONSE = 1;
    inline constexpr std::uint8_t PARTIALLY_APPROVED_RESPONSE = 2;
    inline constexpr std::uint8_t FAILURE_RESPONSE = 3;

    #pragma pack(push, 1)

    struct Response {
        BaseProtocol protocol; // 2 bytes
        std::uint16_t correlationId; // 2 bytes
        std::uint8_t status; // 1 byte

        Response(std::uint16_t size, std::uint16_t correlationId, std::uint8_t status);
    }; // Approved and Failure share the same header.

    struct UnsuccessfulResponse {
        BaseProtocol protocol; // 2 bytes
        std::uint16_t correlationId; // 2 bytes
        std::uint8_t status; // 1 byte
        std::uint16_t reasonCode; // 2 bytes

        UnsuccessfulResponse(std::uint16_t size, std::uint16_t correlationId, std::uint8_t status, std::uint16_t reasonCode);
    }; // Denied and PartiallyApproved share the same header.

    #pragma pack(pop)
}
#endif
