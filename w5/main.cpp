#include <raylib.h>
#include <enet/enet.h>
#include <math.h>
#include <cstdio>
#include <deque>
#include <queue>
#include <map>

#include "entity.h"
#include "protocol.h"

namespace {
    const uint32_t PREDICTION_WINDOW = 10;
    const int INITIAL_WINDOW_WIDTH = 600;
    const int INITIAL_WINDOW_HEIGHT = 600;
    const float CAMERA_ZOOM = 10.0f;
    const size_t MAX_HISTORY_SIZE = 200;
    const Color BACKGROUND_COLOR = GRAY;
}


class GameClient {
    public:
    GameClient();
    ~GameClient();
    
    bool InitializeNetwork();
    void Run();
    
private:
    struct GameState {
        std::map<uint16_t, std::queue<Entity::State>> pendingStates;
        std::map<uint16_t, Entity> entities;
        std::deque<Entity::State> stateHistory;
        Entity::State correction = {0, 0, 0};
        uint16_t controlledEntityId = Entity::invalid;
        uint32_t currentFrame = 0;
        int historyBeginFrame = 0;
    };
    
    void ProcessNetworkEvents();
    void ProcessPlayerInput();
    void UpdateEntities(int deltaFrames);
    void RenderFrame(int deltaFrames);
    void HandleNewEntity(ENetPacket* packet);
    void HandleControlledEntity(ENetPacket* packet);
    void HandleSnapshot(ENetPacket* packet);
    
    void ApplyCorrection(const Entity::State& historicalState, const Entity::State& serverState);
    void UpdateEntityInterpolation(Entity& entity, float& renderX, float& renderY, float& renderOri);
    void UpdateControlledEntity(Entity& entity, int deltaFrames);
    
    GameState m_state;
    ENetHost* m_client = nullptr;
    ENetPeer* m_serverPeer = nullptr;
    Camera2D m_camera;
    bool m_isConnected = false;
};

GameClient::GameClient() {
    InitWindow(INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT, "Networked Game Client");
    
    const int screenWidth = GetMonitorWidth(0);
    const int screenHeight = GetMonitorHeight(0);
    
    if (screenWidth < INITIAL_WINDOW_WIDTH || screenHeight < INITIAL_WINDOW_HEIGHT) {
        int width = std::min(screenWidth, INITIAL_WINDOW_WIDTH);
        int height = std::min(screenHeight - 150, INITIAL_WINDOW_HEIGHT);
        SetWindowSize(width, height);
    }

    m_camera = {
        .offset = {GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f},
        .target = {0.0f, 0.0f},
        .rotation = 0.0f,
        .zoom = CAMERA_ZOOM
    };
    
    SetTargetFPS(60);
}

GameClient::~GameClient() {
    if (m_client) {
        enet_host_destroy(m_client);
    }
    CloseWindow();
}

bool GameClient::InitializeNetwork() {
    if (enet_initialize() != 0) {
        printf("Failed to initialize ENet\n");
        return false;
    }
    atexit(enet_deinitialize);

    m_client = enet_host_create(nullptr, 1, 2, 0, 0);
    if (!m_client) {
        printf("Failed to create ENet client\n");
        return false;
    }

    ENetAddress address;
    enet_address_set_host(&address, "localhost");
    address.port = 10131;

    m_serverPeer = enet_host_connect(m_client, &address, 2, 0);
    if (!m_serverPeer) {
        printf("Failed to connect to server\n");
        return false;
    }
    
    return true;
}

void GameClient::Run() {
    uint32_t lastTime = enet_time_get();

    while (!WindowShouldClose()) {
        const uint32_t currentTime = enet_time_get();
        const int deltaFrames = static_cast<int>(currentTime / update) - static_cast<int>(lastTime / update);
        
        m_state.currentFrame += deltaFrames;
        
        ProcessNetworkEvents();
        ProcessPlayerInput();
        UpdateEntities(deltaFrames);
        
        BeginDrawing();
        ClearBackground(BACKGROUND_COLOR);
        RenderFrame(deltaFrames);
        EndDrawing();
        
        lastTime = currentTime;
    }
}

void GameClient::ProcessNetworkEvents() {
    ENetEvent event;
    while (enet_host_service(m_client, &event, 0) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                printf("Connected to %x:%u\n", event.peer->address.host, event.peer->address.port);
                send_join(m_serverPeer);
                m_isConnected = true;
                break;
                
            case ENET_EVENT_TYPE_RECEIVE:
                switch (get_packet_type(event.packet)) {
                    case E_SERVER_TO_CLIENT_NEW_ENTITY:
                        HandleNewEntity(event.packet);
                        break;
                    case E_SERVER_TO_CLIENT_SET_CONTROLLED_ENTITY:
                        HandleControlledEntity(event.packet);
                        break;
                    case E_SERVER_TO_CLIENT_SNAPSHOT:
                        HandleSnapshot(event.packet);
                        break;
                }
                enet_packet_destroy(event.packet);
                break;
                
            default:
                break;
        }
    }
}

void GameClient::HandleNewEntity(ENetPacket* packet) {
    Entity newEntity;
    deserialize_new_entity(packet, newEntity);
    m_state.entities[newEntity.eid] = newEntity;
}

