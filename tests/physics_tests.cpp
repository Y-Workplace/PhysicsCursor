#include "BuildAction.hpp"
#include "CursorEffects.hpp"
#include "CursorTransition.hpp"
#include <cmath>
#include <iostream>
#include <thread>
#include <sstream>

static void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

static void testPhysics() {
    PhysicsEngine physics;
    require(physics.springK == PhysicsDefaults::springK &&
            physics.damping == PhysicsDefaults::damping &&
            physics.velocityInfluence == PhysicsDefaults::velocityInfluence &&
            physics.inertiaInfluence == PhysicsDefaults::inertiaInfluence,
            "Compiled defaults differ from selected parameters");
    physics.setPivotPosition(0, 0);
    physics.update(0.002f);
    float maxAngle = 0;
    for (int i = 0; i < 1000; ++i) {
        // Fast repeated direction changes, followed by a stationary pointer.
        const float x = 160 * std::sin(i * 0.002f * 40);
        physics.setPivotPosition(x, 0);
        physics.update(0.002f);
        require(std::isfinite(physics.angle) && std::isfinite(physics.angularVelocity),
                "Rapid shake destabilized physics");
        require(std::abs(physics.angle) <= physics.maxDeflectionDeg * float(M_PI / 180) + 1e-5f,
                "Rapid shake exceeded deflection limit");
        maxAngle = std::max(maxAngle, std::abs(physics.angle));
    }
    require(maxAngle > 0.01f, "Rapid shake produced no rotation");
    for (int i = 0; i < 15000; ++i) physics.update(0.002f);
    require(std::abs(physics.angle) < 0.002f && std::abs(physics.angularVelocity) < 0.02f,
            "Spring failed to return to rest");
    physics.resetToDefault();
    physics.setPivotPosition(8000, -9000);
    physics.update(0.002f);
    require(physics.velocity.length() == 0, "Reset retained stale pointer position");

    // Integration of the same kick must agree across render refresh rates.
    float results[3]{};
    const int rates[] = {60, 144, 500};
    for (int j = 0; j < 3; ++j) {
        PhysicsEngine engine;
        engine.update(1.f / rates[j]);
        engine.applyAngularImpulse(18);
        for (int i = 0; i < rates[j]; ++i) engine.update(1.f / rates[j]);
        results[j] = engine.angle;
    }
    require(std::abs(results[0] - results[2]) < 0.005f &&
            std::abs(results[1] - results[2]) < 0.005f,
            "Spring response depends on render refresh rate");
}

static void testMagnifiedPhysics() {
    // Minimal rendering result exercises the same composition used by the plugin,
    // without requiring a running compositor for these regression tests.
    struct Result { double rotation = 0; double scale = 1; };
    const Result local{0.25, 1};
    for (const bool effects : {false, true}) {
        for (const double zoom : {1.0, 4.0, 8.0, 3.0, 1.0}) {
            for (const double angle : {0.4, -0.2, 0.05}) {
                const auto result = composeCursorEffects(local, zoom, effects, true, true, angle);
                require(result.rotation == angle && result.scale == zoom,
                        "Magnification suppressed or froze daemon physics");
            }
            const auto fallback = composeCursorEffects(local, zoom, effects, true, false, 0);
            require(fallback.rotation == local.rotation && fallback.scale == zoom,
                    "Magnification suppressed local tilt fallback");
        }
    }
    const auto otherMode = composeCursorEffects(local, 4, false, false, true, 0.4);
    require(otherMode.rotation == 0 && otherMode.scale == 4,
            "Legacy shake suppression changed for non-tilt mode");
    require(local.rotation == 0.25 && local.scale == 1,
            "Presentation overwrote ongoing physics state");
}

static void testShapeTransition() {
    const auto first = cursorTransitionFrame(0, .25, true);
    const auto last = cursorTransitionFrame(.25, .25, true);
    require(first.opacity == 0 && first.scale == .6 && first.rotation < 0,
            "New cursor must enter small, turned and transparent");
    require(last.opacity == 1 && last.scale == 1 && last.rotation == 0,
            "Transition must finish at the native shape and scale");
    const auto gone = cursorTransitionFrame(.25, .25, false);
    require(gone.opacity == 0 && std::abs(gone.scale - .6) < 1e-8 && gone.rotation > 0,
            "Old cursor must fade, shrink and turn away");
    double peak = 1;
    for (int i = 0; i <= 250; ++i) {
        const auto incoming = cursorTransitionFrame(i / 1000., .25, true);
        const auto outgoing = cursorTransitionFrame(i / 1000., .25, false);
        require(std::abs(incoming.opacity + outgoing.opacity - 1) < 1e-8,
                "Transition lost visible opacity");
        require(std::isfinite(incoming.rotation) && incoming.scale > 0,
                "Transition generated invalid transform");
        peak = std::max(peak, incoming.scale);
    }
    require(peak > 1.01 && peak < 1.25, "Missing or excessive elastic overshoot");
    // A reversal retains both previously visible images without a opacity gap.
    const auto a = cursorTransitionFrame(.09, .25, true);
    const auto b = cursorTransitionFrame(.09, .25, false);
    const auto newA = cursorTransitionFrame(.04, .25, false, a);
    const auto newB = cursorTransitionFrame(.04, .25, false, b);
    const auto next = cursorTransitionFrame(.04, .25, true);
    require(std::abs(newA.opacity + newB.opacity + next.opacity - 1) < 1e-8,
            "Interrupted transition lost opacity");
}

static void testBuildExport() {
    namespace fs = std::filesystem;
    char name[] = "/tmp/physics-cursor-export-XXXXXX";
    const auto raw = mkdtemp(name);
    require(raw != nullptr, "Could not create export test directory");
    const fs::path root(raw);
    const auto previous = fs::current_path();
    fs::create_directory(root / "src");
    // Stub only the external build, leaving real saving/fork/log/status behavior.
    std::ofstream(root / "build.sh") << "#!/bin/bash\n[[ $1 == --daemon-only && $2 == --apply ]]\n";
    fs::current_path(root);
    PhysicsEngine engine;
    engine.springK = 230.0f;
    engine.damping = 8.0f;
    engine.velocityInfluence = 0.0042f;
    BuildAction action;
    action.start(engine);
    for (int i = 0; i < 100 && action.status == "Compiling and testing..."; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        action.poll();
    }
    require(action.status == "OK: compiled, tested, daemon active", "Build child failed");
    std::ifstream file(root / "src/PhysicsDefaults.hpp");
    std::stringstream saved;
    saved << file.rdbuf();
    const auto text = saved.str();
    require(text.find("springK = 2.300000000e+02f") != std::string::npos,
            "Custom spring parameter was not saved to disk");
    require(text.find("damping = 8.000000000e+00f") != std::string::npos,
            "Custom damping parameter was not saved to disk");
    require(!fs::exists(root / "src/PhysicsDefaults.hpp.tmp"), "Export left temporary file");
    fs::current_path(previous);
    fs::remove_all(root);
}

int main() {
    try {
        testPhysics();
        testMagnifiedPhysics();
        testShapeTransition();
        testBuildExport();
        std::cout << "Physics, magnification, transitions and playground export tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
