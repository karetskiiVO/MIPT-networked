#include "entity.h"
#include "mathUtils.h"
#include "protocol.h"
#include <enet/enet.h>
#include <iostream>
#include <map>
#include <stdlib.h>
#include <vector>

static std::vector<ServerEntity> entities;
static std::map<uint16_t, ENetPeer *> controlledMap;
static std::map<ENetPeer *, uint16_t> controlledEntities;

#include <windows.h>
void usleep (int64_t usec) {
    HANDLE timer;
    LARGE_INTEGER ft;
    ft.QuadPart = -(10*usec);
    timer = CreateWaitableTimer(NULL, TRUE, NULL);
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
}

void on_join(ENetPacket *packet, ENetPeer *peer, ENetHost *host) {
    for (const ServerEntity &ent : entities) send_new_entity(peer, ent.entity);

    uint16_t maxEid = entities.empty() ? invalid_entity : entities[0].entity.eid;
    for (const ServerEntity &e : entities) maxEid = std::max(maxEid, e.entity.eid);
    uint16_t newEid = maxEid + 1;
    uint32_t color = 0x000000ff + 0x44000000 * (rand() % 4 + 1) + 0x00440000 * (rand() % 4 + 1) + 0x00004400 * (rand() % 4 + 1);
    float x = (rand() % 4) * 5.f;
    float y = (rand() % 4) * 5.f;
    Entity ent = {color, false, x, y, 0.f, (rand() / RAND_MAX) * 3.141592654f, 0.f, 0.f, 0.f, 0.f, newEid};
    ServerEntity sent;
    sent.entity = ent;
    for (auto &[id, peer] : controlledMap) sent.deadReckonings[id] = sent.entity;
    
    entities.push_back(sent);
    controlledEntities[peer] = newEid;

    for (ServerEntity &e : entities) e.deadReckonings[newEid] = e.entity;

    controlledMap[newEid] = peer;

    for (size_t i = 0; i < host->peerCount; ++i) send_new_entity(&host->peers[i], ent);
    send_set_controlled_entity(peer, newEid);
}

void create_server_entity (ENetHost *host) {
    uint16_t maxEid = entities.empty() ? invalid_entity : entities[0].entity.eid;
    for (const ServerEntity &e : entities) maxEid = std::max(maxEid, e.entity.eid);
    uint16_t newEid = maxEid + 1;
    uint32_t color = 0xff000000 + 0x00440000 * (rand() % 5) + 0x00004400 * (rand() % 5) + 0x00000044 * (rand() % 5);
    float x = rand() % int(worldSize * 2) - worldSize;
    float y = rand() % int(worldSize * 2) - worldSize;
    Entity ent = {color, true, x, y, 0.f, (rand() / RAND_MAX) * 3.141592654f, 0.f, 0.f, 0.f, 0.f, newEid};
    ServerEntity sent;
    sent.entity = ent;
    for (auto &[id, peer] : controlledMap) sent.deadReckonings[id] = sent.entity;
    entities.push_back(sent);

    for (ServerEntity &e : entities) e.deadReckonings[newEid] = e.entity;
    for (size_t i = 0; i < host->peerCount; ++i) send_new_entity(&host->peers[i], ent);
}

void on_input (ENetPacket *packet) {
    uint16_t eid = invalid_entity;
    float thr = 0.f;
    float steer = 0.f;
    deserialize_entity_input(packet, eid, thr, steer);
    for (ServerEntity &e : entities)
        if (e.entity.eid == eid) {
            e.entity.thr = thr;
            e.entity.steer = steer;
        }
}

static void Update_net (ENetHost *server) {
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
}

static void Update_ai (Entity &e, float dt) {
    if (rand() % 100 == 0) e.thr = e.thr > 0.f ? 0.f : 1.f;
    if (rand() % 10 == 0) e.steer = e.steer != 0.f ? 0.f : ((rand() % 2) * 2.f - 1.f);
}

static float manhattan_distance (const Entity &e1, const Entity &e2) {
    float deltaX = e1.x - e2.x;
    float deltaY = e1.y - e2.y;
    return std::min(abs(deltaX), abs(worldSize - deltaX)) + std::min(abs(deltaY), abs(worldSize - deltaY));
}

static float angular_distance (const Entity &e1, const Entity &e2) {
    float deltaPhi = e1.ori - e2.ori;
    return std::min(abs(deltaPhi), abs(2 * PI - deltaPhi));
}

static void simulate_world (ENetHost *server, float dt) {
    for (ServerEntity &e : entities) {
        if (e.entity.serverControlled)
        Update_ai(e.entity, dt);
        
        simulate_entity(e.entity, dt);
        for (auto &[id, replica] : e.deadReckonings) {
            simulate_entity(replica, dt);
        }
        
        for (size_t i = 0; i < server->peerCount; ++i) {
            ENetPeer *peer = &server->peers[i];
            if (controlledEntities.find(peer) == controlledEntities.end()) continue;
            uint16_t peerEid = controlledEntities[peer];
            ServerEntity peerEntity = entities[peerEid];

            float distance = manhattan_distance(peerEntity.entity, e.entity);

            unsigned lodId = get_lod(distance);
            LOD lod = LODS[lodId];

            Entity approximation = e.deadReckonings[peerEid];
            float approxMhDistance = manhattan_distance(approximation, e.entity);
            float approxAngDistance = angular_distance(approximation, e.entity);

            if (lod.treshold * 2.0f > approxMhDistance && lod.treshold > approxAngDistance)
                continue;

            e.deadReckonings[peerEid] = e.entity;

            send_snapshot(peer, lodId, e.entity.eid, e.entity.x, e.entity.y, e.entity.ori, e.entity.vx, e.entity.vy, e.entity.omega);
        }
    }
}

static void Update_time (ENetHost *server, uint32_t curTime) {
    for (size_t i = 0; i < server->peerCount; i++) send_time_msec(&server->peers[i], curTime);
}

int main (int argc, const char **argv) {
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

    constexpr size_t numShips = 100;
    for (size_t i = 0; i < numShips; ++i) create_server_entity(server);

    uint32_t lastTime = enet_time_get();
    while (true) {
        uint32_t curTime = enet_time_get();
        float dt = (curTime - lastTime) * 0.001f;
        lastTime = curTime;

        Update_net(server);
        simulate_world(server, dt);
        Update_time(server, curTime);
        usleep(10000);
    }

    enet_host_destroy(server);

    atexit(enet_deinitialize);
    return 0;
}
