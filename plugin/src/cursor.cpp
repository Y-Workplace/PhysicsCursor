#include <any>    // IWYU pragma: keep
#include <chrono> // IWYU pragma: keep
#include <ranges> // IWYU pragma: keep
#define private public
#include <hyprland/src/pointer/cursor/CursorManager.hpp>
#include <hyprland/src/pointer/PointerManager.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/Compositor.hpp>
#undef private

#include <hyprcursor/hyprcursor.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/protocols/core/Compositor.hpp>
#include <hyprland/src/state/MonitorState.hpp>
#include <hyprland/src/protocols/core/Seat.hpp>
#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/helpers/math/Math.hpp>
#include <hyprlang.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
#include <cmath>
#include <cstdlib>
#include <climits>
#include <gbm.h>
#include <numbers>

#include "cursor.hpp"
#include "render/renderer.hpp"
#include "config/ConfigManager.hpp"
#include "mode/Mode.hpp"
#include "../../src/CursorEffects.hpp"
#include "render/CursorPassElement.hpp"

void tickRaw(SP<CEventLoopTimer> self, void* data) {
    if (g_pDynamicCursors)
        g_pDynamicCursors->onTick(Pointer::mgr().get());

    const int TIMEOUT = g_pHyprRenderer->m_mostHzMonitor && g_pHyprRenderer->m_mostHzMonitor->m_refreshRate > 0 ? 1000.0 / g_pHyprRenderer->m_mostHzMonitor->m_refreshRate : 16;
    self->updateTimeout(std::chrono::milliseconds(TIMEOUT));
}

CDynamicCursors::CDynamicCursors() {
    shapeName = g_pHyprRenderer->m_lastCursorData.name;
    indexClientThemeFrames();
    cursorChanged = Pointer::mgr()->m_events.cursorChanged.listen([this] { onCursorImageChanged(); });
    onCursorImageChanged();
    this->tick = SP<CEventLoopTimer>(new CEventLoopTimer(std::chrono::microseconds(500), tickRaw, nullptr));
    g_pEventLoopManager->addTimer(this->tick);
}

CDynamicCursors::~CDynamicCursors() {
    cursorChanged.reset();
    // stop and deallocate timer
    g_pEventLoopManager->removeTimer(this->tick);
    this->tick.reset();

    endTransition();

    // release software lock
    if (zoomSoftware) {
        Pointer::mgr()->unlockSoftwareAll();
        zoomSoftware = false;
    }
}

