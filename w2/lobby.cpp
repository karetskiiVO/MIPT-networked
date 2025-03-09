#include <enet/enet.h>

#include <map>
#include <string>
#include <iostream>

#include "output.h"

template <>
void print<ENetAddress> (const ENetAddress& value) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value.host);
    std::cout << unsigned(bytes[0]) << '.' 
              << unsigned(bytes[1]) << '.' 
              << unsigned(bytes[2]) << '.'
              << unsigned(bytes[3]) << ':' 
              << value.port;
}

class LobbyServer {
    ENetHost* server = nullptr;

    ENetAddress playServer;
    bool started = false;

    size_t peerSize = 0;
public:
    LobbyServer (uint16_t port, ENetAddress playServer) : playServer(playServer) {
        ENetAddress address;

        address.host = ENET_HOST_ANY;
        address.port = port;

        server = enet_host_create(&address, 32, 2, 0, 0);

        if (server == nullptr) throw std::runtime_error("Cannot create ENet server");
    }

    void Poll () {
        ENetEvent event;
        while (enet_host_service(server, &event, 20) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    println(event.peer->address, "connected");

                    if (started) {
                        char packetContent[100];
                        sprintf(packetContent, "%u %hu", playServer.host, playServer.port);
                        
                        ENetPacket* packet = enet_packet_create(packetContent, strlen(packetContent) + 1, ENET_PACKET_FLAG_RELIABLE);
                        enet_peer_send(event.peer, 0, packet);
                    }

                    peerSize++;
                    break;
                case ENET_EVENT_TYPE_RECEIVE: {
                    println("from", event.peer->address, "recieved", event.packet->data);
                    
                    auto msg = std::string(reinterpret_cast<const char*>(event.packet->data));
                    if (msg == "start") {
                        started = true;

                        char packetContent[100];
                        sprintf(packetContent, "%u %hu", playServer.host, playServer.port);
                        
                        for (size_t i = 0; i < peerSize; i++) {
                            ENetPacket* packet = enet_packet_create(packetContent, strlen(packetContent) + 1, ENET_PACKET_FLAG_RELIABLE);
                            enet_peer_send(&server->peers[i], 0, packet);
                        }
                    }
                    
                    enet_packet_destroy(event.packet);
                    }
                    break;
                case ENET_EVENT_TYPE_DISCONNECT:
                    println(event.peer->address, "disconnected");
                    peerSize--;
                    break;
                default:
                    break;
            };
        }
    }

    ~LobbyServer () {
        enet_host_destroy(server);
    }
};

int main () {
    try {
        if (enet_initialize()) throw std::runtime_error("Cannot init ENet");
        atexit(enet_deinitialize);

        auto targetAddress = ENetAddress{.host = 0, .port = 10888};
        enet_address_set_host(&targetAddress, "localhost");

        auto server = LobbyServer(10887, targetAddress);

        while (true) {
            server.Poll();
        }
    } catch (std::exception& e) {
        std::cout << e.what();
    }

    return 0;
}


