#pragma once
#include <cstdint>
#include <atomic>
#include <chrono>

// Tests can compile a private bridge so their pointer never drives the desktop daemon.
#ifndef PHYSICS_CURSOR_SHM_PATH
#define PHYSICS_CURSOR_SHM_PATH "/physics_cursor_bridge_shm"
#endif
constexpr const char* PHYSICS_CURSOR_SHM_NAME = PHYSICS_CURSOR_SHM_PATH;
constexpr uint32_t PHYSICS_BRIDGE_MAGIC = 0x50485953; // 'PHYS'

struct alignas(64) SharedCursorBridge {
    uint32_t magic{PHYSICS_BRIDGE_MAGIC};
    uint32_t version{1};

    // Posição atual do mouse (atualizada pelo Hyprland ou Daemon)
    std::atomic<float> mouseX{0.0f};
    std::atomic<float> mouseY{0.0f};

    // Ângulo de rotação calculado pelo PhysicsCursor Daemon (radianos)
    std::atomic<float> rotationAngle{0.0f};

    // Heartbeat em milissegundos para detecção de queda
    std::atomic<uint64_t> lastHeartbeatMs{0};

    // Flag indicando se o daemon de física está rodando
    std::atomic<bool> isDaemonActive{false};
};

inline uint64_t getNowMs() {
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}
