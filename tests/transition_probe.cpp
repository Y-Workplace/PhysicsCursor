// Loaded only by the isolated compositor test; never installed in the user session.
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/pointer/cursor/CursorManager.hpp>
#include <hyprland/src/state/MonitorState.hpp>
#include <hyprland/src/output/Monitor.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/protocols/core/Compositor.hpp>
#include <hyprland/src/protocols/core/Seat.hpp>
#include "cursor-probe.hpp"
#include <dlfcn.h>
#include <fstream>
#include <cstdlib>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

static void snapshot() {
    const char* path = std::getenv("PHYSICS_CURSOR_TEST_PLUGIN");
    const char* output = std::getenv("PHYSICS_CURSOR_TEST_STATE");
    if (!path || !output) return;
    void* handle = dlopen(path, RTLD_LAZY | RTLD_NOLOAD);
    if (!handle) return;
    auto* storage = static_cast<UP<CDynamicCursors>*>(dlsym(handle, "g_pDynamicCursors"));
    if (storage && storage->get()) {
        auto* cursor = storage->get();
        int locks = 0;
        for (auto& monitor : State::monitorState()->monitors())
            locks += Pointer::mgr()->softwareLockedFor(monitor);
        uint64_t outgoingHash = 0;
        if (!cursor->outgoingShapes.empty()) {
            auto texture = cursor->outgoingShapes.back().texture;
            if (texture) outgoingHash = cursorImageFingerprint(texture->dataCopy(), texture->m_size.x,
                texture->m_size.y, unsigned(texture->m_size.x) * 4);
        }
        auto tex = Pointer::mgr()->getCurrentCursorTexture();
        auto surf = Pointer::mgr()->currentCursorImage().surface.lock();
        size_t pixels = surf ? CCursorSurfaceRole::cursorPixelData(surf->resource()).size() : 0;
        std::ofstream out(output);
        out << "{\"active\":" << (cursor->transitionSoftware ? "true" : "false")
            << ",\"layers\":" << cursor->outgoingShapes.size()
            << ",\"locks\":" << locks
            << ",\"angle\":" << cursor->resultShown.rotation
            << ",\"zoom\":" << cursor->resultShown.scale
            << ",\"outgoing_hash\":" << outgoingHash
            << ",\"format\":" << (tex ? tex->m_drmFormat : 0)
            << ",\"width\":" << (tex ? tex->m_size.x : 0)
            << ",\"height\":" << (tex ? tex->m_size.y : 0)
            << ",\"pixels\":" << pixels
            << ",\"client_shape\":" << cursor->clientImage.shape
            << ",\"client_hash\":" << cursor->clientImage.fingerprint
            << ",\"theme_frames\":" << cursor->clientThemeFrames.size()
            << ",\"surface\":" << (Pointer::mgr()->currentCursorImage().surface ? "true" : "false")
            << ",\"has_cursor\":" << (Pointer::mgr()->hasCursor() ? "true" : "false") << "}";
    }
    dlclose(handle);
}

static int probe(lua_State* L) {
    const char* shape = luaL_checkstring(L, 1);
    lua_pushstring(L, shape);
    lua_pushnumber(L, luaL_optnumber(L, 2, 160));
    lua_pushnumber(L, luaL_optnumber(L, 3, 160));
    lua_pushcclosure(L, [](lua_State* state) -> int {
        const std::string name = lua_tostring(state, lua_upvalueindex(1));
        if (name == "warp") {
            auto focused = Desktop::focusState()->surface();
            Pointer::mgr()->warpTo({lua_tonumber(state, lua_upvalueindex(2)), lua_tonumber(state, lua_upvalueindex(3))});
            g_pInputManager->refocus();
            g_pInputManager->simulateMouseMovement();
            if (focused) {
                g_pSeatManager->setPointerFocus(focused, {100, 100});
                g_pSeatManager->sendPointerMotion(0, {100, 100});
            }
        }
        else if (name != "state") {
            Pointer::mgr()->warpTo({lua_tonumber(state, lua_upvalueindex(2)), lua_tonumber(state, lua_upvalueindex(3))});
            Pointer::Cursor::mgr()->setCursorFromName(name);
        }
        snapshot();
        return 0;
    }, 3);
    return 1;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    HyprlandAPI::addLuaFunction(handle, "physics_cursor_test", "shape", probe);
    return {"physics-cursor-test", "Isolated cursor transition probe", "PhysicsCursor", "test"};
}
APICALL EXPORT void PLUGIN_EXIT() {}
APICALL EXPORT std::string PLUGIN_API_VERSION() { return HYPRLAND_API_VERSION; }
