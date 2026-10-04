// Controlled client used only inside the nested compositor regression test.
#include <SDL3/SDL.h>
#include <X11/Xcursor/Xcursor.h>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 3 || !SDL_Init(SDL_INIT_VIDEO)) return 1;
    auto* window = SDL_CreateWindow("PhysicsCursor isolated surface test", 640, 480, 0);
    auto* renderer = SDL_CreateRenderer(window, nullptr);
    if (!window || !renderer) { std::cerr << SDL_GetError(); return 2; }
    std::vector<SDL_Cursor*> cursors;
    std::string previous;
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) if (event.type == SDL_EVENT_QUIT) running = false;
        std::ifstream input(argv[1]);
        std::string command; input >> command;
        if (!command.empty() && command != previous) {
            previous = command;
            if (command == "quit") break;
            if (command == "hide") SDL_HideCursor();
            else {
                const auto colon = command.find(':');
                const auto name = command.substr(0, colon);
                const int frame = colon == std::string::npos ? 0 : std::stoi(command.substr(colon + 1));
                const auto file = std::string(argv[2]) + "/cursors/" + name;
                auto* images = XcursorFilenameLoadImages(file.c_str(), 24);
                if (!images || frame >= images->nimage) { std::cerr << "Missing theme frame " << command; return 3; }
                auto* image = images->images[frame];
                // XCursor stores premultiplied pixels; SDL expects straight alpha.
                std::vector<uint32_t> pixels(image->pixels, image->pixels + image->width * image->height);
                for (auto& pixel : pixels) {
                    const auto alpha = pixel >> 24;
                    if (alpha && alpha < 255) {
                        uint32_t straight = alpha << 24;
                        for (int shift : {0, 8, 16}) {
                            auto channel = (((pixel >> shift) & 255) * 255 + alpha / 2) / alpha;
                            straight |= std::min(channel, 255u) << shift;
                        }
                        pixel = straight;
                    }
                }
                auto* surface = SDL_CreateSurfaceFrom(image->width, image->height, SDL_PIXELFORMAT_ARGB8888,
                                                      pixels.data(), image->width * 4);
                auto* cursor = SDL_CreateColorCursor(surface, image->xhot, image->yhot);
                SDL_DestroySurface(surface);
                XcursorImagesDestroy(images);
                if (!cursor || !SDL_SetCursor(cursor)) { std::cerr << SDL_GetError(); return 4; }
                SDL_ShowCursor();
                cursors.push_back(cursor);
            }
        }
        SDL_SetRenderDrawColor(renderer, 40, 50, 65, 255);
        SDL_RenderClear(renderer); SDL_RenderPresent(renderer);
        std::ofstream(std::string(argv[1]) + ".ack") << previous;
        SDL_Delay(5);
    }
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window);
    for (auto* cursor : cursors) SDL_DestroyCursor(cursor);
    SDL_Quit();
}
