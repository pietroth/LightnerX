#include <cstdint>
#include <functional>
#include <thread>

import network.server;
import network.client;
import communication.event;
import network.send;

void server_run(network::Server& server) {
    server.run();
}

int main() {
    network::Server server{49823, 16, 2};
    network::Client client{2, 3000};

    std::thread serverThread(server_run, std::ref(server));

    client.connect("localhost", 49823);

    #pragma pack(push, 0)
    struct PayloadExample {
        uint16_t id;
    };
    #pragma pack(pop)

    PayloadExample payloadExample;
    payloadExample.id = 1;

    network::send::event(client.getPeer(), 16, payloadExample);
    while (true) {
        client.update();
    }

    serverThread.join();

    return 0;
}
