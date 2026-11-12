#include <steam/steamnetworkingsockets.h>

import std;
import lightnerx.network.client;
import lightnerx.network.transport;

static_assert(lightnerx::network::k_invalidConnection == k_HSteamNetConnection_Invalid);
static_assert(!std::is_copy_constructible_v<lightnerx::network::Client>);

int main() {
    lightnerx::network::Client client;
    return client.getServerHConn() == lightnerx::network::k_invalidConnection ? 0 : 1;
}
