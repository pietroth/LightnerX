module;
#include "enet.h"
#include <system_error>
#include <expected>
#include <print>
#include <system_error>
#include <iostream>

export module network.client;

export namespace network {
    class Client {
    private:
        bool connected = false;
        int channelLimit;
        int timeout;
        ENetPeer* peer = nullptr;
        ENetHost* host = nullptr;

    public:
        Client(int channelLimit, int msTimeout) : channelLimit(channelLimit), timeout(msTimeout) {}

        std::expected<void, std::error_code> connect(const std::string& hostName, int port) {
            if (host != nullptr || connected) {
                return std::unexpected(std::make_error_code(std::errc::already_connected));
            }

            if (enet_initialize() != 0) {
                std::println(std::cerr, "An error occurred while initializing ENet.");
                return std::unexpected(std::make_error_code(std::errc::io_error));
            }

            ENetHost* client;
            client = enet_host_create(
                NULL,
                1,
                channelLimit,
                0,
                0
            );
            this->host = client;

            if (this->host == NULL) {
                std::println(std::cerr, "An error occurred while trying to create an ENet client host.");
                return std::unexpected(std::make_error_code(std::errc::resource_unavailable_try_again));
            }

            ENetAddress address;
            ENetEvent event;

            enet_address_set_host(&address, hostName.c_str());
            address.port = port;
            this->peer = enet_host_connect(client, &address, channelLimit, 0);
            if (this->peer == NULL) {
                std::println(std::cerr, "No available peers for initiating an ENet connection.");
                return std::unexpected(std::make_error_code(std::errc::resource_unavailable_try_again));
            }

            if (enet_host_service(client, &event, timeout) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
                std::println("Connection to {}:{} succeeced.", hostName, port);
                connected = true;
                return {};
            } else {
                enet_peer_reset(peer);
                enet_host_destroy(host);
                host = nullptr;
                peer = nullptr;
                std::println(std::cerr, "Connection to {}:{} failed.", hostName, port);
                return std::unexpected(std::make_error_code(std::errc::network_unreachable));
            }

            return {};
        }

        std::expected<void, std::error_code> disconnect() {
            if (!connected || peer == nullptr) {
                return std::unexpected(std::make_error_code(std::errc::not_connected));
            }

            enet_peer_disconnect(peer, 0);
            return {};
        }

        std::expected<void, std::error_code> update() {
            if (!connected) {
                return std::unexpected(std::make_error_code(std::errc::not_connected));
            }

            ENetEvent event;

            while (enet_host_service(this->host, &event, 0) > 0) {
                switch (event.type) {
                    case ENET_EVENT_TYPE_CONNECT:
                        std::println("Sucessfully connected.");
                        this->connected = true;
                        break;

                    case ENET_EVENT_TYPE_RECEIVE:
                        std::println("Message received.");
                        enet_packet_destroy(event.packet);
                        break;

                    case ENET_EVENT_TYPE_DISCONNECT:
                        std::println("Disconnected.");
                        this->connected = false;
                        break;

                    case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
                        std::println("Disconnected due to timeout.");
                        this->connected = false;
                        break;

                    case ENET_EVENT_TYPE_NONE:
                        break;
                }
            }

            return {};
        }

        ENetPeer* getPeer() {
            return peer;
        }
    };
}
