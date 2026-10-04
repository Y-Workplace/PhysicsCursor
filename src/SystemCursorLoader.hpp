#pragma once
#include <SDL3/SDL.h>
#include <X11/Xcursor/Xcursor.h>
#include <string>
#include <vector>
#include <iostream>
#include <cstdlib>

struct SystemCursorData {
    SDL_Texture* texture{nullptr};
    int width{0};
    int height{0};
    int xhot{0};
    int yhot{0};
    int nominalSize{24};
    std::string themeName;
    bool valid{false};
};

class SystemCursorLoader {
public:
    static SystemCursorData loadSystemCursor(SDL_Renderer* renderer) {
        SystemCursorData result;

        // 1. Read theme and size from the CachyOS / Hyprland environment
        const char* envTheme = std::getenv("HYPRCURSOR_THEME");
        if (!envTheme || envTheme[0] == '\0') envTheme = std::getenv("XCURSOR_THEME");
        if (!envTheme || envTheme[0] == '\0') envTheme = "Bibata-Modern-Ice";

        int size = 24;
        const char* envSize = std::getenv("HYPRCURSOR_SIZE");
        if (!envSize || envSize[0] == '\0') envSize = std::getenv("XCURSOR_SIZE");
        if (envSize && envSize[0] != '\0') {
            try {
                int s = std::stoi(envSize);
                if (s >= 16 && s <= 128) size = s;
            } catch (...) {}
        }

        result.themeName = envTheme;
        result.nominalSize = size;

        XcursorImage* img = nullptr;

        // 2. Try loading with XcursorLibraryLoadImage
        img = XcursorLibraryLoadImage("default", envTheme, size);
        if (!img) {
            img = XcursorLibraryLoadImage("left_ptr", envTheme, size);
        }

        // 3. If that fails, try direct filesystem paths
        if (!img) {
            const char* home = std::getenv("HOME");
            std::vector<std::string> paths;
            if (home) {
                paths.push_back(std::string(home) + "/.local/share/icons/" + envTheme + "/cursors/default");
                paths.push_back(std::string(home) + "/.local/share/icons/" + envTheme + "/cursors/left_ptr");
                paths.push_back(std::string(home) + "/.icons/" + envTheme + "/cursors/default");
            }
            paths.push_back("/usr/share/icons/" + std::string(envTheme) + "/cursors/default");
            paths.push_back("/usr/share/icons/" + std::string(envTheme) + "/cursors/left_ptr");

            for (const auto& path : paths) {
                FILE* f = fopen(path.c_str(), "rb");
                if (f) {
                    img = XcursorFileLoadImage(f, size);
                    fclose(f);
                    if (img) break;
                }
            }
        }

        // 4. Fall back to the system default theme if no image was found
        if (!img) {
            img = XcursorLibraryLoadImage("default", "default", size);
        }

        if (!img) {
            std::cerr << "[SystemCursorLoader] Could not load the system cursor for theme: "
                      << envTheme << std::endl;
            return result;
        }

        result.width = (int)img->width;
        result.height = (int)img->height;
        result.xhot = (int)img->xhot;
        result.yhot = (int)img->yhot;

        // 5. Create an SDL3 texture using the native XCursor ARGB8888 format
        result.texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STATIC,
            result.width,
            result.height
        );

        if (!result.texture) {
            std::cerr << "[SystemCursorLoader] Failed to create an SDL_Texture for the cursor: "
                      << SDL_GetError() << std::endl;
            XcursorImageDestroy(img);
            return result;
        }

        SDL_SetTextureBlendMode(result.texture, SDL_BLENDMODE_BLEND);

        // Upload pixels to the GPU
        SDL_UpdateTexture(
            result.texture,
            nullptr,
            img->pixels,
            result.width * sizeof(uint32_t)
        );

        XcursorImageDestroy(img);

        result.valid = true;
        std::cout << "[SystemCursorLoader] System cursor loaded successfully!" << std::endl;
        std::cout << "  Theme: " << result.themeName << " | Size: " << result.width << "x" << result.height
                  << " | Original pivot/hotspot: (" << result.xhot << ", " << result.yhot << ")" << std::endl;

        return result;
    }
};
