#include <SDL3/SDL.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>
#include "PhysicsEngine.hpp"
#include "CursorRenderer.hpp"
#include "SystemCursorLoader.hpp"
#include "BridgeServer.hpp"
#include "HUD.hpp"
#include "BuildAction.hpp"

static std::atomic<bool> g_running{true};

void handleSignal(int signum) {
    (void)signum;
    g_running.store(false);
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    bool isDaemonMode = false;
    bool startInOverlay = false;
    bool forceVector = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--daemon" || arg == "-d") {
            isDaemonMode = true;
        } else if (arg == "--overlay" || arg == "-o") {
            startInOverlay = true;
        } else if (arg == "--vector") {
            forceVector = true;
        }
    }

    BridgeServer bridgeServer;
    if (!bridgeServer.init(isDaemonMode)) {
        std::cerr << "[PhysicsCursor] Failed to initialize the IPC bridge." << std::endl;
        return 1;
    }

    PhysicsEngine physics;

    // ==========================================
    // 1. DAEMON MODE (BACKGROUND, NO WINDOW)
    // ==========================================
    if (isDaemonMode) {
        std::cout << "[PhysicsCursor] Daemon mode active. Publishing physics to Hyprland through SHM (500 Hz)..." << std::endl;
        using Clock = std::chrono::steady_clock;
        constexpr auto interval = std::chrono::microseconds(2000);
        auto lastTime = Clock::now();
        auto nextTick = lastTime;

        while (g_running.load()) {
            const auto now = Clock::now();
            const float dt = std::chrono::duration<float>(now - lastTime).count();
            lastTime = now;

            float x = 0.0f, y = 0.0f;
            if (bridgeServer.getHyprlandPointerPos(x, y))
                physics.setPivotPosition(x, y);

            physics.update(dt);
            bridgeServer.publish(physics.angle);

            nextTick += interval;
            // Resume from the current time after a stall, without a burst of updates.
            if (nextTick <= Clock::now())
                nextTick = Clock::now() + interval;
            std::this_thread::sleep_until(nextTick);
        }

        std::cout << "[PhysicsCursor] Daemon shut down safely." << std::endl;
        return 0;
    }

    // ==========================================
    // 2. INTERACTIVE / GUI MODE (VISUAL SIMULATOR)
    // ==========================================
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::cerr << "Failed to initialize SDL3: " << SDL_GetError() << std::endl;
        return 1;
    }

    int windowWidth = 1280;
    int windowHeight = 720;
    bool isOverlayMode = startInOverlay;

    SDL_WindowFlags winFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_TRANSPARENT;
    if (isOverlayMode) {
        winFlags |= SDL_WINDOW_FULLSCREEN | SDL_WINDOW_BORDERLESS;
    }

    SDL_Window* window = SDL_CreateWindow("PhysicsCursor - Simulation & Playground",
                                          windowWidth, windowHeight, winFlags);

    if (!window) {
        std::cerr << "Failed to create the SDL3 window: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::cerr << "Failed to create the SDL3 renderer: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_HideCursor();

    CursorRenderer cursorRend;
    HUD hud;
    BuildAction buildAction;

    SystemCursorData sysCursorData = SystemCursorLoader::loadSystemCursor(renderer);
    cursorRend.sysCursor = sysCursorData;
    cursorRend.useSystemCursor = sysCursorData.valid && !forceVector;
    cursorRend.cursorScale = 1.0f;

    physics.setPivotPosition(windowWidth * 0.5f, windowHeight * 0.5f);

    uint64_t perfFreq = SDL_GetPerformanceFrequency();
    uint64_t lastCounter = SDL_GetPerformanceCounter();

    bool isLeftMouseDown = false;

    std::cout << "[PhysicsCursor] Playground ready. Press H to toggle the HUD." << std::endl;

    while (g_running.load()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    g_running.store(false);
                    break;

                case SDL_EVENT_MOUSE_MOTION:
                    physics.setPivotPosition(event.motion.x, event.motion.y);
                    break;

                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        isLeftMouseDown = true;
                        physics.applyAngularImpulse(18.0f);
                    }
                    break;

                case SDL_EVENT_MOUSE_BUTTON_UP:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        isLeftMouseDown = false;
                    }
                    break;

                case SDL_EVENT_KEY_DOWN: {
                    SDL_Keycode key = event.key.key;
                    if (key == SDLK_ESCAPE || key == SDLK_Q) {
                        g_running.store(false);
                    } else if (key == SDLK_F9) {
                        buildAction.start(physics);
                    } else if (key == SDLK_C) {
                        cursorRend.useSystemCursor = !cursorRend.useSystemCursor;

                    } else if (key == SDLK_1) {
                        physics.velocityInfluence = std::max(0.0002f, physics.velocityInfluence - 0.0002f);
                    } else if (key == SDLK_2) {
                        physics.velocityInfluence = std::min(0.010f, physics.velocityInfluence + 0.0002f);
                    } else if (key == SDLK_3) {
                        physics.springK = std::max(20.0f, physics.springK - 15.0f);
                    } else if (key == SDLK_4) {
                        physics.springK = std::min(600.0f, physics.springK + 15.0f);
                    } else if (key == SDLK_5) {
                        physics.damping = std::max(2.0f, physics.damping - 2.0f);
                    } else if (key == SDLK_6) {
                        physics.damping = std::min(80.0f, physics.damping + 2.0f);
                    } else if (key == SDLK_7) {
                        physics.inertiaInfluence = std::max(0.00005f, physics.inertiaInfluence - 0.00008f);
                    } else if (key == SDLK_8) {
                        physics.inertiaInfluence = std::min(0.002f, physics.inertiaInfluence + 0.00008f);
                    } else if (key == SDLK_V) {
                        cursorRend.showPhysicsVectors = !cursorRend.showPhysicsVectors;
                    } else if (key == SDLK_P) {
                        cursorRend.showPivotPoint = !cursorRend.showPivotPoint;
                    } else if (key == SDLK_T) {
                        cursorRend.showTrail = !cursorRend.showTrail;
                    } else if (key == SDLK_H) {
                        hud.visible = !hud.visible;
                    } else if (key == SDLK_SPACE) {
                        physics.applyAngularImpulse(30.0f);
                    } else if (key == SDLK_R) {
                        physics.resetToDefault();
                    } else if (key == SDLK_EQUALS || key == SDLK_PLUS) {
                        cursorRend.cursorScale = std::min(3.5f, cursorRend.cursorScale + 0.2f);
                    } else if (key == SDLK_MINUS) {
                        cursorRend.cursorScale = std::max(0.5f, cursorRend.cursorScale - 0.2f);
                    } else if (key == SDLK_F11 || key == SDLK_O) {
                        isOverlayMode = !isOverlayMode;
                        SDL_SetWindowFullscreen(window, isOverlayMode);
                    }
                    break;
                }

                case SDL_EVENT_WINDOW_RESIZED:
                    windowWidth = event.window.data1;
                    windowHeight = event.window.data2;
                    break;

                default:
                    break;
            }
        }

        uint64_t currentCounter = SDL_GetPerformanceCounter();
        float dt = (float)(currentCounter - lastCounter) / (float)perfFreq;
        lastCounter = currentCounter;

        if (dt > 0.05f) dt = 0.05f;

        physics.update(dt);
        bridgeServer.publish(physics.angle);

        SDL_GetWindowSize(window, &windowWidth, &windowHeight);

        if (isOverlayMode) {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
            SDL_RenderClear(renderer);
        } else {
            SDL_SetRenderDrawColor(renderer, 20, 24, 34, 255);
            SDL_RenderClear(renderer);

            SDL_SetRenderDrawColor(renderer, 32, 38, 52, 255);
            const int gridSize = 64;
            for (int gx = 0; gx < windowWidth; gx += gridSize) {
                SDL_RenderLine(renderer, (float)gx, 0.0f, (float)gx, (float)windowHeight);
            }
            for (int gy = 0; gy < windowHeight; gy += gridSize) {
                SDL_RenderLine(renderer, 0.0f, (float)gy, (float)windowWidth, (float)gy);
            }
        }

        cursorRend.render(renderer, physics);

        if (isLeftMouseDown) {
            cursorRend.drawCircle(renderer, physics.pivotPos, 8.0f, {255, 80, 80, 220}, false);
            cursorRend.drawCircle(renderer, physics.pivotPos, 14.0f, {255, 120, 120, 140}, false);
        }

        buildAction.poll();
        hud.buildStatus = buildAction.status;
        hud.render(renderer, physics, cursorRend, windowWidth, windowHeight, isOverlayMode, bridgeServer.isDaemonActive());

        SDL_RenderPresent(renderer);

        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }

    if (cursorRend.sysCursor.texture) {
        SDL_DestroyTexture(cursorRend.sysCursor.texture);
    }

    SDL_ShowCursor();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "[PhysicsCursor] Playground closed." << std::endl;
    return 0;
}
