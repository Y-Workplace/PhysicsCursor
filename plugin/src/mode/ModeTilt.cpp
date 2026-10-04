#include "ModeTilt.hpp"
#include "utils.hpp"
#include "../config/ConfigManager.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <numbers>
#include <cmath>
#include <algorithm>

EModeUpdate CModeTilt::strategy() {
    return TICK;
}

SModeResult CModeTilt::update(Vector2D pos) {
    auto now = std::chrono::high_resolution_clock::now();
    double dt = 1.0 / 60.0;
    if (hasLastTime) {
        dt = std::chrono::duration<double>(now - lastTime).count();
        if (dt <= 0.0001 || dt > 0.1)
            dt = 1.0 / 60.0;
    }
    lastTime = now;
    hasLastTime = true;

    auto function  = CONFIG(tiltFunction);
    auto limit     = CONFIG(tiltLimit);
    auto full_tilt = CONFIG(tiltFull);

    if (!initialized) {
        lastPos = pos;
        initialized = true;
        return SModeResult();
    }

    // 1. Linear kinematics with exponential filtering
    Vector2D instantVel = (pos - lastPos) / dt;
    lastPos = pos;

    double velDecay = std::exp(-35.0 * dt);
    velocity = velocity * velDecay + instantVel * (1.0 - velDecay);

    Vector2D instantAccel = (velocity - lastVel) / dt;
    lastVel = velocity;
    double accelDecay = std::exp(-25.0 * dt);
    accel = accel * accelDecay + instantAccel * (1.0 - accelDecay);

    // 2. Target spring angle based on velocity and inertia
    double speedX = velocity.x;
    double targetNorm = activation(function, limit, speedX);
    double targetAngle = targetNorm * (std::numbers::pi / (180.0 / full_tilt));

    // Inertial acceleration impulse (initial recoil and overshoot when braking)
    double inertialFactor = -(accel.x / (double)limit) * 0.20 * (std::numbers::pi / (180.0 / full_tilt));
    targetAngle += inertialFactor;

    // 3. Underdamped elastic harmonic oscillator (physical spring with sway)
    // Natural spring frequency (rad/s)
    double omega0 = 24.0;

    // Damping ratio zeta: 0.42 = underdamped
    // Produces natural elastic sway with noticeable overshoot when the mouse stops
    double zeta = 0.42;

    double springAccel = (omega0 * omega0) * (targetAngle - angle);
    double dampingAccel = (2.0 * zeta * omega0) * angularVelocity;
    double totalAngularAccel = springAccel - dampingAccel;

    // Numerical substeps (semi-implicit Euler) for stability
    const int subSteps = 4;
    double subDt = dt / subSteps;
    for (int i = 0; i < subSteps; ++i) {
        angularVelocity += totalAngularAccel * subDt;
        angle += angularVelocity * subDt;
    }

    // Maximum safety limit
    double maxAngle = full_tilt * (std::numbers::pi / 180.0) * 1.35;
    angle = std::clamp(angle, -maxAngle, maxAngle);

    // Snap gently to zero near rest to avoid unnecessary GPU work
    if (std::abs(speedX) < 1.0 && std::abs(angle) < 0.0005 && std::abs(angularVelocity) < 0.005) {
        angle = 0.0;
        angularVelocity = 0.0;
    }

    auto result = SModeResult();
    result.rotation = angle;
    return result;
}

void CModeTilt::warp(Vector2D old, Vector2D pos) {
    auto delta = pos - old;
    lastPos += delta;
}

void CModeTilt::reset() {
    angle = 0.0;
    angularVelocity = 0.0;
    velocity = {0, 0};
    lastVel = {0, 0};
    accel = {0, 0};
    initialized = false;
    hasLastTime = false;
}
