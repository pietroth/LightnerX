module;

#include <cstdint>

export module communication.response;

import communication.base_protocol;

export namespace communication {
    using namespace communication;

    constexpr uint8_t APPROVED_RESPONSE = 0;
    constexpr uint8_t DENIED_RESPONSE = 1;
    constexpr uint8_t PARTIALLY_APPROVED_RESPONSE = 2;
    constexpr uint8_t FAILURE_RESPONSE = 3;

    #pragma pack(push, 1)

    struct Response {
        BaseProtocol protocol; // 2 bytes
        uint16_t correlationId; // 2 bytes
        uint8_t status; // 1 byte

        Response(uint16_t size, uint16_t correlationId, uint8_t status)
        :   protocol(size), correlationId(correlationId), status(status) {}
    }; // Approved and Failure share the same header.

    struct UnsuccessfulResponse {
        BaseProtocol protocol; // 2 bytes
        uint16_t correlationId; // 2 bytes
        uint8_t status; // 1 byte
        uint16_t reasonCode; // 2 bytes

        UnsuccessfulResponse(uint16_t size, uint16_t correlationId, uint8_t status, uint16_t reasonCode)
        : protocol(size), correlationId(correlationId), status(status), reasonCode(reasonCode) {}
    }; // Denied and PartiallyApproved share the same header.

    #pragma pack(pop)
}
