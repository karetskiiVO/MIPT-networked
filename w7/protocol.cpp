#include "protocol.h"
#include "quantisation.h"
#include <cstring>
#include <iostream>

void send_join (ENetPeer *peer) {
    ENetPacket *packet = enet_packet_create(nullptr, sizeof(uint8_t), ENET_PACKET_FLAG_RELIABLE);
    *packet->data = E_CLIENT_TO_SERVER_JOIN;

    enet_peer_send(peer, 0, packet);
}

void send_new_entity (ENetPeer *peer, const Entity &ent) {
    ENetPacket *packet = enet_packet_create(nullptr, sizeof(uint8_t) + sizeof(Entity), ENET_PACKET_FLAG_RELIABLE);
    uint8_t *ptr = packet->data;
    *ptr = E_SERVER_TO_CLIENT_NEW_ENTITY;
    ptr += sizeof(uint8_t);
    memcpy(ptr, &ent, sizeof(Entity));
    ptr += sizeof(Entity);

    enet_peer_send(peer, 0, packet);
}

void send_set_controlled_entity (ENetPeer *peer, uint16_t eid) {
    ENetPacket *packet = enet_packet_create(nullptr, sizeof(uint8_t) + sizeof(uint16_t), ENET_PACKET_FLAG_RELIABLE);
    uint8_t *ptr = packet->data;
    *ptr = E_SERVER_TO_CLIENT_SET_CONTROLLED_ENTITY;
    ptr += sizeof(uint8_t);
    memcpy(ptr, &eid, sizeof(uint16_t));
    ptr += sizeof(uint16_t);

    enet_peer_send(peer, 0, packet);
}

void send_entity_input(ENetPeer* peer, uint16_t eid, float thr, float steer)
{
    ENetPacket* packet = enet_packet_create(nullptr, sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint8_t), ENET_PACKET_FLAG_UNSEQUENCED);
    uint8_t* ptr = packet->data;
    *ptr = E_CLIENT_TO_SERVER_INPUT;
    ptr += sizeof(uint8_t);
    memcpy(ptr, &eid, sizeof(uint16_t));
    ptr += sizeof(uint16_t);
    float4bitsQuantized thrPacked(thr, -1.f, 1.f);
    float4bitsQuantized steerPacked(steer, -1.f, 1.f);
    uint8_t thrSteerPacked = (thrPacked.packedVal << 4) | steerPacked.packedVal;
    memcpy(ptr, &thrSteerPacked, sizeof(uint8_t));
    ptr += sizeof(uint8_t);

    enet_peer_send(peer, 1, packet);
}

static constexpr unsigned PositionQuantizedHDLen = 16;
static constexpr unsigned PositionQuantizedLDLen = 8;
static constexpr unsigned RotationHDLen = 8;
static constexpr unsigned RotationLDLen = 5;

using PositionQuantizedHD = PackedFloat<uint16_t, PositionQuantizedHDLen>;
using PositionQuantizedLD = PackedFloat<uint8_t, PositionQuantizedLDLen>;
using VelocityQuantized   = PackedFloat<uint8_t, 8>;
using OmegaQuantized      = PackedFloat<uint8_t, 8>;

struct [[gnu::packed]] SnapshotPacketHD {
    uint8_t messageType;
    uint8_t lod;
    uint16_t entityId;
    uint16_t xPacked;
    uint16_t yPacked;
    uint8_t orientation;
    uint8_t velXPacked;
    uint8_t velYPacked;
    uint8_t omegaPacked;
};

struct [[gnu::packed]] SnapshotPacketLD {
    uint8_t messageType;
    uint8_t lod;
    uint16_t entityId;
    uint8_t xPacked;
    uint8_t yPacked;
    uint8_t orientation;
    uint8_t velXPacked;
    uint8_t velYPacked;
    uint8_t omegaPacked;
};

void send_snapshot (ENetPeer *peer, unsigned short lodId, uint16_t eid, float x, float y, float ori, float velX, float velY, float omega){
    LOD lod = LODS[lodId];

    if (lod.quality > 0) {
        SnapshotPacketHD packet{
            .messageType = E_SERVER_TO_CLIENT_SNAPSHOT,
            .lod = (uint8_t)lodId,
            .entityId = eid,
            .xPacked = PositionQuantizedHD(x, -worldSize, worldSize).packedVal,
            .yPacked = PositionQuantizedHD(y, -worldSize, worldSize).packedVal,
            .orientation = packFloat<uint8_t>(ori, -PI, PI, RotationHDLen),
            .velXPacked = packFloat<uint8_t>(velX, -40.0f, 40.0f, 8),
            .velYPacked = packFloat<uint8_t>(velY, -40.0f, 40.0f, 8),
            .omegaPacked = packFloat<uint8_t>(omega, -10.0f, 10.0f, 8),
        };

        ENetPacket *enetPacket = enet_packet_create(&packet, sizeof(SnapshotPacketHD), ENET_PACKET_FLAG_UNSEQUENCED);
        enet_peer_send(peer, 1, enetPacket);
    } else {
        SnapshotPacketLD packet{
            .messageType = E_SERVER_TO_CLIENT_SNAPSHOT,
            .lod = (uint8_t)lodId,
            .entityId = eid,
            .xPacked = PositionQuantizedLD(x, -worldSize, worldSize).packedVal,
            .yPacked = PositionQuantizedLD(y, -worldSize, worldSize).packedVal,
            .orientation = packFloat<uint8_t>(ori, -PI, PI, RotationLDLen),
            .velXPacked = packFloat<uint8_t>(velX, -40.0f, 40.0f, 8),
            .velYPacked = packFloat<uint8_t>(velY, -40.0f, 40.0f, 8),
            .omegaPacked = packFloat<uint8_t>(omega, -10.0f, 10.0f, 8),
        };

        ENetPacket *enetPacket = enet_packet_create(&packet, sizeof(SnapshotPacketLD), ENET_PACKET_FLAG_UNSEQUENCED);

        enet_peer_send(peer, 1, enetPacket);
    }
}

