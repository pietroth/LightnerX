module;
#define ENET_IMPLEMENTATION
#include <functional>
#include <vector>
#include <cstddef>
#include <expected>
#include <system_error>
#include <cstdint>
#include "enet.h"
#include <iostream>
#include <print>

export module network.server;

export namespace network {
    class Server {
    private:
        int port;
        int peerCount;
        int channelLimit;

        using ClientConnectedCallback = std::function<void()>;
        std::vector<ClientConnectedCallback> clientConnectedCallbacks;

    public:
        Server(int port, int peerCount, int channelLimit)
        :   port(port), peerCount(peerCount), channelLimit(channelLimit) {}

        void addClientConnectListener(ClientConnectedCallback callback) {
            clientConnectedCallbacks.push_back(callback);
        }

        std::expected<void, std::error_code> run() {
            if (enet_initialize() != 0) {
                std::println(std::cerr, "An error occurred while initializing ENet.");
                return std::unexpected(std::make_error_code(std::errc::io_error));
            }

            ENetAddress address;
            address.host = ENET_HOST_ANY;
            address.port = port;

            ENetHost* server = enet_host_create(&address, peerCount, channelLimit, 0, 0);
            if (server == nullptr) {
                std::println(std::cerr, "Error creating server.");
                return std::unexpected(std::make_error_code(std::errc::resource_unavailable_try_again));
            }

            std::println("Server started on port {}", address.port);

            ENetEvent event;

            while (true) {
                while (enet_host_service(server, &event, 15)) {
                    switch (event.type) {
                        case ENET_EVENT_TYPE_CONNECT: {
                            auto raw_rost = *reinterpret_cast<const uint32_t*>(&event.peer->address.host);
                            std::println("Client connected from {:x}:{}", raw_rost, event.peer->address.port);

                            for (const auto& callback : clientConnectedCallbacks) {
                                if (callback) {
                                    callback();
                                }
                            }

                            break;
                        }
                        case ENET_EVENT_TYPE_RECEIVE: {
                            std::print("Message received. Lenght: {}. Data:", event.packet->dataLength);

                            for (size_t i = 0; i < event.packet->dataLength; i++) {
                                std::print(" {:02X}", static_cast<unsigned>(event.packet->data[i]));
                            }

                            std::println(". Channel: {}", event.channelID);

                            enet_packet_destroy(event.packet);
                            break;
                        }
                        case ENET_EVENT_TYPE_DISCONNECT: {
                            std::println("{} disconnected.", event.peer->data);
                            break;
                        }
                        case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT: {
                            std::println("{} disconnected due to timeout.", event.peer->data);
                            break;
                        }
                        case ENET_EVENT_TYPE_NONE:
                            break;
                    }
                }
            }

            enet_host_destroy(server);
            enet_deinitialize();
        }
    };
}
