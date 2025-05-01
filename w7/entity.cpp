#include "entity.h"
#include "mathUtils.h"

float tile_val(float val, float border) {
    if (val < -border) return val + 2.f * border;
    if (val >  border) return val - 2.f * border;
    return val;
}

void simulate_entity(Entity &e, float dt){
    bool isBraking = e.thr < 0.f;
    float accel = isBraking ? 6.f : 1.5f;
    float va = clamp(e.thr, -0.3f, 1.f) * accel;
    e.vx += cosf(e.ori) * va * dt;
    e.vy += sinf(e.ori) * va * dt;
    e.omega += e.steer * dt * 0.3f;
    e.ori += e.omega * dt;

    if (e.ori > PI) e.ori -= 2.f * PI;
    if (e.ori < -PI) e.ori += 2.f * PI;
    
    e.x += e.vx * dt;
    e.y += e.vy * dt;

    e.x = tile_val(e.x, worldSize);
    e.y = tile_val(e.y, worldSize);
}

LOD LODS[LODS_CNT] = {
    LOD{1, 0.01f, 20.0f},
    LOD{1, 0.05f, 50.0f},
    LOD{0, 0.1f, 70.0f},
    LOD{0, 0.5f, -1.0f},
};

unsigned short get_lod(float distance) {
    for (unsigned i = 0; i < LODS_CNT; ++i) 
        if (distance <= LODS[i].bound || LODS[i].bound < 0.0f) return i;
    return 0;
}