/*
Reimplements rendering of the software cursor.
Is also largely identical to hyprlands impl, but uses our custom rendering to rotate the cursor.
*/
void CDynamicCursors::renderSoftware(Pointer::CPointerManager* pointers, PHLMONITOR pMonitor, const Time::steady_tp& now, CRegion& damage, std::optional<Vector2D> overridePos,
                                     bool screencopy, bool forceRender) {
    if (!pointers->hasCursor())
        return;

    auto state = pointers->stateFor(pMonitor);
    auto zoom  = resultShown.scale;

    if (!state->hardwareFailed && state->softwareLocks == 0 && !screencopy) {
        if (pointers->m_currentCursorImage.surface)
            pointers->m_currentCursorImage.surface->resource()->frame(now);

        return;
    }

    // don't render cursor on screencopy if using sw cursors
    // otherwise we draw the cursor again for screencopy when using sw cursors
    // unless this is toplevel capture and we *actually* have to force render cursors
    if (screencopy && !forceRender && (state->hardwareFailed || state->softwareLocks != 0))
        return;

    auto box = state->box.copy();
    if (overridePos.has_value()) {
        box.x = overridePos->x;
        box.y = overridePos->y;

        box.translate(-pointers->m_currentCursorImage.hotspot);
    }

    auto texture = pointers->getCurrentCursorTexture();
    bool nearest = false;
    auto renderHotspot = pointers->m_currentCursorImage.hotspot * zoom;

    if (zoom > 1) {
        // this first has to undo the hotspot transform from getCursorBoxGlobal
        box.x += pointers->m_currentCursorImage.hotspot.x;
        box.y += pointers->m_currentCursorImage.hotspot.y;

        auto high = highres.getTexture();

        if (high) {
            texture  = high;
            auto buf = highres.getBuffer();

            // we calculate a more accurate hotspot location if we have bigger shapes
            renderHotspot = {
                (buf->m_hotspot.x / buf->size.x) * pointers->m_currentCursorImage.size.x * zoom,
                (buf->m_hotspot.y / buf->size.y) * pointers->m_currentCursorImage.size.y * zoom};
            box.translate(-renderHotspot);

            // only use nearest-neighbour if magnifying over size
            nearest = CONFIG(highresNearest) == 2 && pointers->m_currentCursorImage.size.x * zoom > buf->size.x;

        } else {
            box.x -= pointers->m_currentCursorImage.hotspot.x * zoom;
            box.y -= pointers->m_currentCursorImage.hotspot.y * zoom;

            nearest = CONFIG(highresNearest);
        }
    }

    if (!texture)
        return;

    box.w *= zoom;
    box.h *= zoom;

    box.scale(pMonitor->m_scale);
    box.x = std::round(box.x);
    box.y = std::round(box.y);

    const bool animating = transitionSoftware;
    const double elapsed = animating ? transitionElapsed() : transitionDuration;
    const auto entering = animating ? cursorTransitionFrame(elapsed, transitionDuration, true) : CursorTransitionFrame{};
    const auto pivot = box.pos() + renderHotspot * pMonitor->m_scale;
    const auto submit = [&](SP<Render::ITexture> tex, Vector2D size, Vector2D hotspot,
                            CursorTransitionFrame frame, bool nearestFilter) {
        if (!tex || frame.opacity <= .001) return;
        size *= frame.scale;
        hotspot *= frame.scale;
        CCursorPassElement::SRenderData data;
        data.tex = tex;
        data.box = CBox{pivot - hotspot, size};
        data.box.rot = resultShown.rotation + frame.rotation;
        data.hotspot = hotspot;
        data.nearest = nearestFilter;
        data.opacity = frame.opacity;
        data.stretchAngle = resultShown.stretch.angle;
        data.stretchMagnitude = resultShown.stretch.magnitude;
        g_pHyprRenderer->m_renderPass.add(makeUnique<CCursorPassElement>(data));
    };
    for (const auto& old : outgoingShapes) {
        submit(old.texture, old.size * zoom * pMonitor->m_scale,
               old.hotspot * zoom * pMonitor->m_scale,
               cursorTransitionFrame(elapsed, transitionDuration, false, old.start), CONFIG(highresNearest));
    }
    submit(texture, box.size(), renderHotspot * pMonitor->m_scale, entering, nearest);

    if (pointers->m_currentCursorImage.surface)
        pointers->m_currentCursorImage.surface->resource()->frame(now);
}

/*
This function implements damaging the screen such that the software cursor is drawn.
It is largely identical to hyprlands implementation, but expands the damage region, to accommodate various rotations.
*/
void CDynamicCursors::damageSoftware(Pointer::CPointerManager* pointers) {

    const auto zoom = resultShown.scale;
    auto size = pointers->m_currentCursorImage.size / pointers->m_currentCursorImage.scale;
    double radius = size.size();
    for (const auto& old : outgoingShapes)
        radius = std::max(radius, old.size.size());
    radius *= zoom * 1.25 * std::max({1.0, resultShown.stretch.magnitude.x, resultShown.stretch.magnitude.y});
    CBox b{pointers->m_pointerPos - Vector2D{radius, radius}, Vector2D{2 * radius, 2 * radius}};

    static auto PNOHW = CConfigValue<Hyprlang::INT>("cursor:no_hardware_cursors");

    for (auto& mw : pointers->m_monitorStates) {
        if (mw->monitor.expired())
            continue;

        if ((mw->softwareLocks > 0 || mw->hardwareFailed || *PNOHW) && b.overlaps({mw->monitor->m_position, mw->monitor->m_size})) {
            g_pHyprRenderer->damageBox(b, mw->monitor->shouldSkipScheduleFrameOnMouseEvent());
        }
    }
}

