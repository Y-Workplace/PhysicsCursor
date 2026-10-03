#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <sstream>
#include <iomanip>
#include "PhysicsEngine.hpp"
#include "CursorRenderer.hpp"
#include "Font8x8.hpp"

class HUD {
public:
    bool visible = true;

    void render(SDL_Renderer* renderer, const PhysicsEngine& physics, const CursorRenderer& cursorRend, int winW, int winH, bool isOverlay) {
        if (!visible) return;
        (void)winW;

        SDL_FRect panelRect = { 15.0f, 15.0f, 440.0f, 335.0f };
        SDL_SetRenderDrawColor(renderer, 15, 20, 30, 215);
        SDL_RenderFillRect(renderer, &panelRect);

        SDL_SetRenderDrawColor(renderer, 70, 90, 120, 240);
        SDL_RenderRect(renderer, &panelRect);

        float x = panelRect.x + 12.0f;
        float y = panelRect.y + 12.0f;
        float scale = 1.0f;

        EmbeddedFont::drawString(renderer, "=== FISICA DE CURSOR (CACHYOS/WAYLAND) ===", x, y, scale, {0, 220, 255, 255});
        y += 18.0f;

        SDL_SetRenderDrawColor(renderer, 50, 70, 95, 200);
        SDL_RenderLine(renderer, x, y, x + panelRect.w - 24.0f, y);
        y += 8.0f;

        // Tipo de cursor ativo
        std::ostringstream ss;
        ss << "[C] Cursor Ativo: " << (cursorRend.useSystemCursor ? ("SISTEMA (" + cursorRend.sysCursor.themeName + ")") : "VETORIAL");
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {255, 215, 0, 255});
        y += 14.0f;

        ss.str("");
        ss << std::fixed << std::setprecision(1);
        ss << "Pivo no Ponto de Evento: (" << physics.pivotPos.x << ", " << physics.pivotPos.y << ") px";
        if (cursorRend.useSystemCursor && cursorRend.sysCursor.valid) {
            ss << " [Hotspot: " << cursorRend.sysCursor.xhot << "," << cursorRend.sysCursor.yhot << "]";
        }
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {255, 100, 100, 255});
        y += 14.0f;

        // Velocidade e Aceleração
        float speed = physics.velocity.length();
        float accel = physics.acceleration.length();
        ss.str("");
        ss << "Velocidade v: " << speed << " px/s (vx: " << physics.velocity.x << ")";
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {46, 204, 113, 255});
        y += 14.0f;

        ss.str("");
        ss << "Aceleracao a: " << accel << " px/s^2 (ax: " << physics.acceleration.x << ")";
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {243, 156, 18, 255});
        y += 14.0f;

        // Balanço Angular Simétrico
        float defDeg = physics.angle * (180.0f / (float)M_PI);
        ss.str("");
        ss << "Balanco Angular: " << defDeg << " deg (" 
           << (defDeg > 0.5f ? "DIREITA >>" : (defDeg < -0.5f ? "<< ESQUERDA" : "CENTRO")) << ")";
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {255, 230, 80, 255});
        y += 14.0f;

        ss.str("");
        ss << "Vel. Angular omega: " << physics.angularVelocity << " rad/s";
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {200, 220, 240, 255});
        y += 14.0f;

        ss.str("");
        ss << "Torque Liquido: " << physics.totalTorque << " | Amortecimento: " << physics.dampingTorque;
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {180, 180, 210, 255});
        y += 18.0f;

        SDL_SetRenderDrawColor(renderer, 50, 70, 95, 200);
        SDL_RenderLine(renderer, x, y, x + panelRect.w - 24.0f, y);
        y += 8.0f;

        EmbeddedFont::drawString(renderer, "-- CONTROLES DE PARAMETROS FISICOS --", x, y, scale, {200, 200, 200, 255});
        y += 14.0f;

        ss.str("");
        ss << "[1/2] Sensibilidade/Arrasto: " << std::setprecision(5) << physics.velocityInfluence;
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {230, 240, 255, 255});
        y += 13.0f;

        ss.str("");
        ss << "[3/4] Rigidez da Mola (k): " << physics.springK;
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {230, 240, 255, 255});
        y += 13.0f;

        ss.str("");
        ss << "[5/6] Amortecimento (gamma): " << physics.damping << " (Suavidade)";
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {230, 240, 255, 255});
        y += 13.0f;

        ss.str("");
        ss << "[7/8] Forca Inercial (m*a): " << std::setprecision(5) << physics.inertiaInfluence;
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {230, 240, 255, 255});
        y += 13.0f;

        ss.str("");
        ss << "[C] Alternar Cursor (Sistema/Vetor) | [R] Reset";
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {255, 220, 150, 255});
        y += 13.0f;

        ss.str("");
        ss << "[V] Vetores | [T] Rastro | [P] Pivo | [SPACE] Impulso";
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {150, 200, 255, 255});

        // Rodapé de status
        std::string modeText = isOverlay ? "MODO: OVERLAY TRANSPARENTE [F11/O para Janela]" : "MODO: PLAYGROUND INTERATIVO [F11/O para Overlay]";
        EmbeddedFont::drawString(renderer, modeText, 20.0f, winH - 25.0f, 1.0f, {255, 255, 255, 230});
    }
};
