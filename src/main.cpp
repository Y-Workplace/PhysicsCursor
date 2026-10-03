#include <SDL3/SDL.h>
#include <iostream>
#include <chrono>
#include <thread>
#include "PhysicsEngine.hpp"
#include "CursorRenderer.hpp"
#include "HUD.hpp"

int main(int argc, char* argv[]) {
    std::cout << "[PhysicsCursor] Iniciando aplicacao de fisica de cursor no CachyOS/Wayland..." << std::endl;

    bool startInOverlay = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--overlay" || arg == "-o") {
            startInOverlay = true;
        }
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::cerr << "Erro fatal ao inicializar SDL3: " << SDL_GetError() << std::endl;
        return 1;
    }

    std::cout << "[PhysicsCursor] Driver de video ativo: " << SDL_GetCurrentVideoDriver() << std::endl;

    int windowWidth = 1280;
    int windowHeight = 720;
    bool isOverlayMode = startInOverlay;

    // Criação da janela SDL3 com suporte a transparência e alta taxa de atualização
    SDL_WindowFlags winFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (isOverlayMode) {
        winFlags |= SDL_WINDOW_FULLSCREEN;
    }

    SDL_Window* window = SDL_CreateWindow("Fisica de Cursor - CachyOS (Pivo no Ponto de Evento)", 
                                          windowWidth, windowHeight, winFlags);

    if (!window) {
        std::cerr << "Erro ao criar janela SDL3: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    // Criação do renderizador com suporte a VSync
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::cerr << "Erro ao criar renderizador SDL3: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Configura o cursor nativo do sistema como invisível para que apenas o cursor físico apareça
    SDL_HideCursor();

    PhysicsEngine physics;
    CursorRenderer cursorRend;
    HUD hud;

    // Centraliza o cursor inicialmente
    physics.setPivotPosition(windowWidth * 0.5f, windowHeight * 0.5f);

    uint64_t perfFreq = SDL_GetPerformanceFrequency();
    uint64_t lastCounter = SDL_GetPerformanceCounter();

    bool running = true;
    bool isLeftMouseDown = false;

    std::cout << "[PhysicsCursor] Pronto! Mova o mouse para experimentar a inercia e o balanco angular." << std::endl;
    std::cout << "[PhysicsCursor] O pivo permanece travado no primeiro pixel (ponto de evento do cursor)." << std::endl;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    running = false;
                    break;

                case SDL_EVENT_MOUSE_MOTION:
                    // Atualiza a posição exata do pivô com precisão de float (subpixel)
                    physics.setPivotPosition(event.motion.x, event.motion.y);
                    break;

                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        isLeftMouseDown = true;
                        // Ao clicar, o pivô recebe um pequeno pulso inercial tátil
                        physics.applyAngularImpulse(22.0f);
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
                        running = false;
                    } else if (key == SDLK_1) {
                        physics.mass = std::max(0.2f, physics.mass - 0.2f);
                    } else if (key == SDLK_2) {
                        physics.mass = std::min(8.0f, physics.mass + 0.2f);
                    } else if (key == SDLK_3) {
                        physics.springK = std::max(20.0f, physics.springK - 50.0f);
                    } else if (key == SDLK_4) {
                        physics.springK = std::min(2000.0f, physics.springK + 50.0f);
                    } else if (key == SDLK_5) {
                        physics.damping = std::max(1.0f, physics.damping - 2.0f);
                    } else if (key == SDLK_6) {
                        physics.damping = std::min(80.0f, physics.damping + 2.0f);
                    } else if (key == SDLK_7) {
                        physics.airDrag = std::max(0.0001f, physics.airDrag - 0.0005f);
                    } else if (key == SDLK_8) {
                        physics.airDrag = std::min(0.02f, physics.airDrag + 0.0005f);
                    } else if (key == SDLK_G) {
                        physics.gravityY = (physics.gravityY > 0.0f) ? 0.0f : 9.8f * 15.0f;
                    } else if (key == SDLK_V) {
                        cursorRend.showPhysicsVectors = !cursorRend.showPhysicsVectors;
                    } else if (key == SDLK_P) {
                        cursorRend.showPivotPoint = !cursorRend.showPivotPoint;
                    } else if (key == SDLK_T) {
                        cursorRend.showTrail = !cursorRend.showTrail;
                    } else if (key == SDLK_H) {
                        hud.visible = !hud.visible;
                    } else if (key == SDLK_SPACE) {
                        // Aplica impulso angular de teste
                        physics.applyAngularImpulse(45.0f);
                    } else if (key == SDLK_R) {
                        physics.resetToDefault();
                    } else if (key == SDLK_EQUALS || key == SDLK_PLUS) {
                        cursorRend.cursorScale = std::min(4.0f, cursorRend.cursorScale + 0.2f);
                    } else if (key == SDLK_MINUS) {
                        cursorRend.cursorScale = std::max(0.6f, cursorRend.cursorScale - 0.2f);
                    } else if (key == SDLK_F11 || key == SDLK_O) {
                        // Alternar entre modo janela e tela cheia / overlay
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

        // Cálculo de delta time real com alta precisão
        uint64_t currentCounter = SDL_GetPerformanceCounter();
        float dt = (float)(currentCounter - lastCounter) / (float)perfFreq;
        lastCounter = currentCounter;

        // Limite de segurança de dt para evitar saltos
        if (dt > 0.05f) dt = 0.05f;

        // Atualização da simulação física (equações de movimento, inércia, torque, amortecimento)
        physics.update(dt);

        // Renderização
        SDL_GetWindowSize(window, &windowWidth, &windowHeight);

        if (isOverlayMode) {
            // Modo transparente / translúcido
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
            SDL_RenderClear(renderer);
        } else {
            // Fundo escuro com sutil grade cartesiana para referência espacial
            SDL_SetRenderDrawColor(renderer, 20, 24, 34, 255);
            SDL_RenderClear(renderer);

            // Grade de fundo suave
            SDL_SetRenderDrawColor(renderer, 32, 38, 52, 255);
            const int gridSize = 64;
            for (int gx = 0; gx < windowWidth; gx += gridSize) {
                SDL_RenderLine(renderer, (float)gx, 0.0f, (float)gx, (float)windowHeight);
            }
            for (int gy = 0; gy < windowHeight; gy += gridSize) {
                SDL_RenderLine(renderer, 0.0f, (float)gy, (float)windowWidth, (float)gy);
            }
        }

        // Renderiza o cursor com o pivô físico no ponto de evento
        cursorRend.render(renderer, physics);

        // Feedback de clique no ponto de evento
        if (isLeftMouseDown) {
            cursorRend.drawCircle(renderer, physics.pivotPos, 9.0f, {255, 80, 80, 200}, false);
            cursorRend.drawCircle(renderer, physics.pivotPos, 14.0f, {255, 120, 120, 130}, false);
        }

        // Renderiza o painel HUD de telemetria e controles
        hud.render(renderer, physics, cursorRend, windowWidth, windowHeight, isOverlayMode);

        // Apresenta na tela
        SDL_RenderPresent(renderer);

        // Pequeno sleep para aliviar CPU se VSync não limitar a taxa
        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }

    SDL_ShowCursor();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "[PhysicsCursor] Finalizado com sucesso." << std::endl;
    return 0;
}
