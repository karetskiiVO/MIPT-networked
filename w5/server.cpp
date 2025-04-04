#include <enet/enet.h>
#include <stdlib.h>

#include <iostream>
#include <map>
#include <vector>
#include <chrono>
#include <thread>

#include "entity.h"
#include "mathUtils.h"
#include "protocol.h"

#include <windows.h>
void usleep(int64_t usec) {
    HANDLE timer;
    LARGE_INTEGER ft;
    ft.QuadPart = -(10*usec);
    timer = CreateWaitableTimer(NULL, TRUE, NULL);
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
}

std::vector<Entity> entities;
std::map<uint16_t, ENetPeer*> controlledMap;
uint32_t frame = 0;

void on_join(ENetPacket *packet, ENetPeer *peer, ENetHost *host) {
    for (Entity &ent : entities) {
        ent.physFrame = frame;
        send_new_entity(peer, ent);
    }

    uint16_t maxEid = entities.empty() ? Entity::invalid : entities[0].eid;
    for (const Entity &e : entities) maxEid = std::max(maxEid, e.eid);
    uint16_t newEid = maxEid + 1;
    uint32_t color = 0xff000000 + 0x00440000 * (rand() % 5) + 0x00004400 * (rand() % 5) + 0x00000044 * (rand() % 5);
    float x = (rand() % 4) * 5.f;
    float y = (rand() % 4) * 5.f;
    Entity ent = {color, x, y, 0.f, (rand() / RAND_MAX) * 3.141592654f, 0.f, 0.f, newEid, frame};
    entities.push_back(ent);

    controlledMap[newEid] = peer;

    for (size_t i = 0; i < host->peerCount; i++) send_new_entity(&host->peers[i], ent);
    send_set_controlled_entity(peer, newEid, enet_time_get());
}

void on_input(ENetPacket *packet) {
    uint16_t eid = Entity::invalid;
    float thr = 0.f;
    float steer = 0.f;
    deserialize_entity_input(packet, eid, thr, steer);
    entities[eid].thr = thr;
    entities[eid].steer = steer;
}

int main(int argc, const char **argv) {
    if (enet_initialize() != 0) {
        printf("Cannot init ENet");
        return 1;
    }
    ENetAddress address;

    address.host = ENET_HOST_ANY;
    address.port = 10131;

    ENetHost *server = enet_host_create(&address, 32, 2, 0, 0);

    if (!server) {
        printf("Cannot create ENet server\n");
        return 1;
    }

    uint32_t lastTime = enet_time_get();
    frame = lastTime / update;
    while (true) {
        uint32_t curTime = enet_time_get();

        ENetEvent event;
        while (enet_host_service(server, &event, 0) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    printf("Connection with %x:%u established\n", event.peer->address.host, event.peer->address.port);
                    break;
                case ENET_EVENT_TYPE_RECEIVE:
                    switch (get_packet_type(event.packet)) {
                        case E_CLIENT_TO_SERVER_JOIN:
                            on_join(event.packet, event.peer, server);
                            break;
                        case E_CLIENT_TO_SERVER_INPUT:
                            on_input(event.packet);
                            break;
                    };
                    enet_packet_destroy(event.packet);
                    break;
                default:
                    break;
            };
        }

        int dt = curTime / update - lastTime / update;
        frame += dt;

        for (Entity &e : entities) {
            simulate_entity(e, dt);
            for (size_t i = 0; i < server->peerCount; ++i) {
                ENetPeer *peer = &server->peers[i];
                send_snapshot(peer, e.eid, e.x, e.y, e.ori, frame);
            }
        }
        lastTime = curTime;
        usleep(100000);
    }

    enet_host_destroy(server);

    atexit(enet_deinitialize);
    return 0;
}
