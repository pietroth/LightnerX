module;
#include <cstdint>
#include <expected>
#include <system_error>
#include <cstring>
#include "enet.h"

export module network.send;

import communication.event;
import communication.intention;
import communication.response;
import communication.mbt;
import network.client;

namespace network::send {
    std::expected<void, std::error_code> sPacket(ENetPeer* peer, ENetPacket* packet, int channelId) {
        if (peer->state != ENET_PEER_STATE_CONNECTED) {
            return std::unexpected(std::make_error_code(std::errc::not_connected));
        }

        if (packet->dataLength > peer->host->maximumPacketSize) {
            return std::unexpected(std::make_error_code(std::errc::message_size));
        }

        int result = enet_peer_send(peer, channelId, packet);

        if (result < 0) {
            enet_packet_destroy(packet);
            return std::unexpected(std::make_error_code(std::errc::resource_unavailable_try_again));
        }

        return {};
    }

    export template <typename T>
    std::expected<void, std::error_code> event(
        ENetPeer* peer, const communication::MBT& mbt, const T& payload
    ){
        static_assert(std::is_trivially_copyable_v<T>,
            "The payload must be trivially copyable for direct transmission.");

        uint16_t headerSize = sizeof(communication::Event);
        uint16_t payloadSize = sizeof(T);
        uint16_t totalSize = headerSize + payloadSize;

        ENetPacket* packet = enet_packet_create(
            nullptr,
            totalSize,
            ENET_PACKET_FLAG_RELIABLE
        );

        communication::Event header = { {totalSize}, mbt };
        std::memcpy(packet->data, &header, headerSize);
        std::memcpy(packet->data + headerSize, &payload, payloadSize);

        return sPacket(peer, packet, 0);
    }

    export template<typename T>
    std::expected<void, std::error_code> intention(
        ENetPeer* peer, const communication::MBT& mbt, uint16_t correlationId, const T& payload
    ) {
        static_assert(std::is_trivially_copyable_v<T>,
            "The payload must be trivially copyable for direct transmission.");

        uint16_t headerSize = sizeof(communication::Intention);
        uint16_t payloadSize = sizeof(T);
        uint16_t totalSize = headerSize + payloadSize;

        ENetPacket* packet = enet_packet_create(
            nullptr,
            totalSize,
            ENET_PACKET_FLAG_RELIABLE
        );

        communication::Intention header = { {totalSize}, mbt, correlationId };
        std::memcpy(packet->data, &header, headerSize);
        std::memcpy(packet->data + headerSize, &payload, payloadSize);

        return sPacket(peer, packet, 0);
    }

    std::expected<void, std::error_code> response(
        ENetPeer* peer, uint16_t correlationId, uint8_t status
    ) {
        uint16_t size = sizeof(communication::Response);

        ENetPacket* packet = enet_packet_create(
            nullptr,
            size,
            ENET_PACKET_FLAG_RELIABLE
        );

        communication::Response header = { {size}, correlationId, status };
        std::memcpy(packet->data, &header, size);

        return sPacket(peer, packet, 0);
    }

    export std::expected<void, std::error_code> approvedResponse(ENetPeer* peer, uint16_t correlationId){
        return response(peer, correlationId, communication::APPROVED);
    }

    export std::expected<void, std::error_code> failureResponse(ENetPeer* peer, uint16_t correlationId) {
        return response(peer, correlationId, communication::FAILURE);
    }

    export std::expected<void, std::error_code> deniedResponse(
        ENetPeer* peer, uint16_t correlationId, uint16_t reasonCode
    ) {
        uint16_t size = sizeof(communication::UnsuccessfulResponse);

        ENetPacket* packet = enet_packet_create(
            nullptr,
            size,
            ENET_PACKET_FLAG_RELIABLE
        );

        communication::UnsuccessfulResponse header = { {size}, correlationId, reasonCode, communication::DENIED };
        std::memcpy(packet->data, &header, size);

        return sPacket(peer, packet, 0);
    }

    export template<typename T>
    std::expected<void, std::error_code> partiallyApprovedResponse(
        ENetPeer* peer, uint16_t correlationId, uint16_t reasonCode, const T& payload
    ) {
        uint16_t headerSize = sizeof(communication::UnsuccessfulResponse);
        uint16_t payloadSize = sizeof(T);
        uint16_t totalSize = headerSize + payloadSize;

        ENetPacket* packet = enet_packet_create(
            nullptr,
            totalSize,
            ENET_PACKET_FLAG_RELIABLE
        );

        communication::UnsuccessfulResponse header =
            { {totalSize}, correlationId, reasonCode, communication::PARTIALLY_APPROVED };
        std::memcpy(packet->data, &header, headerSize);
        std::memcpy(packet->data + headerSize, &payload, payloadSize);

        return sPacket(peer, packet, 0);
    }
}