/*
This function reimplements the hardware cursor buffer drawing.
It is largely copied from hyprland, but adjusted to allow the cursor to be rotated.
*/
SP<Aquamarine::IBuffer> CDynamicCursors::renderHardware(Pointer::CPointerManager* pointers, SP<Pointer::CPointerManager::SMonitorPointerState> state,
                                                        SP<Render::ITexture> texture) {
    auto output = state->monitor->m_output;

    auto maxSize = output->cursorPlaneSize();
    auto zoom    = resultShown.scale;

    auto cursorSize     = pointers->m_currentCursorImage.size * zoom;
    int  cursorDiagonal = cursorSize.size();
    auto cursorPadding  = Vector2D{cursorDiagonal, cursorDiagonal};
    auto targetSize     = cursorSize + cursorPadding * 2;

    if (maxSize == Vector2D{})
        return nullptr;

    if (maxSize != Vector2D{-1, -1}) {
        if (targetSize.x > maxSize.x || targetSize.y > maxSize.y) {
            Log::logger->log(Log::TRACE, "hardware cursor too big! {} > {}", pointers->m_currentCursorImage.size, maxSize);
            return nullptr;
        }
    } else {
        maxSize = targetSize;
        if (maxSize.x < 16 || maxSize.y < 16)
            maxSize = {16, 16}; // fix some annoying crashes in nest
    }

    if (!state->monitor->m_cursorSwapchain || maxSize != state->monitor->m_cursorSwapchain->currentOptions().size ||
        state->monitor->m_cursorSwapchain->currentOptions().length != 3) {

        if (!state->monitor->m_cursorSwapchain) {
            auto backend = state->monitor->m_output->getBackend();
            auto primary = backend->getPrimary();

            state->monitor->m_cursorSwapchain = Aquamarine::CSwapchain::create(state->monitor->m_output->getBackend()->preferredAllocator(), primary ? primary.lock() : backend);
        }

        auto options = state->monitor->m_cursorSwapchain->currentOptions();
        options.size = maxSize;
        // we still have to create a triple buffering swapchain, as we seem to be running into some sort of race condition
        // or something. I'll continue debugging this when I find some energy again, I've spent too much time here already.
        options.length   = 3;
        options.scanout  = true;
        options.cursor   = true;
        options.multigpu = state->monitor->m_output->getBackend()->preferredAllocator()->drmFD() != g_pCompositor->m_drm.fd;
        // We do not set the format. If it's unset (DRM_FORMAT_INVALID) then the swapchain will pick for us,
        // but if it's set, we don't wanna change it.

        if (!state->monitor->m_cursorSwapchain->reconfigure(options)) {
            Log::logger->log(Log::TRACE, "Failed to reconfigure cursor swapchain");
            return nullptr;
        }
    }

    // if we already rendered the cursor, revert the swapchain to avoid rendering the cursor over
    // the current front buffer
    // this flag will be reset in the preRender hook, so when we commit this buffer to KMS
    // see https://github.com/hyprwm/Hyprland/commit/4c3b03516209a49244a8f044143c1162752b8a7a
    // this is however still not enough, see above
    if (state->cursorRendered)
        state->monitor->m_cursorSwapchain->rollback();

    state->cursorRendered = true;

    auto buf = state->monitor->m_cursorSwapchain->next(nullptr);
    if (!buf) {
        Log::logger->log(Log::TRACE, "Failed to acquire a buffer from the cursor swapchain");
        return nullptr;
    }

    CRegion damage = {0, 0, INT16_MAX, INT16_MAX};

    g_pHyprRenderer->m_renderData.pMonitor = state->monitor;
    auto RBO                               = g_pHyprRenderer->getOrCreateRenderbuffer(buf, state->monitor->m_cursorSwapchain->currentOptions().format);

    // we just fail if we cannot create a render buffer, this will force hl to render software cursors, which we support
    if (!RBO)
        return nullptr;

    RBO->bind();

    CRegion damageRegion = {0, 0, INT_MAX, INT_MAX};
    g_pHyprRenderer->beginFullFakeRender(state->monitor.lock(), damageRegion, RBO->getFB());
    g_pHyprRenderer->startRenderPass();

    if (CONFIG(hwDebug))
        g_pHyprRenderer->draw(CClearPassElement::SClearData{CHyprColor{rand() / float(RAND_MAX), rand() / float(RAND_MAX), rand() / float(RAND_MAX), 1.F}});
    else
        g_pHyprRenderer->draw(CClearPassElement::SClearData{{0.F, 0.F, 0.F, 0.F}});

    CBox   xbox      = {cursorPadding, Vector2D{Pointer::mgr()->m_currentCursorImage.size / Pointer::mgr()->m_currentCursorImage.scale * state->monitor->m_scale * zoom}.round()};
    Mat3x3 transform = toTransform(xbox, resultShown.rotation, Pointer::mgr()->m_currentCursorImage.hotspot * state->monitor->m_scale * zoom, resultShown.stretch.angle,
                                   resultShown.stretch.magnitude);

    drawCursor(transform, texture, xbox, damageRegion, zoom > 1 && CONFIG(highresNearest));

    g_pHyprRenderer->endRender();
    g_pHyprRenderer->m_renderData.pMonitor.reset();

    return buf;
}

