#pragma once
#include <SDL3/SDL.h>
#include <string>

// Bitmap font 8x8 básica para renderização de telemetria sem dependências externas
namespace EmbeddedFont {

// Representação de 128 caracteres ASCII (8 bytes por caractere)
// Caracteres comuns (espaço até '~')
extern const unsigned char FONT_DATA[128][8];

inline void drawChar(SDL_Renderer* renderer, char c, float x, float y, float scale, SDL_Color color) {
    unsigned char uc = static_cast<unsigned char>(c);
    if (uc >= 128) uc = '?';
    const unsigned char* glyph = FONT_DATA[uc];
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    
    for (int row = 0; row < 8; ++row) {
        unsigned char b = glyph[row];
        for (int col = 0; col < 8; ++col) {
            if (b & (1 << (7 - col))) {
                if (scale <= 1.0f) {
                    SDL_RenderPoint(renderer, x + col, y + row);
                } else {
                    SDL_FRect r = { x + col * scale, y + row * scale, scale, scale };
                    SDL_RenderFillRect(renderer, &r);
                }
            }
        }
    }
}

inline void drawString(SDL_Renderer* renderer, const std::string& str, float x, float y, float scale, SDL_Color color) {
    float startX = x;
    for (char c : str) {
        if (c == '\n') {
            y += 10.0f * scale;
            x = startX;
            continue;
        }
        drawChar(renderer, c, x, y, scale, color);
        x += 8.0f * scale;
    }
}

} // namespace EmbeddedFont
