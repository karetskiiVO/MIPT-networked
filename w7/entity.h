#pragma once
#include <cstdint>
#include <map>

constexpr uint16_t invalid_entity = -1;
constexpr float worldSize = 120.f;
struct Entity {
    uint32_t color = 0xff00ffff;
    bool serverControlled = false;

    float x = 0.f;
    float y = 0.f;
    float vx = 0.f;
    float vy = 0.f;
    float ori = 0.f;
    float omega = 0.f;

    float thr = 0.f;
    float steer = 0.f;

    uint16_t eid = invalid_entity;
};

struct LOD {
    uint8_t quality = 0;
    float treshold = 0.1f;
    float bound = 0.0f;

    LOD () = default;
    LOD (uint8_t ql, float drThr, float dist) : quality(ql), treshold(drThr), bound(dist) {}
};

static constexpr unsigned LODS_CNT = 4;
extern LOD LODS[LODS_CNT];

unsigned short get_lod(float distance);

struct ServerEntity {
    Entity entity;
    std::map<uint16_t, Entity> deadReckonings;
};

void simulate_entity(Entity &e, float dt);
