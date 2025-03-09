#include <raylib.h>
#include <enet/enet.h>
#include <iostream>

#include <string>
#include <map>

#include "player.h"
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

bool operator== (const ENetAddress& lhv,  const ENetAddress& rhv) {
    return (lhv.host == rhv.host) && (lhv.port == rhv.port);
}

class Client {
    ENetHost* client = nullptr;
    ENetPeer* lobbyPeer = nullptr;
    ENetPeer* gamePeer = nullptr;

    std::map<std::string, Player> players;

    bool initiatedOnServer = false;

    float posx = 150.f;
    float posy = 150.f;
    float velx = 0.f;
    float vely = 0.f;
public:
    Client (ENetAddress lobbyServerAddress) {
        // инициализация сети
        client = enet_host_create(nullptr, 2, 4, 0, 0);
        if (!client) throw std::runtime_error("Cannot create ENet client");
        lobbyPeer = enet_host_connect(client, &lobbyServerAddress, 2, 0);
        if (!lobbyPeer) throw std::runtime_error("Cannot connect to lobby");
        
        // инициализация окна
        int width = 800;
        int height = 600;
        InitWindow(width, height, "w2 MIPT networked");
    
        const int scrWidth = GetMonitorWidth(0);
        const int scrHeight = GetMonitorHeight(0);
        if (scrWidth < width || scrHeight < height) {
            width = std::min(scrWidth, width);
            height = std::min(scrHeight - 150, height);
            SetWindowSize(width, height);
        }
    
        SetTargetFPS(60);  // Set our game to run at 60 frames-per-second
    }

    bool InProcess () {
        return !WindowShouldClose();
    }

    void Update () {
        float dt = GetFrameTime();
        HandleInput(dt);
        NetPoll(dt);
        Draw(dt);
    }

    void Draw (float dt) {
        BeginDrawing();
        ClearBackground(BLACK);
        DrawText(TextFormat("Current status: %s", "unknown"), 20, 20, 20, WHITE);
        DrawText(TextFormat("My position: (%d, %d)", (int)posx, (int)posy), 20, 40, 20, WHITE);
        DrawText("List of players:", 20, 60, 20, WHITE);
        
        int cnt = 1;
        for (const auto& [id, player] : players) {
            DrawText(TextFormat("%s %d", id.c_str(), player.ping), 20, 60 + cnt * 20, 20, GREEN);
            DrawCircleV(Vector2{player.x, player.y}, 10.f, WHITE);
            cnt++;
        }
        EndDrawing();
    }

    void HandleInput (float dt) {
        if (gamePeer == nullptr) {
            if (!IsKeyDown(KEY_ENTER)) return;

            printf("%p\n", gamePeer);

            const std::string startMsg = "start";

            ENetPacket* packet = enet_packet_create(startMsg.c_str(), startMsg.length() + 1, ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(lobbyPeer, 0, packet);
        } else {
            bool left = IsKeyDown(KEY_LEFT);
            bool right = IsKeyDown(KEY_RIGHT);
            bool up = IsKeyDown(KEY_UP);
            bool down = IsKeyDown(KEY_DOWN);
            constexpr float accel = 30.f;

            velx += ((left ? -1.f : 0.f) + (right ? 1.f : 0.f)) * dt * accel;
            vely += ((up ? -1.f : 0.f) + (down ? 1.f : 0.f)) * dt * accel;
            posx += velx * dt;
            posy += vely * dt;
            velx *= 0.99f;
            vely *= 0.99f;
        }
    }

    void NetPoll (float dt) {
        ENetEvent event;
        while (enet_host_service(client, &event, 10) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    println("connected to", event.peer->address);
                    break;
                case ENET_EVENT_TYPE_RECEIVE:
                    println("from", event.peer->address, "recieved", event.packet->data);

                    if (gamePeer == nullptr && event.peer->address == lobbyPeer->address) {
                        ENetAddress gameServerAddress;
                        sscanf(reinterpret_cast<const char*>(event.packet->data), "%u %hu", &gameServerAddress.host, &gameServerAddress.port);
                        println(gameServerAddress);
                        gamePeer = enet_host_connect(client, &gameServerAddress, 2, 0);
                    }

                    if (gamePeer != nullptr && !initiatedOnServer && event.peer->address == gamePeer->address && event.packet->data[0] == 'l') {
                        std::vector<std::string> playersList = split(reinterpret_cast<const char*>(event.packet->data + 2));
                        
                        for (const auto& player : playersList) {
                            players[player] = Player{
                                .id = "",
                                .x = 150.f,
                                .y = 150.f,
                                .ping = 999,
                            };
                        }     
                        
                        initiatedOnServer = true;
                    }

                    if (gamePeer != nullptr && initiatedOnServer && event.peer->address == gamePeer->address && event.packet->data[0] == 'u') {
                        std::vector<std::string> updateList = split(reinterpret_cast<const char*>(event.packet->data + 2));
                        
                        for (const auto& update : updateList) {
                            std::vector<std::string> info = split(update, '-');
                            players[info[0]] = Player{
                                .id = "",
                                .x = std::stof(info[1]),
                                .y = std::stof(info[2]),
                                .ping = std::stoul(info[3]),
                            };
                        } 
                    }

                    enet_packet_destroy(event.packet);
                    break;
                default:
                    break;
            };
        }

        if (gamePeer != nullptr) {
            std::string msg = std::to_string(posx) + " " + std::to_string(posy);        
            ENetPacket* packet = enet_packet_create(msg.c_str(), msg.length() + 1, ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(gamePeer, 0, packet);
        }
    }
};

int main () {
    try {
        if (enet_initialize()) throw std::runtime_error("Cannot init ENet");
        atexit(enet_deinitialize);

        auto targetAddress = ENetAddress{.host = 0, .port = 10887};
        enet_address_set_host(&targetAddress, "localhost");

        auto client = Client(targetAddress);

        while (client.InProcess()) {
            client.Update();
        }
    } catch (std::exception& e) {
        std::cout << e.what();
    }

    return 0;
}


