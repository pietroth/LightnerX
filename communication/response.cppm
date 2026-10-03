module;
#include <cstdint>

export module communication.response;

import communication.base_protocol;

export namespace communication {
    using namespace communication;

    constexpr uint8_t APPROVED = 0;
    constexpr uint8_t DENIED = 1;
    constexpr uint8_t PARTIALLY_APPROVED = 2;
    constexpr uint8_t FAILURE = 3;

    #pragma pack(push, 1)

    struct Response {
        BaseProtocol protocol;
        uint16_t correlationId;
        uint8_t status;

        Response(BaseProtocol protocol, uint16_t correlationId, uint8_t status)
        :   protocol(protocol), correlationId(correlationId), status(status) {}
    }; // ApprovedResponse and FailedResponse have the same header.

    struct UnsuccessfulResponse {
        Response base;
        uint16_t reasonCode;

        UnsuccessfulResponse(BaseProtocol protocol, uint16_t correlationId, uint16_t reasonCode, uint8_t status)
        :   base(protocol, correlationId, status), reasonCode(reasonCode) {}
    }; // DeniedResponse and FailedResponse share the same header.

    #pragma pack(pop)
}
