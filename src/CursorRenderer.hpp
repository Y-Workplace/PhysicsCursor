#pragma once
#include <SDL3/SDL.h>
#include <vector>
#include <cmath>
#include "PhysicsEngine.hpp"
#include "SystemCursorLoader.hpp"

class CursorRenderer {
public:
    bool useSystemCursor = true;    // Se true, usa o cursor real do sistema
    float cursorScale = 1.0f;       // Escala 1.0 = tamanho nativo 1:1 original (ex: 24px)
    bool showPhysicsVectors = true; // Mostrar vetores de velocidade e aceleração
    bool showPivotPoint = true;     // Mostrar ponto do pivô vermelho no primeiro pixel
    bool showTrail = true;          // Mostrar rastro suave inercial
    bool showAngleArc = true;       // Mostrar arco de balanço angular

    SystemCursorData sysCursor;

    struct Vertex2D {
        float x, y;
    };

    const std::vector<Vertex2D> vectorArrowOutline = {
        {  0.0f,   0.0f },    // 0: Ponta (PIVÔ DE EVENTO)
        {  0.0f,  17.0f },    // 1: Aresta esquerda
        {  4.0f,  13.0f },    // 2: Junção da cauda esquerda
        {  7.0f,  19.5f },    // 3: Ponta inferior esquerda da cauda
        {  9.5f,  18.5f },    // 4: Ponta inferior direita da cauda
        {  6.5f,  12.0f },    // 5: Junção da cauda direita
        { 12.0f,  12.0f },    // 6: Canto direito da asa
    };

    static Vector2D rotatePoint(Vector2D p, float angle) {
        float c = std::cos(angle);
        float s = std::sin(angle);
        return {
            p.x * c - p.y * s,
            p.x * s + p.y * c
        };
    }

    void drawLine(SDL_Renderer* renderer, Vector2D from, Vector2D to, SDL_Color color, float width = 1.0f) {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        if (width <= 1.0f) {
            SDL_RenderLine(renderer, from.x, from.y, to.x, to.y);
        } else {
            Vector2D dir = (to - from).normalized();
            Vector2D perp = {-dir.y * (width * 0.5f), dir.x * (width * 0.5f)};
            
            SDL_Vertex verts[4];
            SDL_FColor col = {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f};

            verts[0] = {{from.x + perp.x, from.y + perp.y}, col, {0, 0}};
            verts[1] = {{to.x + perp.x,   to.y + perp.y},   col, {0, 0}};
            verts[2] = {{to.x - perp.x,   to.y - perp.y},   col, {0, 0}};
            verts[3] = {{from.x - perp.x, from.y - perp.y}, col, {0, 0}};

            int indices[6] = {0, 1, 2, 0, 2, 3};
            SDL_RenderGeometry(renderer, nullptr, verts, 4, indices, 6);
        }
    }

    void drawArrowVector(SDL_Renderer* renderer, Vector2D start, Vector2D dir, float scale, SDL_Color color) {
        float len = dir.length();
        if (len < 1.0f) return;

        float clampedLen = std::min(len * scale, 120.0f);
        Vector2D norm = dir.normalized();
        Vector2D end = start + norm * clampedLen;

        drawLine(renderer, start, end, color, 2.0f);

        Vector2D perp = {-norm.y * 6.0f, norm.x * 6.0f};
        Vector2D barb1 = end - norm * 8.0f + perp;
        Vector2D barb2 = end - norm * 8.0f - perp;

        drawLine(renderer, end, barb1, color, 1.8f);
        drawLine(renderer, end, barb2, color, 1.8f);
    }

    void drawCircle(SDL_Renderer* renderer, Vector2D center, float radius, SDL_Color color, bool fill = false) {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        const int segments = 20;
        
        if (fill) {
            std::vector<SDL_Vertex> verts(segments + 1);
            std::vector<int> indices(segments * 3);
            SDL_FColor fcol = {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f};

            verts[0] = {{center.x, center.y}, fcol, {0, 0}};
            for (int i = 0; i < segments; ++i) {
                float a = (i * 2.0f * (float)M_PI) / segments;
                verts[i + 1] = {{center.x + std::cos(a) * radius, center.y + std::sin(a) * radius}, fcol, {0, 0}};
            }

            for (int i = 0; i < segments; ++i) {
                indices[i * 3 + 0] = 0;
                indices[i * 3 + 1] = i + 1;
                indices[i * 3 + 2] = (i + 1 == segments) ? 1 : (i + 2);
            }

            SDL_RenderGeometry(renderer, nullptr, verts.data(), (int)verts.size(), indices.data(), (int)indices.size());
        } else {
            for (int i = 0; i < segments; ++i) {
                float a1 = (i * 2.0f * (float)M_PI) / segments;
                float a2 = ((i + 1) * 2.0f * (float)M_PI) / segments;
                SDL_RenderLine(renderer, 
                    center.x + std::cos(a1) * radius, center.y + std::sin(a1) * radius,
                    center.x + std::cos(a2) * radius, center.y + std::sin(a2) * radius);
            }
        }
    }