/*
Implements the hardware cursor setting.
It is also mostly the same as stock hyprland, but with the hotspot translated more into the middle.
*/
bool CDynamicCursors::setHardware(Pointer::CPointerManager* pointers, SP<Pointer::CPointerManager::SMonitorPointerState> state, SP<Aquamarine::IBuffer> buf) {
    if (!(state->monitor->m_output->getBackend()->capabilities() & Aquamarine::IBackendImplementation::eBackendCapabilities::AQ_BACKEND_CAPABILITY_POINTER))
        return false;

    if (!state->monitor->m_cursorSwapchain)
        return false;

    // we need to transform the hotspot manually as we need to indent it by the padding
    int      diagonal = pointers->m_currentCursorImage.size.size();
    Vector2D padding  = {diagonal, diagonal};

    const auto HOTSPOT = CBox{((pointers->m_currentCursorImage.hotspot * state->monitor->m_scale) + padding) * resultShown.scale, {0, 0}}
                             .transform(Math::wlTransformToHyprutils(Math::invertTransform(state->monitor->m_transform)),
                                        state->monitor->m_cursorSwapchain->currentOptions().size.x, state->monitor->m_cursorSwapchain->currentOptions().size.y)
                             .pos();

    Log::logger->log(Log::TRACE, "[pointer] hw transformed hotspot for {}: {}", state->monitor->m_name, HOTSPOT);

    if (!state->monitor->m_output->setCursor(buf, HOTSPOT))
        return false;

    state->cursorFrontBuffer = buf;

    if (!state->monitor->shouldSkipScheduleFrameOnMouseEvent())
        state->monitor->scheduleFrame(Aquamarine::IOutput::AQ_SCHEDULE_CURSOR_SHAPE);

    state->monitor->m_scanoutNeedsCursorUpdate = true;

    return true;
}

/*
Handles cursor move events.
*/
void CDynamicCursors::onCursorMoved(Pointer::CPointerManager* pointers) {
    if (!pointers->hasCursor())
        return;

    const auto CURSORBOX = pointers->getCursorBoxGlobal();
    bool       recalc    = false;

    for (auto& m : State::monitorState()->monitors()) {
        auto state = pointers->stateFor(m);

        state->box = pointers->getCursorBoxLogicalForMonitor(state->monitor.lock());

        auto CROSSES = !m->logicalBox().intersection(CURSORBOX).empty();

        if (!CROSSES && state->cursorFrontBuffer) {
            Log::logger->log(Log::TRACE, "onCursorMoved for output {}: cursor left the viewport, removing it from the backend", m->m_name);
            pointers->setHWCursorBuffer(state, nullptr);
            continue;
        } else if (CROSSES && !state->cursorFrontBuffer) {
            Log::logger->log(Log::TRACE, "onCursorMoved for output {}: cursor entered the output, but no front buffer, forcing recalc", m->m_name);
            recalc = true;
        }

        if (!state->entered)
            continue;

        Hyprutils::Utils::CScopeGuard x([m] { m->onCursorMovedOnMonitor(); });

        if (state->hardwareFailed)
            continue;

        const auto CURSORPOS = pointers->getCursorPosForMonitor(m);
        m->m_output->moveCursor(CURSORPOS);

        state->monitor->m_scanoutNeedsCursorUpdate = true;
    }

    if (recalc)
        pointers->updateCursorBackend();

    // ignore warp
    if (!isMove && CONFIG(ignoreWarps)) {
        auto mode = this->currentMode();
        if (mode)
            mode->warp(lastPos, pointers->m_pointerPos);

        if (CONFIG(shakeEnabled))
            shake.warp(lastPos, pointers->m_pointerPos);
    }

    calculate(MOVE);

    isMove  = false;
    lastPos = pointers->m_pointerPos;
}

