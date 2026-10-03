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
    bool isOwner{false};
    bool isDaemonMode{false};

    bool init(bool asDaemon = false) {
        isDaemonMode = asDaemon;

        // 1. Verifica se já existe um segmento ativo com daemon rodando
        int existingFd = shm_open(PHYSICS_CURSOR_SHM_NAME, O_RDWR, 0666);
        if (existingFd >= 0) {
            void* ptr = mmap(nullptr, sizeof(SharedCursorBridge), PROT_READ | PROT_WRITE, MAP_SHARED, existingFd, 0);
            if (ptr != MAP_FAILED && ptr != nullptr) {
                auto* existingBridge = static_cast<SharedCursorBridge*>(ptr);
                uint64_t now = getNowMs();
                uint64_t lastHb = existingBridge->lastHeartbeatMs.load(std::memory_order_relaxed);
                bool daemonRunning = (existingBridge->magic == PHYSICS_BRIDGE_MAGIC) &&
                                     existingBridge->isDaemonActive.load(std::memory_order_relaxed) &&
                                     (now >= lastHb) && (now - lastHb < 1500);

                if (daemonRunning) {
                    if (isDaemonMode) {
                        // Novo daemon assumindo a ponte
                        bridge = existingBridge;
                        shmFd = existingFd;
                        isOwner = true;
                        bridge->lastHeartbeatMs.store(now, std::memory_order_relaxed);
                        bridge->isDaemonActive.store(true, std::memory_order_relaxed);
                        std::cout << "[BridgeServer] Daemon assumiu segmento existente (" 
                                  << PHYSICS_CURSOR_SHM_NAME << ")." << std::endl;
                        return true;
                    } else {
                        // Processo GUI/Simulação: conecta em modo cliente/observador
                        // NÃO é dono do ciclo de vida, não destruirá a memória ao fechar a janela
                        bridge = existingBridge;
                        shmFd = existingFd;
                        isOwner = false;
                        std::cout << "[BridgeServer] Daemon em segundo plano ativo detectado. "
                                  << "GUI conectada como observador (o cursor do sistema permanecera ativo ao fechar)." << std::endl;
                        return true;
                    }
                }
                munmap(ptr, sizeof(SharedCursorBridge));
            }
            close(existingFd);
        }

        // 2. Se nenhum daemon ativo existe, cria o segmento
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
        isOwner = isDaemonMode; // Somente daemon gerencia remoção total

        bridge->magic = PHYSICS_BRIDGE_MAGIC;
        bridge->version = 1;
        bridge->rotationAngle.store(0.0f, std::memory_order_relaxed);
        bridge->lastHeartbeatMs.store(getNowMs(), std::memory_order_relaxed);
        bridge->isDaemonActive.store(true, std::memory_order_relaxed);

        std::cout << "[BridgeServer] Memoria compartilhada ativada: " 
                  << PHYSICS_CURSOR_SHM_NAME << (isOwner ? " (Dono Daemon)" : " (Modo Standalone)") << std::endl;
        return true;
    }

    void publish(float angle) {
        if (!bridge) return;
        // Se a GUI estiver aberta e um daemon já estiver rodando, a GUI não sobrescreve a rotação do sistema
        if (!isDaemonMode && !isOwner) return;

        bridge->rotationAngle.store(angle, std::memory_order_relaxed);
        bridge->lastHeartbeatMs.store(getNowMs(), std::memory_order_relaxed);
    }

    bool isDaemonActive() const {
        if (!bridge) return false;
        uint64_t now = getNowMs();
        uint64_t lastHb = bridge->lastHeartbeatMs.load(std::memory_order_relaxed);
        return (bridge->magic == PHYSICS_BRIDGE_MAGIC) &&
               bridge->isDaemonActive.load(std::memory_order_relaxed) &&
               (now >= lastHb) && (now - lastHb < 1500);
    }

    bool getHyprlandPointerPos(float& x, float& y) {
        if (!bridge) return false;
        x = bridge->mouseX.load(std::memory_order_relaxed);
        y = bridge->mouseY.load(std::memory_order_relaxed);
        return true;
    }

    bool getHyprlandPointerPos(float& x, float& y, uint64_t& seq) {
        if (!bridge) return false;
        x = bridge->mouseX.load(std::memory_order_relaxed);
        y = bridge->mouseY.load(std::memory_order_relaxed);
        seq = bridge->moveSeq.load(std::memory_order_relaxed);
        return true;
    }

    void shutdown() {
        if (bridge) {
            if (isOwner) {
                bridge->isDaemonActive.store(false, std::memory_order_relaxed);
                bridge->rotationAngle.store(0.0f, std::memory_order_relaxed);
            }
            munmap(bridge, sizeof(SharedCursorBridge));
            bridge = nullptr;
        }
        if (shmFd >= 0) {
            close(shmFd);
            shmFd = -1;
        }
        // Somente o dono real do daemon desvincula a memória compartilhada
        if (isOwner) {
            shm_unlink(PHYSICS_CURSOR_SHM_NAME);
            std::cout << "[BridgeServer] Segmento SHM desvinculado pelo daemon." << std::endl;
        }
    }

    ~BridgeServer() {
        shutdown();
    }
};
