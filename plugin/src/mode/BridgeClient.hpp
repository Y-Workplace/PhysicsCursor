#pragma once

#include "SharedBridge.hpp"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cmath>

// Only maps existing daemon memory; never creates or removes its segment.
class BridgeClient {
  public:
    ~BridgeClient() { disconnect(); }
    BridgeClient() = default;
    BridgeClient(const BridgeClient&) = delete;
    BridgeClient& operator=(const BridgeClient&) = delete;

    bool update(float x, float y, float& angle) {
        const auto now = std::chrono::steady_clock::now();
        if (!bridge && now >= nextAttempt) {
            nextAttempt = now + std::chrono::milliseconds(250);
            const int fd = shm_open(PHYSICS_CURSOR_SHM_NAME, O_RDWR, 0);
            if (fd >= 0) {
                struct stat info{};
                if (fstat(fd, &info) == 0 && info.st_size == sizeof(SharedCursorBridge)) {
                    void* ptr = mmap(nullptr, sizeof(SharedCursorBridge), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
                    if (ptr != MAP_FAILED)
                        bridge = static_cast<SharedCursorBridge*>(ptr);
                }
                close(fd);
            }
        }
        if (!bridge)
            return false;

        const auto time = getNowMs();
        const auto heartbeat = bridge->lastHeartbeatMs.load(std::memory_order_relaxed);
        if (bridge->magic != PHYSICS_BRIDGE_MAGIC || bridge->version != 1 ||
            !bridge->isDaemonActive.load(std::memory_order_relaxed) ||
            heartbeat > time || time - heartbeat >= 1500) {
            disconnect();
            return false;
        }

        bridge->mouseX.store(x, std::memory_order_relaxed);
        bridge->mouseY.store(y, std::memory_order_relaxed);
        angle = bridge->rotationAngle.load(std::memory_order_relaxed);
        return std::isfinite(angle);
    }

  private:
    SharedCursorBridge* bridge = nullptr;
    std::chrono::steady_clock::time_point nextAttempt{};
    void disconnect() {
        if (bridge)
            munmap(bridge, sizeof(SharedCursorBridge));
        bridge = nullptr;
    }
};
