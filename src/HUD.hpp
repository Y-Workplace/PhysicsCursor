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
    std::string buildStatus;
    void render(SDL_Renderer* renderer, const PhysicsEngine& physics, const CursorRenderer& cursorRend,
                int winW, int winH, bool isOverlay, bool daemonActive = false) {
        if (!visible) return;
        (void)winW;

        SDL_FRect panelRect = { 15.0f, 15.0f, 470.0f, 385.0f };
        SDL_SetRenderDrawColor(renderer, 15, 20, 30, 220);
        SDL_RenderFillRect(renderer, &panelRect);

        SDL_SetRenderDrawColor(renderer, 70, 90, 120, 240);
        SDL_RenderRect(renderer, &panelRect);

        float x = panelRect.x + 12.0f;
        float y = panelRect.y + 12.0f;
        float scale = 1.0f;

        // Title
        std::string title = "=== CURSOR PHYSICS (CACHYOS/WAYLAND) ===";
        EmbeddedFont::drawString(renderer, title, x, y, scale, {0, 220, 255, 255});
        y += 18.0f;

        SDL_SetRenderDrawColor(renderer, 50, 70, 95, 200);
        SDL_RenderLine(renderer, x, y, x + panelRect.w - 24.0f, y);
        y += 8.0f;

        // Active cursor type
        std::ostringstream ss;
        ss << "[C] Active Cursor: " << (cursorRend.useSystemCursor ? ("SYSTEM (" + cursorRend.sysCursor.themeName + ")") : "VECTOR");
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {255, 215, 0, 255});
        y += 14.0f;

        // Pivot point
        ss.str("");
        ss << std::fixed << std::setprecision(1);
        ss << "Hotspot Pivot: (" << physics.pivotPos.x << ", " << physics.pivotPos.y << ") px";
        if (cursorRend.useSystemCursor && cursorRend.sysCursor.valid) {
            ss << " [Hotspot: " << cursorRend.sysCursor.xhot << "," << cursorRend.sysCursor.yhot << "]";
        }
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {255, 100, 100, 255});
        y += 14.0f;

        // Velocity and acceleration
        float speed = physics.velocity.length();
        float accel = physics.acceleration.length();
        ss.str("");
        ss << "Velocity v: " << speed << " px/s (vx: " << physics.velocity.x << ")";
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {46, 204, 113, 255});
        y += 14.0f;

        ss.str("");
        ss << "Acceleration a: " << accel << " px/s^2 (ax: " << physics.acceleration.x << ")";
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {243, 156, 18, 255});
        y += 14.0f;

        // Symmetric angular sway
        float defDeg = physics.angle * (180.0f / (float)M_PI);
        ss.str("");
        std::string dirStr;
        dirStr = (defDeg > 0.5f ? "RIGHT >>" : (defDeg < -0.5f ? "<< LEFT" : "CENTER"));
        ss << "Angular Sway: " << defDeg << " deg (" << dirStr << ")";
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {255, 230, 80, 255});
        y += 14.0f;

        ss.str("");
        ss << "Angular Vel. omega: " << physics.angularVelocity << " rad/s";
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {200, 220, 240, 255});
        y += 14.0f;

        // Background daemon status
        ss.str("");
        ss << "System Daemon: " << (daemonActive ? "ACTIVE (IPC CONNECTED)" : "STANDALONE");
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, daemonActive ? SDL_Color{0, 255, 180, 255} : SDL_Color{180, 180, 200, 255});
        y += 18.0f;

        SDL_SetRenderDrawColor(renderer, 50, 70, 95, 200);
        SDL_RenderLine(renderer, x, y, x + panelRect.w - 24.0f, y);
        y += 8.0f;

        std::string paramHeader = "-- PHYSICS PARAMETER CONTROLS --";
        EmbeddedFont::drawString(renderer, paramHeader, x, y, scale, {200, 200, 200, 255});
        y += 14.0f;

        ss.str("");
        ss << "[1/2] Sensitivity/Drag: " << std::setprecision(5) << physics.velocityInfluence;
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {230, 240, 255, 255});
        y += 13.0f;

        ss.str("");
        ss << "[3/4] Spring Stiffness (k): " << physics.springK;
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {230, 240, 255, 255});
        y += 13.0f;

        ss.str("");
        ss << "[5/6] Damping (gamma): " << physics.damping << " (Smoothness)";
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {230, 240, 255, 255});
        y += 13.0f;

        ss.str("");
        ss << "[7/8] Inertia Force (m*a): " << std::setprecision(5) << physics.inertiaInfluence;
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {230, 240, 255, 255});
        y += 13.0f;

        ss.str("");
        ss << "[C] Cursor | [R] Reset";
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {255, 220, 150, 255});
        y += 13.0f;

        ss.str("");
        ss << "[V] Vectors | [T] Trail | [P] Pivot | [SPACE] Impulse";
        EmbeddedFont::drawString(renderer, ss.str(), x, y, scale, {150, 200, 255, 255});

        y += 16.0f;
        EmbeddedFont::drawString(renderer,
            "[F9] Save, build and test on system",
            x, y, scale, {255, 220, 150, 255});
        y += 14.0f;
        EmbeddedFont::drawString(renderer, buildStatus.substr(0, 55), x, y, scale, {150, 220, 200, 255});

        // Status footer
        std::string modeText;
        modeText = isOverlay ? "MODE: TRANSPARENT OVERLAY [F11/O for Window]" : "MODE: INTERACTIVE PLAYGROUND [F11/O for Overlay]";
        EmbeddedFont::drawString(renderer, modeText, 20.0f, winH - 25.0f, 1.0f, {255, 255, 255, 230});
    }
};
