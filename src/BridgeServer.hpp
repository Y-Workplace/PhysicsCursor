#pragma once

#include "SharedBridge.hpp"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>

class BridgeServer {
public:
    SharedCursorBridge* bridge{nullptr};
    int shmFd{-1};

    bool init() {
        // Cria ou abre o segmento de memória compartilhada
        shmFd = shm_open(PHYSICS_CURSOR_SHM_NAME, O_CREAT | O_RDWR, 0666);
        if (shmFd < 0) {
            std::cerr << "[BridgeServer] Falha ao criar shm_open: " << errno << std::endl;
            return false;
        }

        if (ftruncate(shmFd, sizeof(SharedCursorBridge)) != 0) {
            std::cerr << "[BridgeServer] Falha no ftruncate: " << errno << std::endl;
            close(shmFd);
            shmFd = -1;
            return false;
        }

        void* ptr = mmap(nullptr, sizeof(SharedCursorBridge), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
        if (ptr == MAP_FAILED || ptr == nullptr) {
            std::cerr << "[BridgeServer] Falha no mmap: " << errno << std::endl;
            close(shmFd);
            shmFd = -1;
            return false;
        }

        bridge = static_cast<SharedCursorBridge*>(ptr);

        // Inicializa o contrato compartilhado
        bridge->magic = PHYSICS_BRIDGE_MAGIC;
        bridge->version = 1;
        bridge->rotationAngle.store(0.0f, std::memory_order_relaxed);
        bridge->lastHeartbeatMs.store(getNowMs(), std::memory_order_relaxed);
        bridge->isDaemonActive.store(true, std::memory_order_relaxed);

        std::cout << "[BridgeServer] Memoria compartilhada ativada com sucesso: " 
                  << PHYSICS_CURSOR_SHM_NAME << std::endl;
        return true;
    }

    void publish(float angle) {
        if (!bridge) return;
        bridge->rotationAngle.store(angle, std::memory_order_relaxed);
        bridge->lastHeartbeatMs.store(getNowMs(), std::memory_order_relaxed);
    }

    bool getHyprlandPointerPos(float& x, float& y) {
        if (!bridge) return false;
        x = bridge->mouseX.load(std::memory_order_relaxed);
        y = bridge->mouseY.load(std::memory_order_relaxed);
        return true;
    }

    void shutdown() {
        if (bridge) {
            bridge->isDaemonActive.store(false, std::memory_order_relaxed);
            bridge->rotationAngle.store(0.0f, std::memory_order_relaxed);
            munmap(bridge, sizeof(SharedCursorBridge));
            bridge = nullptr;
        }
        if (shmFd >= 0) {
            close(shmFd);
            shmFd = -1;
        }
        shm_unlink(PHYSICS_CURSOR_SHM_NAME);
    }

    ~BridgeServer() {
        shutdown();
    }
};