void send_time_msec (ENetPeer *peer, uint32_t timeMsec) {
    ENetPacket *packet = enet_packet_create(nullptr, sizeof(uint8_t) + sizeof(uint32_t), ENET_PACKET_FLAG_RELIABLE);
    uint8_t *ptr = packet->data;
    *ptr = E_SERVER_TO_CLIENT_TIME_MSEC;
    ptr += sizeof(uint8_t);
    memcpy(ptr, &timeMsec, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    enet_peer_send(peer, 0, packet);
}

MessageType get_packet_type (ENetPacket *packet) {
    return (MessageType)*packet->data;
}

void deserialize_new_entity (ENetPacket *packet, Entity &ent) {
    uint8_t *ptr = packet->data;
    ptr += sizeof(uint8_t);
    ent = *(Entity *)(ptr);
    ptr += sizeof(Entity);
}

void deserialize_set_controlled_entity (ENetPacket *packet, uint16_t &eid) {
    uint8_t *ptr = packet->data;
    ptr += sizeof(uint8_t);
    eid = *(uint16_t *)(ptr);
    ptr += sizeof(uint16_t);
}

void deserialize_entity_input (ENetPacket *packet, uint16_t &eid, float &thr, float &steer) {
    uint8_t *ptr = packet->data;
    ptr += sizeof(uint8_t);
    eid = *(uint16_t *)(ptr);
    ptr += sizeof(uint16_t);
    uint8_t thrSteerPacked = *(uint8_t *)(ptr);
    ptr += sizeof(uint8_t);
    static uint8_t neutralPackedValue = packFloat<uint8_t>(0.f, -1.f, 1.f, 4);
    static uint8_t nominalPackedValue = packFloat<uint8_t>(1.f, 0.f, 1.2f, 4);
    float4bitsQuantized thrPacked(thrSteerPacked >> 4);
    float4bitsQuantized steerPacked(thrSteerPacked & 0x0f);
    thr = thrPacked.packedVal == neutralPackedValue ? 0.f : thrPacked.unpack(-1.f, 1.f);
    steer = steerPacked.packedVal == neutralPackedValue ? 0.f : steerPacked.unpack(-1.f, 1.f);
}

void deserialize_snapshot (ENetPacket* packet, uint16_t& eid, float& x, float& y, float& ori, float& velX, float& velY, float& omega) {
    const uint8_t lodId = packet->data[1];

    if (LODS[lodId].quality > 0) {
        auto &hd = *reinterpret_cast<const SnapshotPacketHD*>(packet->data);
        eid   = hd.entityId;
        x     = PositionQuantizedHD(hd.xPacked).unpack(-worldSize, worldSize);
        y     = PositionQuantizedHD(hd.yPacked).unpack(-worldSize, worldSize);
        ori   = unpackFloat<uint8_t>(hd.orientation, -PI, PI, RotationHDLen);
        velX  = unpackFloat<uint8_t>(hd.velXPacked, -40, 40, 8);
        velY  = unpackFloat<uint8_t>(hd.velYPacked, -40, 40, 8);
        omega = unpackFloat<uint8_t>(hd.omegaPacked, -10, 10, 8);
    } else {
        auto &ld = *reinterpret_cast<const SnapshotPacketLD*>(packet->data);
        eid   = ld.entityId;
        x     = PositionQuantizedLD(ld.xPacked).unpack(-worldSize, worldSize);
        y     = PositionQuantizedLD(ld.yPacked).unpack(-worldSize, worldSize);
        ori   = unpackFloat<uint8_t>(ld.orientation, -PI, PI, RotationLDLen);
        velX  = unpackFloat<uint8_t>(ld.velXPacked, -40, 40, 8);
        velY  = unpackFloat<uint8_t>(ld.velYPacked, -40, 40, 8);
        omega = unpackFloat<uint8_t>(ld.omegaPacked, -10, 10, 8);
    }
}

void deserialize_time_msec (ENetPacket *packet, uint32_t &timeMsec) {
    timeMsec = *(uint32_t *)(packet->data + sizeof(uint8_t));
}
