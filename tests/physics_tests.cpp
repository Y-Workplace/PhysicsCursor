#include "BuildAction.hpp"
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
        testBuildExport();
        std::cout << "Physics and playground export tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
