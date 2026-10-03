#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cstdlib>
#include "PhysicsEngine.hpp"
#include "CursorRenderer.hpp"
#include "Font8x8.hpp"

enum class HUDLanguage {
    EN,
    PT
};

class HUD {
public:
    bool visible = true;
    HUDLanguage language = HUDLanguage::EN;

    HUD() {
        const char* langEnv = std::getenv("LANG");
        if (langEnv && (std::strstr(langEnv, "pt") != nullptr || std::strstr(langEnv, "PT") != nullptr)) {
            language = HUDLanguage::PT;
        } else {
            language = HUDLanguage::EN;
        }
    }

    void toggleLanguage() {
        language = (language == HUDLanguage::EN) ? HUDLanguage::PT : HUDLanguage::EN;
    }

    void render(SDL_Renderer* renderer, const PhysicsEngine& physics, const CursorRenderer& cursorRend, 
                int winW, int winH, bool isOverlay, bool daemonActive = false) {
        if (!visible) return;
        (void)winW;

        SDL_FRect panelRect = { 15.0f, 15.0f, 470.0f, 350.0f };
        SDL_SetRenderDrawColor(renderer, 15, 20, 30, 220);
        SDL_RenderFillRect(renderer, &panelRect);

        SDL_SetRenderDrawColor(renderer, 70, 90, 120, 240);
        SDL_RenderRect(renderer, &panelRect);

        float x = panelRect.x + 12.0f;
        float y = panelRect.y + 12.0f;
        float scale = 1.0f;

        bool isPT = (language == HUDLanguage::PT);

        // Titulo
        std::string title = isPT ? "=== FISICA DE CURSOR (CACHYOS/WAYLAND) ===" : "=== CURSOR PHYSICS (CACHYOS/WAYLAND) ===";
        EmbeddedFont::drawString(renderer, title, x, y, scale, {0, 220, 255, 255});
        y += 18.0f;

        SDL_SetRenderDrawColor(renderer, 50, 70, 95, 200);
        SDL_RenderLine(renderer, x, y, x + panelRect.w - 24.0f, y);
        y += 8.0f;

        // Tipo de cursor ativo
        std::ostringstream ss;
        if (isPT) {
            ss << "[C] Cursor Ativo: " << (cursorRend.useSystemCursor ? ("SISTEMA (" + cursorRend.sysCursor.themeName + ")") : "VETORIAL");
        } else {
            ss << "[C] Active Cursor: " << (cursorRend.useSystemCursor ? ("SYSTEM (" + cursorRend.sysCursor.themeName + ")") : "VECTORIAL");
        }
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {255, 215, 0, 255});
        y += 14.0f;

