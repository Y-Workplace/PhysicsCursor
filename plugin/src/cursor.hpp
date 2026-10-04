#include <any>    // IWYU pragma: keep
#include <chrono> // IWYU pragma: keep
#define private public
#include <hyprland/src/pointer/PointerManager.hpp>
#undef private

#include <hyprcursor/hyprcursor.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopManager.hpp>
#include <hyprutils/math/Vector2D.hpp>

#include "mode/ModeRotate.hpp"
#include "mode/ModeTilt.hpp"
#include "mode/BridgeClient.hpp"
#include "mode/ModeStretch.hpp"
#include "other/Shake.hpp"
#include "highres.hpp"
#include "../../src/CursorTransition.hpp"
#include <vector>
#include <unordered_map>
#include "../../src/CursorImageIdentity.hpp"

class CDynamicCursors {
  public:
    CDynamicCursors();
    ~CDynamicCursors();

    /* hook on onCursorMoved */
    void onCursorMoved(Pointer::CPointerManager* pointers);
    /* called on tick */
    void onTick(Pointer::CPointerManager* pointers);

    /* hook on renderSoftwareCursorsFor */
    void renderSoftware(Pointer::CPointerManager* pointers, PHLMONITOR pMonitor, const Time::steady_tp& now, CRegion& damage, std::optional<Vector2D> overridePos,
                        bool screencopy, bool forceRender);
    /* hook on damageIfSoftware*/
    void damageSoftware(Pointer::CPointerManager* pointers);
    /* hook on renderHWCursorBuffer */
    SP<Aquamarine::IBuffer> renderHardware(Pointer::CPointerManager* pointers, SP<Pointer::CPointerManager::SMonitorPointerState> state, SP<Render::ITexture> texture);
    /* hook on setHWCursorBuffer */
    bool                    setHardware(Pointer::CPointerManager* pointers, SP<Pointer::CPointerManager::SMonitorPointerState> state, SP<Aquamarine::IBuffer> buf);

    /* hook on setCursorFromName */
    void setShape(const std::string& name);
    /* hook on setCursorSoftware */
    void unsetShape();
    /* hook on updateTheme */
    void updateTheme();

    /* hook on move, indicate that next onCursorMoved is actual move */
    void setMove();

    void dispatchMagnify(std::optional<int> duration, std::optional<float> size);

  private:
    SP<CEventLoopTimer> tick;

    /* hyprcursor handler for highres images */
    CHighresHandler highres;

    // current state of the cursor
    SModeResult resultMode;
    double      resultShake = 1;
    Vector2D    lastPos; // used for warp compensation

    SModeResult resultShown;

    // whether we have already locked software for cursor zoom
    bool zoomSoftware = false;

    struct ShapeLayer {
        SP<Render::ITexture> texture;
        Vector2D size;
        Vector2D hotspot;
        CursorTransitionFrame start;
    };
    std::vector<ShapeLayer> outgoingShapes;
    std::string shapeName;
    std::chrono::steady_clock::time_point transitionStart;
    double transitionDuration = .25;
    bool transitionSoftware = false;
    double transitionElapsed() const;
    void beginTransition();
    void beginTransition(ShapeLayer old);
    void onCursorImageChanged();
    ShapeLayer snapshotClientCursor();
    CHyprSignalListener cursorChanged;
    struct ClientImage {
        std::vector<uint8_t> pixels;
        Vector2D size, logicalSize, hotspot;
        uint32_t format = 0, stride = 0;
        uint64_t fingerprint = 0, shape = 0;
        WP<Desktop::View::CWLSurface> surface;
    } clientImage;
    std::unordered_map<uint64_t, uint64_t> clientThemeFrames;
    void indexClientThemeFrames();
    void endTransition();

    // modes
    CModeRotate  rotate;
    CModeTilt    tilt;
    BridgeClient bridge;
    CModeStretch stretch;

    /* returns the current mode, nullptr if none is selected */
    IMode* currentMode();
    IMode* lastMode = nullptr; // used to reset the mode if it was switched (to prune stale data)

    // shake
    CShake shake;

    /* is set true if a genuine move is being performed, and will be reset to false after onCursorMoved */
    bool isMove = false;

    // calculates the current angle of the cursor, and changes the cursor shape
    void calculate(EModeUpdate type);
};

inline UP<CDynamicCursors> g_pDynamicCursors;
