#pragma once
#include <cstdint>

const int update = 20;

struct Entity {
    static constexpr uint16_t invalid = static_cast<uint16_t>(-1);

    uint32_t color = 0xff00ffff;
    float x = 0.f;
    float y = 0.f;
    float speed = 0.f;
    float ori = 0.f;

    float thr = 0.f;
    float steer = 0.f;

    uint16_t eid = invalid;

    uint32_t physFrame;

    struct State {
        float x = 0.f;
        float y = 0.f;
        float ori = 0.f;

        uint32_t physFrame;
    };
};

void simulate_entity(Entity& e, int frames);