void GameClient::HandleControlledEntity(ENetPacket* packet) {
    uint32_t serverTime;
    deserialize_set_controlled_entity(packet, m_state.controlledEntityId, serverTime);
    m_state.currentFrame = serverTime / update;
    m_state.historyBeginFrame = m_state.currentFrame;
}

void GameClient::HandleSnapshot(ENetPacket* packet) {
    uint16_t entityId = Entity::invalid;
    Entity::State state;
    deserialize_snapshot(packet, entityId, state.x, state.y, state.ori, state.physFrame);
    
    state.physFrame += PREDICTION_WINDOW;
    
    if (m_state.currentFrame < state.physFrame) {
        m_state.pendingStates[entityId].push(state);
    } 
    else if (state.physFrame - PREDICTION_WINDOW >= m_state.historyBeginFrame && 
             entityId == m_state.controlledEntityId) {
        const auto& historicalState = m_state.stateHistory[state.physFrame - PREDICTION_WINDOW - m_state.historyBeginFrame];
        ApplyCorrection(historicalState, state);
    }
}

void GameClient::ApplyCorrection(const Entity::State& historicalState, const Entity::State& serverState) {
    m_state.correction.x += historicalState.x - serverState.x;
    m_state.correction.y += historicalState.y - serverState.y;
    m_state.correction.ori += historicalState.ori - serverState.ori;
}

void GameClient::ProcessPlayerInput() {
    if (m_state.controlledEntityId == Entity::invalid) return;

    const float thr = IsKeyDown(KEY_UP) ? 1.0f : (IsKeyDown(KEY_DOWN) ? -1.0f : 0.0f);
    const float steer = IsKeyDown(KEY_LEFT) ? -1.0f : (IsKeyDown(KEY_RIGHT) ? 1.0f : 0.0f);
    
    auto& entity = m_state.entities[m_state.controlledEntityId];
    entity.thr = thr;
    entity.steer = steer;
    
    send_entity_input(m_serverPeer, m_state.controlledEntityId, thr, steer);
}

void GameClient::UpdateEntities(int deltaFrames) {
    for (auto& [entityId, entity] : m_state.entities) {
        auto& stateQueue = m_state.pendingStates[entityId];

        if (entityId == m_state.controlledEntityId && m_state.historyBeginFrame > 0 && deltaFrames > 0) {
            UpdateControlledEntity(entity, deltaFrames);
        }

        while (!stateQueue.empty() && stateQueue.front().physFrame <= m_state.currentFrame) {
            const auto& nextState = stateQueue.front();
            
            entity.x = nextState.x;
            entity.y = nextState.y;
            entity.ori = nextState.ori;
            entity.physFrame = nextState.physFrame;
            
            m_state.correction = {0, 0, 0};
            stateQueue.pop();
        }
    }
}

void GameClient::UpdateControlledEntity(Entity& entity, int deltaFrames) {
    for (int i = 0; i < deltaFrames - 1; i++) {
        m_state.stateHistory.push_back({entity.x, entity.y, entity.ori});
        if (m_state.stateHistory.size() > MAX_HISTORY_SIZE) {
            m_state.stateHistory.pop_front();
            m_state.historyBeginFrame++;
        }
    }
}

void GameClient::RenderFrame(int deltaFrames) {
    BeginMode2D(m_camera);
    
    for (auto& [entityId, entity] : m_state.entities) {
        float renderX, renderY, renderOri;
        
        if (!m_state.pendingStates[entityId].empty()) {
            UpdateEntityInterpolation(entity, renderX, renderY, renderOri);
        } else {
            if (entityId == m_state.controlledEntityId) {
                simulate_entity(entity, deltaFrames);
            }
            renderX = entity.x + m_state.correction.x;
            renderY = entity.y + m_state.correction.y;
            renderOri = entity.ori + m_state.correction.ori;
        }

        if (entityId == m_state.controlledEntityId && m_state.historyBeginFrame > 0 && deltaFrames > 0) {
            m_state.stateHistory.push_back({renderX, renderY, renderOri});
        }
        
        const Rectangle rect = {renderX, renderY, 3.0f, 1.0f};
        DrawRectanglePro(rect, {0.0f, 0.5f}, renderOri * 180.0f / PI, GetColor(entity.color));
    }
    
    EndMode2D();
}

void GameClient::UpdateEntityInterpolation(Entity& entity, float& renderX, float& renderY, float& renderOri) {
    const auto& nextState = m_state.pendingStates[entity.eid].front();
    const int totalFrames = nextState.physFrame - entity.physFrame;
    const int framesToNext = nextState.physFrame - m_state.currentFrame;
    const int framesFromPrev = m_state.currentFrame - entity.physFrame;
    
    renderX = ((entity.x + m_state.correction.x) * framesToNext + nextState.x * framesFromPrev) / totalFrames;
    renderY = ((entity.y + m_state.correction.y) * framesToNext + nextState.y * framesFromPrev) / totalFrames;
    renderOri = ((entity.ori + m_state.correction.ori) * framesToNext + nextState.ori * framesFromPrev) / totalFrames;
}

int main(int argc, const char** argv) {
    GameClient client;
    if (!client.InitializeNetwork()) {
        return EXIT_FAILURE;
    }
    
    client.Run();
    return EXIT_SUCCESS;
}