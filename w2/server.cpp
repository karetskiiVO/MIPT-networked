#include <enet/enet.h>

#include <map>
#include <set>
#include <string>
#include <iostream>

#include "output.h"
#include "player.h"

bool operator< (const ENetAddress& rhv, const ENetAddress& lhv) {
    if (lhv.host == rhv.host) return lhv.port < rhv.port;
    return lhv.host < rhv.host;
}

template <>
void print<ENetAddress> (const ENetAddress& value) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value.host);
    std::cout << unsigned(bytes[0]) << '.' 
              << unsigned(bytes[1]) << '.' 
              << unsigned(bytes[2]) << '.'
              << unsigned(bytes[3]) << ':' 
              << value.port;
}

class GameServer {
    std::map<ENetAddress, Player> players;

    ENetHost* server = nullptr;
public:
    GameServer (uint16_t port) {
        ENetAddress address;

        address.host = ENET_HOST_ANY;
        address.port = port;

        server = enet_host_create(&address, 32, ENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT, 0, 0);

        if (server == nullptr) throw std::runtime_error("Cannot create ENet server");
    }

    void Poll () {
        std::map<ENetAddress, bool> updatedAddr;

        ENetEvent event;
        while (enet_host_service(server, &event, 10) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT: 
                    if (!players.contains(event.peer->address)) {

                    auto newPlayerName = "player#" + std::to_string(players.size());
                    
                    println("registered", event.peer->address, "as", newPlayerName);
                    
                    players[event.peer->address] = Player{
                        .id = newPlayerName,
                        .x = 150.f,
                        .y = 150.f,
                        .ping = event.peer->pingInterval,
                    };

                    auto playerList = std::string("l");
                    for (const auto& [_, player] : players) {
                        playerList += " ";
                        playerList += player.id;
                    }

                    ENetPacket* packet = enet_packet_create(playerList.c_str(), playerList.size() + 1, ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(event.peer, 0, packet);

                    updatedAddr[event.peer->address] = true;
                    }   
                    break;
                case ENET_EVENT_TYPE_RECEIVE:
                    println("from", event.peer->address, "recieved", event.packet->data);
                    
                    if (players.contains(event.peer->address)) {
                        players[event.peer->address].UpdateFromString(reinterpret_cast<const char*>(event.packet->data));
                        players[event.peer->address].ping = event.peer->roundTripTime;
                        
                        if (!updatedAddr.contains(event.peer->address)) updatedAddr[event.peer->address] = false;
                    }

                    break;
                default:
                    break;
            };
        }

        auto nonReliableUpdate = std::string("u");

        for (auto [address, reliable] : updatedAddr) {
            nonReliableUpdate += " ";
            nonReliableUpdate += players[address].String("-");
        }
        
        if (nonReliableUpdate.length() > 1) {
            for (size_t i = 0; i < players.size(); i++) {
                ENetPacket* packet = enet_packet_create(nonReliableUpdate.c_str(), nonReliableUpdate.size() + 1, ENET_PACKET_FLAG_UNSEQUENCED);
                enet_peer_send(&(server->peers[i]), 0, packet);
            }
        }

        
    }
};

int main () {
    try {
        if (enet_initialize()) throw std::runtime_error("Cannot init ENet");
        
        auto server = GameServer(10888);
        
        while (true) {
            server.Poll();
        }
    } catch (std::exception& e) {
        std::cout << e.what();
    }
    
    atexit(enet_deinitialize);
    return 0;
}

