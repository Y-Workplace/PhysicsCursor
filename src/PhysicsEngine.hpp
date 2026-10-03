#pragma once
#include <cmath>
#include <deque>
#include <algorithm>

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
    // --- Parâmetros Físicos Orgânicos e Calibrados ---
    float mass = 1.0f;
    float springK = 140.0f;            // Mola restauradora suave
    float damping = 24.0f;             // Amortecimento viscoso
    float velocityInfluence = 0.0014f; // Sensibilidade ao arrasto horizontal
    float inertiaInfluence = 0.00030f; // Força inercial proporcional à aceleração
    float maxDeflectionDeg = 38.0f;    // Deflexão máxima natural em graus

    // Estado do Pivô (Ponto de Evento do Cursor no pixel exato)
    Vector2D pivotPos{640.0f, 360.0f};
    Vector2D velocity{0.0f, 0.0f};
    Vector2D acceleration{0.0f, 0.0f};

    // Dinâmica Angular em Torno do Pivô (0.0 rad = repouso original)
    float angle = 0.0f;
    float angularVelocity = 0.0f;
    float angularAccel = 0.0f;

    // Telemetria
    float inertiaTorque = 0.0f;
    float dragTorque = 0.0f;
    float springTorque = 0.0f;
    float dampingTorque = 0.0f;
    float totalTorque = 0.0f;

    // Rastro
    std::deque<TrailPoint> trail;
    static constexpr size_t MAX_TRAIL_POINTS = 20;

private:
    Vector2D lastPivotPos{640.0f, 360.0f};
    Vector2D lastFilteredVel{0.0f, 0.0f};
    bool isInitialized = false;

public:
    PhysicsEngine() {
        resetToDefault();
    }

    void resetToDefault() {
        mass = 1.0f;
        springK = 140.0f;
        damping = 24.0f;
        velocityInfluence = 0.0014f;
        inertiaInfluence = 0.00030f;
        maxDeflectionDeg = 38.0f;

        angle = 0.0f;
        angularVelocity = 0.0f;
        angularAccel = 0.0f;
        velocity = {0.0f, 0.0f};
        acceleration = {0.0f, 0.0f};
        lastFilteredVel = {0.0f, 0.0f};
        trail.clear();
    }

    void setPivotPosition(float x, float y) {
        pivotPos = {x, y};
    }

    void applyAngularImpulse(float impulse) {
        angularVelocity += impulse * 0.12f;
    }

    void update(float dt) {
        if (dt <= 0.0001f || dt > 0.1f) {
            dt = 1.0f / 60.0f;
        }

        if (!isInitialized) {
            lastPivotPos = pivotPos;
            isInitialized = true;
            return;
        }

        // 1. Cinemática Linear com Filtro de Tempo Contínuo Suave
        Vector2D rawVel = (pivotPos - lastPivotPos) / dt;
        lastPivotPos = pivotPos;

        float velDecay = std::exp(-24.0f * dt);
        velocity = velocity * velDecay + rawVel * (1.0f - velDecay);

        Vector2D rawAccel = (velocity - lastFilteredVel) / dt;
        lastFilteredVel = velocity;

        float accelDecay = std::exp(-20.0f * dt);
        acceleration = acceleration * accelDecay + rawAccel * (1.0f - accelDecay);

        // 2. Torques Físicos
        dragTorque = velocity.x * velocityInfluence * springK;
        inertiaTorque = -acceleration.x * inertiaInfluence * springK * mass;

        float diagonalComp = (velocity.x - velocity.y * 0.30f) * (velocityInfluence * 0.35f * springK);
        float externalTorque = dragTorque * 0.75f + diagonalComp * 0.25f + inertiaTorque;

        // 3. Retorno ao Repouso Ultra-Natural e Orgânico
        float absAngle = std::abs(angle);
        springTorque = -springK * angle * (1.0f + 0.4f * absAngle);

        float speed = velocity.length();
        float restBlend = std::clamp(1.0f - (speed / 120.0f), 0.0f, 1.0f);
        float effectiveDamping = damping * (1.0f + 0.8f * restBlend);

        dampingTorque = -effectiveDamping * angularVelocity;
        totalTorque = externalTorque + springTorque + dampingTorque;

        float I = mass * 1.0f;
        angularAccel = totalTorque / I;

        // 4. Integração Numérica com Sub-stepping
        const int subSteps = 4;
        float subDt = dt / (float)subSteps;
        float maxRad = maxDeflectionDeg * (float)(M_PI / 180.0);

        for (int i = 0; i < subSteps; ++i) {
            angularVelocity += angularAccel * subDt;
            angle += angularVelocity * subDt;

            if (angle > maxRad) {
                angle = maxRad;
                if (angularVelocity > 0.0f) angularVelocity *= 0.2f;
            } else if (angle < -maxRad) {
                angle = -maxRad;
                if (angularVelocity < 0.0f) angularVelocity *= 0.2f;
            }
        }

        if (speed < 5.0f && absAngle < 0.005f && std::abs(angularVelocity) < 0.05f) {
            angle *= 0.85f;
            angularVelocity *= 0.85f;
        }

        // 5. Rastro
        if (speed > 40.0f || std::abs(angularVelocity) > 0.4f) {
            trail.push_front({pivotPos, angle, 1.0f});
            if (trail.size() > MAX_TRAIL_POINTS) {
                trail.pop_back();
            }
        }

        for (auto& p : trail) {
            p.alpha -= dt * 4.5f;
            if (p.alpha < 0.0f) p.alpha = 0.0f;
        }

        while (!trail.empty() && trail.back().alpha <= 0.02f) {
            trail.pop_back();
        }
    }
};
