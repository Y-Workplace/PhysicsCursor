#pragma once
#include <cmath>
#include <deque>
#include <algorithm>
#include "PhysicsDefaults.hpp"

struct Vector2D {
    float x{0.0f};
    float y{0.0f};

    Vector2D operator+(const Vector2D& o) const { return {x + o.x, y + o.y}; }
    Vector2D operator-(const Vector2D& o) const { return {x - o.x, y - o.y}; }
    Vector2D operator*(float s) const { return {x * s, y * s}; }
    Vector2D operator/(float s) const { return (s != 0.0f) ? Vector2D{x / s, y / s} : Vector2D{0, 0}; }

    float lengthSq() const { return x * x + y * y; }
    float length() const { return std::sqrt(lengthSq()); }

    Vector2D normalized() const {
        float l = length();
        return (l > 0.0001f) ? (*this / l) : Vector2D{0, 0};
    }

    float cross(const Vector2D& o) const {
        return x * o.y - y * o.x;
    }

    float dot(const Vector2D& o) const {
        return x * o.x + y * o.y;
    }
};

struct TrailPoint {
    Vector2D pos;
    float angle;
    float alpha;
};

class PhysicsEngine {
public:
    // Defaults are saved by the playground and compiled into both modes.
    float mass = PhysicsDefaults::mass;
    float springK = PhysicsDefaults::springK;
    float damping = PhysicsDefaults::damping;
    float velocityInfluence = PhysicsDefaults::velocityInfluence;
    float inertiaInfluence = PhysicsDefaults::inertiaInfluence;
    float maxDeflectionDeg = PhysicsDefaults::maxDeflectionDeg;

    // Pivot state (cursor event hotspot at the exact pixel)
    Vector2D pivotPos{640.0f, 360.0f};
    Vector2D velocity{0.0f, 0.0f};
    Vector2D acceleration{0.0f, 0.0f};

    // Angular dynamics around the pivot (0.0 rad = original rest position)
    float angle = 0.0f;
    float angularVelocity = 0.0f;
    float angularAccel = 0.0f;

    // Telemetry
    float inertiaTorque = 0.0f;
    float dragTorque = 0.0f;
    float springTorque = 0.0f;
    float dampingTorque = 0.0f;
    float totalTorque = 0.0f;

    // Trail
    std::deque<TrailPoint> trail;
    static constexpr size_t MAX_TRAIL_POINTS = 24;

private:
    Vector2D lastPivotPos{640.0f, 360.0f};
    Vector2D lastFilteredVel{0.0f, 0.0f};
    bool isInitialized = false;

    // Fixed-step accumulator for frame-rate independence
    float accumulator = 0.0f;
    static constexpr float FIXED_DT = 0.002f; // Fixed step at 500 Hz (2 milliseconds)

public:
    PhysicsEngine() {
        resetToDefault();
    }

    void resetToDefault() {
        mass = PhysicsDefaults::mass;
        springK = PhysicsDefaults::springK;
        damping = PhysicsDefaults::damping;
        velocityInfluence = PhysicsDefaults::velocityInfluence;
        inertiaInfluence = PhysicsDefaults::inertiaInfluence;
        maxDeflectionDeg = PhysicsDefaults::maxDeflectionDeg;

        angle = 0.0f;
        angularVelocity = 0.0f;
        angularAccel = 0.0f;
        velocity = {0.0f, 0.0f};
        acceleration = {0.0f, 0.0f};
        lastFilteredVel = {0.0f, 0.0f};
        accumulator = 0.0f;
        isInitialized = false;
        trail.clear();
    }

    void setPivotPosition(float x, float y) {
        pivotPos = {x, y};
    }

    void applyAngularImpulse(float impulse) {
        angularVelocity += impulse * 0.10f;
    }

    void update(float dt) {
        if (dt <= 0.00001f) return;
        if (dt > 0.1f) dt = 0.1f; // Clamp large time steps while dragging the window

        if (!isInitialized) {
            lastPivotPos = pivotPos;
            isInitialized = true;
            return;
        }

        // 1. Linear kinematics with a frame-rate-independent continuous filter
        Vector2D rawVel = (pivotPos - lastPivotPos) / dt;
        lastPivotPos = pivotPos;

        // Continuous time-decay coefficient: exp(-frequency * dt)
        float velDecay = std::exp(-20.0f * dt);
        velocity = velocity * velDecay + rawVel * (1.0f - velDecay);

        Vector2D rawAccel = (velocity - lastFilteredVel) / dt;
        lastFilteredVel = velocity;

        float accelDecay = std::exp(-16.0f * dt);
        acceleration = acceleration * accelDecay + rawAccel * (1.0f - accelDecay);

        // 2. Fixed-step accumulator (ensures frame-rate independence)
        // Whether the display runs at 60, 120, 144, 240 Hz, or the daemon sleeps:
        // all elapsed time is integrated in identical 2 ms steps (FIXED_DT)
        accumulator += dt;
        if (accumulator > 0.05f) accumulator = 0.05f; // Prevent an accumulator spiral

        while (accumulator >= FIXED_DT) {
            integrateFixedStep(FIXED_DT);
            accumulator -= FIXED_DT;
        }

        // 3. Update the trail
        float speed = velocity.length();
        if (speed > 35.0f || std::abs(angularVelocity) > 0.35f) {
            trail.push_front({pivotPos, angle, 1.0f});
            if (trail.size() > MAX_TRAIL_POINTS) {
                trail.pop_back();
            }
        }

        for (auto& p : trail) {
            p.alpha -= dt * 4.0f;
            if (p.alpha < 0.0f) p.alpha = 0.0f;
        }

        while (!trail.empty() && trail.back().alpha <= 0.02f) {
            trail.pop_back();
        }
    }

private:
    void integrateFixedStep(float fixedDt) {
        // Physical torques as calibrated by the user:
        dragTorque = velocity.x * velocityInfluence * springK;
        inertiaTorque = -acceleration.x * inertiaInfluence * springK * mass;

        float diagonalComp = (velocity.x - velocity.y * 0.25f) * (velocityInfluence * 0.20f * springK);
        float externalTorque = dragTorque * 0.85f + diagonalComp * 0.15f + inertiaTorque;

        // Restoring harmonic spring: -k * theta
        springTorque = -springK * angle;

        // Smooth linear viscous damping: -gamma * omega
        dampingTorque = -damping * angularVelocity;

        totalTorque = externalTorque + springTorque + dampingTorque;

        float I = mass * 1.0f;
        angularAccel = totalTorque / I;

        // Semi-implicit Euler integration
        angularVelocity += angularAccel * fixedDt;
        angle += angularVelocity * fixedDt;

        // Angular safety limit
        float maxRad = maxDeflectionDeg * (float)(M_PI / 180.0);
        if (angle > maxRad) {
            angle = maxRad;
            if (angularVelocity > 0.0f) angularVelocity = 0.0f;
        } else if (angle < -maxRad) {
            angle = -maxRad;
            if (angularVelocity < 0.0f) angularVelocity = 0.0f;
        }

        // Snap to rest only when motion is negligible
        if (velocity.length() < 2.0f && std::abs(angle) < 0.002f && std::abs(angularVelocity) < 0.02f) {
            angle *= 0.96f;
            angularVelocity *= 0.96f;
        }
    }
};