        // Ponto de Pivo
        ss.str("");
        ss << std::fixed << std::setprecision(1);
        if (isPT) {
            ss << "Pivo no Hotspot: (" << physics.pivotPos.x << ", " << physics.pivotPos.y << ") px";
        } else {
            ss << "Hotspot Pivot: (" << physics.pivotPos.x << ", " << physics.pivotPos.y << ") px";
        }
        if (cursorRend.useSystemCursor && cursorRend.sysCursor.valid) {
            ss << " [Hotspot: " << cursorRend.sysCursor.xhot << "," << cursorRend.sysCursor.yhot << "]";
        }
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {255, 100, 100, 255});
        y += 14.0f;

        // Velocidade e Aceleracao
        float speed = physics.velocity.length();
        float accel = physics.acceleration.length();
        ss.str("");
        if (isPT) {
            ss << "Velocidade v: " << speed << " px/s (vx: " << physics.velocity.x << ")";
        } else {
            ss << "Velocity v: " << speed << " px/s (vx: " << physics.velocity.x << ")";
        }
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {46, 204, 113, 255});
        y += 14.0f;

        ss.str("");
        if (isPT) {
            ss << "Aceleracao a: " << accel << " px/s^2 (ax: " << physics.acceleration.x << ")";
        } else {
            ss << "Acceleration a: " << accel << " px/s^2 (ax: " << physics.acceleration.x << ")";
        }
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {243, 156, 18, 255});
        y += 14.0f;

        // Balanço Angular Simétrico
        float defDeg = physics.angle * (180.0f / (float)M_PI);
        ss.str("");
        std::string dirStr;
        if (isPT) {
            dirStr = (defDeg > 0.5f ? "DIREITA >>" : (defDeg < -0.5f ? "<< ESQUERDA" : "CENTRO"));
            ss << "Balanco Angular: " << defDeg << " deg (" << dirStr << ")";
        } else {
            dirStr = (defDeg > 0.5f ? "RIGHT >>" : (defDeg < -0.5f ? "<< LEFT" : "CENTER"));
            ss << "Angular Sway: " << defDeg << " deg (" << dirStr << ")";
        }
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {255, 230, 80, 255});
        y += 14.0f;

        ss.str("");
        if (isPT) {
            ss << "Vel. Angular omega: " << physics.angularVelocity << " rad/s";
        } else {
            ss << "Angular Vel. omega: " << physics.angularVelocity << " rad/s";
        }
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {200, 220, 240, 255});
        y += 14.0f;

        // Status do Daemon em Segundo Plano
        ss.str("");
        if (isPT) {
            ss << "Status do Daemon: " << (daemonActive ? "ATIVO (IPC CONECTADO)" : "STANDALONE");
        } else {
            ss << "System Daemon: " << (daemonActive ? "ACTIVE (IPC CONNECTED)" : "STANDALONE");
        }
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, daemonActive ? SDL_Color{0, 255, 180, 255} : SDL_Color{180, 180, 200, 255});
        y += 18.0f;

        SDL_SetRenderDrawColor(renderer, 50, 70, 95, 200);
        SDL_RenderLine(renderer, x, y, x + panelRect.w - 24.0f, y);
        y += 8.0f;

        std::string paramHeader = isPT ? "-- CONTROLES DE PARAMETROS FISICOS --" : "-- PHYSICS PARAMETER CONTROLS --";
        EmbeddedFont::drawString(renderer, paramHeader, x, y, scale, {200, 200, 200, 255});
        y += 14.0f;

        ss.str("");
        if (isPT) {
            ss << "[1/2] Sensibilidade/Arrasto: " << std::setprecision(5) << physics.velocityInfluence;
        } else {
            ss << "[1/2] Sensitivity/Drag: " << std::setprecision(5) << physics.velocityInfluence;
        }
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {230, 240, 255, 255});
        y += 13.0f;

        ss.str("");
        if (isPT) {
            ss << "[3/4] Rigidez da Mola (k): " << physics.springK;
        } else {
            ss << "[3/4] Spring Stiffness (k): " << physics.springK;
        }
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {230, 240, 255, 255});
        y += 13.0f;

        ss.str("");
        if (isPT) {
            ss << "[5/6] Amortecimento (gamma): " << physics.damping << " (Suavidade)";
        } else {
            ss << "[5/6] Damping (gamma): " << physics.damping << " (Smoothness)";
        }
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {230, 240, 255, 255});
        y += 13.0f;

        ss.str("");
        if (isPT) {
            ss << "[7/8] Forca Inercial (m*a): " << std::setprecision(5) << physics.inertiaInfluence;
        } else {
            ss << "[7/8] Inertia Force (m*a): " << std::setprecision(5) << physics.inertiaInfluence;
        }
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {230, 240, 255, 255});
        y += 13.0f;

        ss.str("");
        if (isPT) {
            ss << "[C] Cursor | [R] Reset | [L] Idioma (" << (isPT ? "PT" : "EN") << ")";
        } else {
            ss << "[C] Cursor | [R] Reset | [L] Language (" << (isPT ? "PT" : "EN") << ")";
        }
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {255, 220, 150, 255});
        y += 13.0f;

        ss.str("");
        if (isPT) {
            ss << "[V] Vetores | [T] Rastro | [P] Pivo | [SPACE] Impulso";
        } else {
            ss << "[V] Vectors | [T] Trail | [P] Pivot | [SPACE] Impulse";
        }
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {150, 200, 255, 255});

        // Rodape de status
        std::string modeText;
        if (isPT) {
            modeText = isOverlay ? "MODO: OVERLAY TRANSPARENTE [F11/O para Janela]" : "MODO: PLAYGROUND INTERATIVO [F11/O para Overlay]";
        } else {
            modeText = isOverlay ? "MODE: TRANSPARENT OVERLAY [F11/O for Window]" : "MODE: INTERACTIVE PLAYGROUND [F11/O for Overlay]";
        }
        EmbeddedFont::drawString(renderer, modeText, 20.0f, winH - 25.0f, 1.0f, {255, 255, 255, 230});
    }
};