void CDynamicCursors::setShape(const std::string& shape) {
    if (shape != shapeName && !shapeName.empty() && CONFIG(transitionEnabled))
        beginTransition();
    shapeName = shape;
    g_pConfigHandler->m_shapeRules->activate(shape);
    highres.loadShape(shape);
}

void CDynamicCursors::unsetShape() {
    // Capture a named cursor before the client surface replaces it. Repeated
    // set_cursor calls on client surfaces are observed through cursorChanged.
    if (shapeName != "clientside" && !shapeName.empty() && CONFIG(transitionEnabled))
        beginTransition();
    shapeName = "clientside";
    g_pConfigHandler->m_shapeRules->activate(shapeName);
    highres.loadShape(shapeName);
}

void CDynamicCursors::updateTheme() {
    endTransition();
    clientImage = {};
    indexClientThemeFrames();
    highres.update();
}

/*
Handle cursor tick events.
*/
void CDynamicCursors::onTick(Pointer::CPointerManager* pointers) {
    if (transitionSoftware && (!g_pConfigHandler->isEnabled() || !CONFIG(transitionEnabled) ||
                               !pointers->hasCursor() || transitionElapsed() >= transitionDuration))
        endTransition();
    if (g_pConfigHandler->isEnabled())
        calculate(TICK);
}

IMode* CDynamicCursors::currentMode() {
    switch (CONFIG(mode)) {
        case MODE_ROTATE: return &rotate;
        case MODE_TILT: return &tilt;
        case MODE_STRETCH: return &stretch;
        default: return nullptr;
    }
}

void CDynamicCursors::calculate(EModeUpdate type) {

    IMode* mode = currentMode();
    const auto pos = Pointer::mgr()->m_pointerPos;
    float daemonAngle = 0.0f;
    const bool daemonActive = bridge.update(pos.x, pos.y, daemonAngle);

    // calculate angle and zoom
    if (mode) {
        // reset mode if it has changed
        if (mode != lastMode)
            mode->reset();

        if (mode->strategy() == type)
            resultMode = mode->update(Pointer::mgr()->m_pointerPos);
    } else
        resultMode = SModeResult();

    lastMode = mode;

    if (CONFIG(shakeEnabled)) {
        if (type == TICK)
            resultShake = shake.update(Pointer::mgr()->m_pointerPos);

    } else
        resultShake = 1;

    // Apply the daemon angle after shake handling. Scaling must never clear
    // tilt physics, and presentation must not reset the mode's cached state.
    auto result = composeCursorEffects(resultMode, resultShake, CONFIG(shakeEffects),
                                       mode == &tilt, daemonActive, daemonAngle);

    if (transitionSoftware || resultShown.hasDifference(&result, CONFIG(threshold) * (std::numbers::pi / 180.0), 0.01, 0.01)) {
        resultShown = result;
        resultShown.clamp(CONFIG(threshold) * (std::numbers::pi / 180.0), 0.01, 0.01); // clamp low values so it is rendered pixel-perfectly when no effect

        // lock software cursors if zooming
        if (resultShown.scale > 1) {
            if (!zoomSoftware) {
                Pointer::mgr()->lockSoftwareAll();
                zoomSoftware = true;
            }
        } else {
            if (zoomSoftware) {
                // damage so it is cleared properly
                Pointer::mgr()->damageIfSoftware();

                Pointer::mgr()->unlockSoftwareAll();
                zoomSoftware = false;
            }
        }

        // damage software and change hardware cursor shape
        Pointer::mgr()->damageIfSoftware();

        bool entered = false;

        for (auto& m : State::monitorState()->monitors()) {
            auto state = Pointer::mgr()->stateFor(m);

            if (state->entered)
                entered = true;
            if (state->hardwareFailed || !state->entered)
                continue;

            Pointer::mgr()->attemptHardwareCursor(state);
        }

        // there should always be one monitor entered
        // this fixes an issue where the cursor shape would not properly update after change
        if (!entered) {
            Log::logger->log(Log::INFO, "[dynamic-cursors] updating because none entered");
            Pointer::mgr()->recheckEnteredOutputs();
            Pointer::mgr()->updateCursorBackend();
        }
    }
}