    void render(SDL_Renderer* renderer, const PhysicsEngine& physics) {
        Vector2D pivot = physics.pivotPos;
        float angle = physics.angle;

        // 1. Rastro Inercial Suave
        if (showTrail && !physics.trail.empty()) {
            for (const auto& tp : physics.trail) {
                if (tp.alpha <= 0.05f) continue;
                uint8_t a = (uint8_t)(tp.alpha * 80.0f);
                drawCursorInstance(renderer, tp.pos, tp.angle, a, cursorScale);
            }
        }

        // 2. Arco de Balanço Angular
        if (showAngleArc && std::abs(angle) > 0.015f) {
            float rArc = 36.0f * cursorScale;
            float startA = (float)(M_PI * 0.25f);
            float endA = startA + angle;
            if (startA > endA) std::swap(startA, endA);

            SDL_SetRenderDrawColor(renderer, 255, 230, 80, 160);
            const int arcSegs = 12;
            for (int i = 0; i < arcSegs; ++i) {
                float t1 = (float)i / arcSegs;
                float t2 = (float)(i + 1) / arcSegs;
                float a1 = startA + t1 * (endA - startA);
                float a2 = startA + t2 * (endA - startA);
                SDL_RenderLine(renderer, 
                    pivot.x + std::cos(a1) * rArc, pivot.y + std::sin(a1) * rArc,
                    pivot.x + std::cos(a2) * rArc, pivot.y + std::sin(a2) * rArc);
            }
        }

        // 3. Renderização Principal do Cursor Físico
        drawCursorInstance(renderer, pivot, angle, 255, cursorScale);

        // 4. Vetores Físicos
        if (showPhysicsVectors) {
            if (physics.velocity.length() > 10.0f) {
                drawArrowVector(renderer, pivot, physics.velocity, 0.08f, {46, 204, 113, 230});
            }
            if (physics.acceleration.length() > 30.0f) {
                drawArrowVector(renderer, pivot, physics.acceleration, 0.02f, {243, 156, 18, 230});
            }
        }

        // 5. Ponto do Pivô Fixo no Primeiro Pixel (Hotspot de Evento)
        if (showPivotPoint) {
            drawCircle(renderer, pivot, 2.5f, {255, 50, 50, 255}, true);
            drawCircle(renderer, pivot, 4.5f, {255, 255, 255, 220}, false);
        }
    }

private:
    void drawCursorInstance(SDL_Renderer* renderer, Vector2D pivot, float angle, uint8_t alpha, float scale) {
        if (useSystemCursor && sysCursor.valid && sysCursor.texture) {
            SDL_FRect dstRect = {
                pivot.x - sysCursor.xhot * scale,
                pivot.y - sysCursor.yhot * scale,
                (float)sysCursor.width * scale,
                (float)sysCursor.height * scale
            };

            SDL_FPoint center = {
                (float)sysCursor.xhot * scale,
                (float)sysCursor.yhot * scale
            };

            float angleDeg = angle * (180.0f / (float)M_PI);

            SDL_SetTextureAlphaMod(sysCursor.texture, alpha);
            SDL_RenderTextureRotated(renderer, sysCursor.texture, nullptr, &dstRect, angleDeg, &center, SDL_FLIP_NONE);
        } else {
            std::vector<Vector2D> worldVerts(vectorArrowOutline.size());
            for (size_t i = 0; i < vectorArrowOutline.size(); ++i) {
                Vector2D scaled = { vectorArrowOutline[i].x * scale * 1.5f, vectorArrowOutline[i].y * scale * 1.5f };
                Vector2D rot = rotatePoint(scaled, angle);
                worldVerts[i] = pivot + rot;
            }

            const int triIndices[] = {
                0, 1, 2,
                0, 2, 5,
                0, 5, 6,
                2, 3, 4,
                2, 4, 5
            };
            const int numTris = 5;

            SDL_FColor fFill = {1.0f, 1.0f, 1.0f, alpha / 255.0f};
            std::vector<SDL_Vertex> vertices(worldVerts.size());
            for (size_t i = 0; i < worldVerts.size(); ++i) {
                vertices[i] = {{worldVerts[i].x, worldVerts[i].y}, fFill, {0, 0}};
            }

            SDL_RenderGeometry(renderer, nullptr, vertices.data(), (int)vertices.size(), triIndices, numTris * 3);

            for (size_t i = 0; i < worldVerts.size(); ++i) {
                size_t next = (i + 1) % worldVerts.size();
                drawLine(renderer, worldVerts[i], worldVerts[next], {15, 18, 25, alpha}, 1.5f * scale);
            }
        }
    }
};
