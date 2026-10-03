module;
#include <set>
#include "enet.h"
#include <cstdint>
#include <unordered_map>

export module network.client_manager;

export namespace network {
    class ClientManager {
    private:
        std::unordered_map<uint16_t, ENetPeer*> clients;
        std::set<uint16_t> freeIds;
        uint16_t nextId = 0;

        uint16_t getId() {
            if (!freeIds.empty()) {
                uint16_t reusedId = *freeIds.begin();
                freeIds.erase(freeIds.begin());
                return reusedId;
            }

            return nextId += 1;
        }

        void freeId(uint16_t id) {
            if (id < nextId) {
                freeIds.insert(id);
            }
        }

    public:
        ENetPeer* getPeer(uint16_t clientId) {
            auto it = clients.find(clientId);

            if (it != clients.end()) {
                return it->second;
            }

            return nullptr;
        }

        void onClientConnected(ENetPeer* peer) {
            if (!peer) return;

            int id = getId();
            clients.emplace(id, peer);
        }
    };
}