void CDynamicCursors::setMove() {
    isMove = true;
}

void CDynamicCursors::dispatchMagnify(std::optional<int> duration, std::optional<float> size) {
    if (!CONFIG(shakeEnabled))
        return;

    shake.force(duration, size);
}


double CDynamicCursors::transitionElapsed() const {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - transitionStart).count();
}

void CDynamicCursors::indexClientThemeFrames() {
    clientThemeFrames.clear();
    auto* manager = Pointer::Cursor::mgr()->m_xcursor.get();
    if (!manager) return;
    for (const auto& cursor : manager->m_cursors) {
        if (!cursor || cursor->images.empty()) continue;
        const auto fingerprint = [](const SXCursorImage& frame) {
            return cursorImageFingerprint(
                {reinterpret_cast<const uint8_t*>(frame.pixels.data()), frame.pixels.size() * 4},
                frame.size.x, frame.size.y, unsigned(frame.size.x) * 4);
        };
        const auto identity = fingerprint(cursor->images.front());
        if (!identity) continue;
        for (const auto& frame : cursor->images)
            if (auto hash = fingerprint(frame)) clientThemeFrames.try_emplace(hash, identity);
    }
}

CDynamicCursors::ShapeLayer CDynamicCursors::snapshotClientCursor() {
    ShapeLayer old;
    if (clientImage.pixels.empty()) return old;
    // The client's live texture may be updated in place on the next commit.
    old.texture = g_pHyprRenderer->createTexture(clientImage.format, clientImage.pixels.data(),
                                                clientImage.stride, clientImage.size, true);
    old.size = clientImage.logicalSize;
    old.hotspot = clientImage.hotspot;
    return old;
}

void CDynamicCursors::onCursorImageChanged() {
    auto* pointers = Pointer::mgr().get();
    const auto& image = pointers->m_currentCursorImage;
    if (!g_pConfigHandler->isEnabled() || !CONFIG(transitionEnabled) || !pointers->hasCursor()) {
        clientImage = {};
        endTransition();
        return;
    }
    if (!image.surface) {
        clientImage = {};
        return; // Named shapes are handled before setCursorFromName replaces them.
    }
    auto surface = image.surface.lock();
    auto resource = surface->resource();
    auto texture = pointers->getCurrentCursorTexture();
    if (!texture || !resource || !resource->m_role || resource->m_role->role() != SURFACE_ROLE_CURSOR) return;
    const auto& pixels = CCursorSurfaceRole::cursorPixelData(resource);
    // set_cursor can precede the first commit carrying SHM pixels. Keep the
    // previous snapshot until that commit instead of treating it as a hide.
    if (pixels.empty()) return;
    const int width = image.size.x, height = image.size.y;
    // Only CPU-backed ARGB cursors can be snapshotted without blocking GPU readback.
    // Hyprland's synchronous surface textures can leave m_drmFormat unset.
    // Prefer the SHM attributes while the committed buffer is still available.
    uint32_t format = texture->m_drmFormat;
    if (resource->m_current.buffer) {
        const auto attrs = resource->m_current.buffer->shm();
        if (!attrs.success) { clientImage = {}; endTransition(); return; }
        format = attrs.format;
    }
    if (!format) format = DRM_FORMAT_ARGB8888; // same fallback as Hyprland's CPU cursor path
    if (width <= 0 || height <= 0 || width > 512 || height > 512 || texture->isDMA() ||
        format != DRM_FORMAT_ARGB8888 || pixels.size() > 4 * 1024 * 1024 || pixels.size() % height) {
        clientImage = {};
        endTransition();
        return;
    }
    const unsigned stride = pixels.size() / height;
    const auto hash = cursorImageFingerprint(pixels, width, height, stride);
    if (!hash) {
        clientImage = {};
        endTransition();
        return;
    }
    const auto match = clientThemeFrames.find(hash);
    const uint64_t shape = match == clientThemeFrames.end() ? 0 : match->second;
    const auto logicalSize = image.size / image.scale;
    const bool geometryChanged = clientImage.logicalSize != logicalSize || clientImage.hotspot != image.hotspot;
    const bool changed = !clientImage.pixels.empty() &&
        ((shape && clientImage.shape && shape != clientImage.shape) ||
         (hash != clientImage.fingerprint && (!shape || !clientImage.shape) &&
          (geometryChanged || clientImage.surface != surface)));
    if (changed) beginTransition(snapshotClientCursor());
    // Unknown same-surface frames are left alone: Wayland supplies no shape name,
    // so animating every commit would restart busy/spinner animations indefinitely.
    clientImage.pixels = pixels;
    clientImage.size = image.size;
    clientImage.logicalSize = logicalSize;
    clientImage.hotspot = image.hotspot;
    clientImage.format = format;
    clientImage.stride = stride;
    clientImage.fingerprint = hash;
    clientImage.shape = shape;
    clientImage.surface = surface;
}

void CDynamicCursors::beginTransition() {
    auto* pointers = Pointer::mgr().get();
    if (!pointers->hasCursor()) { endTransition(); return; }
    if (pointers->m_currentCursorImage.surface) {
        beginTransition(snapshotClientCursor());
        return;
    }
    ShapeLayer old;
    old.texture = pointers->getCurrentCursorTexture();
    old.size = pointers->m_currentCursorImage.size / pointers->m_currentCursorImage.scale;
    old.hotspot = pointers->m_currentCursorImage.hotspot;
    if (resultShown.scale > 1 && highres.getTexture() && highres.getBuffer()) {
        old.texture = highres.getTexture();
        auto buffer = highres.getBuffer();
        old.hotspot = {buffer->m_hotspot.x / buffer->size.x * old.size.x,
                       buffer->m_hotspot.y / buffer->size.y * old.size.y};
    }
    beginTransition(old);
}

void CDynamicCursors::beginTransition(ShapeLayer old) {
    if (!old.texture) return;
    auto* pointers = Pointer::mgr().get();
    const double elapsed = transitionSoftware ? transitionElapsed() : transitionDuration;
    for (auto& layer : outgoingShapes)
        layer.start = cursorTransitionFrame(elapsed, transitionDuration, false, layer.start);
    std::erase_if(outgoingShapes, [](const auto& layer) { return layer.start.opacity < .01; });
    old.start = transitionSoftware ? cursorTransitionFrame(elapsed, transitionDuration, true) : CursorTransitionFrame{};
    outgoingShapes.push_back(old);
    // Retain a small bounded set for interrupted transitions on rapid hover.
    if (outgoingShapes.size() > 4) outgoingShapes.erase(outgoingShapes.begin());
    double opacity = 0;
    for (const auto& layer : outgoingShapes) opacity += layer.start.opacity;
    if (opacity > .001)
        for (auto& layer : outgoingShapes) layer.start.opacity /= opacity;
    transitionDuration = std::clamp<Config::INTEGER>(CONFIG(transitionDuration), 50, 1000) / 1000.0;
    transitionStart = std::chrono::steady_clock::now();
    if (!transitionSoftware) {
        transitionSoftware = true;
        pointers->lockSoftwareAll();
    }
    pointers->damageIfSoftware();
}

void CDynamicCursors::endTransition() {
    if (!transitionSoftware) return;
    // Damage old images before releasing them and restore the hardware path.
    Pointer::mgr()->damageIfSoftware();
    outgoingShapes.clear();
    transitionSoftware = false;
    Pointer::mgr()->unlockSoftwareAll();
    Pointer::mgr()->damageIfSoftware();
}
