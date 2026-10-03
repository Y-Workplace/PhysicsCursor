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
    // --- Parâmetros Físicos Orgânicos, Lentos e Suaves ---
    float mass = 1.0f;
    float springK = 65.0f;             // Mola macia para balanço lento e orgânico (oscilação relaxada)
    float damping = 6.2f;              // Amortecimento reduzido para permitir balanço fluido e overshoot suave
    float velocityInfluence = 0.0010f; // Entrada de velocidade progressiva (sem arrasto repentino)
    float inertiaInfluence = 0.00018f; // Força inercial suave proporcional à aceleração
    float maxDeflectionDeg = 36.0f;    // Deflexão máxima em graus

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
    static constexpr size_t MAX_TRAIL_POINTS = 24;

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
        springK = 65.0f;
        damping = 6.2f;
        velocityInfluence = 0.0010f;
        inertiaInfluence = 0.00018f;
        maxDeflectionDeg = 36.0f;

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
        angularVelocity += impulse * 0.10f;
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

        // 1. Cinemática Linear com Filtro de Velocidade Mais Suave e Progressivo
        Vector2D rawVel = (pivotPos - lastPivotPos) / dt;
        lastPivotPos = pivotPos;

        // velDecay mais suave (~13 Hz em vez de 24 Hz) evita picos bruscos ao iniciar o movimento
        float velDecay = std::exp(-13.0f * dt);
        velocity = velocity * velDecay + rawVel * (1.0f - velDecay);

        Vector2D rawAccel = (velocity - lastFilteredVel) / dt;
        lastFilteredVel = velocity;

        // accelDecay mais suave (~11 Hz) filtra acelerações repentinas
        float accelDecay = std::exp(-11.0f * dt);
        acceleration = acceleration * accelDecay + rawAccel * (1.0f - accelDecay);

        // 2. Torques Físicos Suavizados com Curva Assimptótica
        float maxRad = maxDeflectionDeg * (float)(M_PI / 180.0);

        // Curva suave tanh: pequenos movimentos dão toque sutil, movimentos rápidos não dão tranco
        float softVelFactor = std::tanh(velocity.x * velocityInfluence);
        dragTorque = softVelFactor * maxRad * springK * 0.85f;

        float softAccelFactor = std::tanh(acceleration.x * inertiaInfluence);
        inertiaTorque = -softAccelFactor * maxRad * springK * 0.35f;

        // Leve inclinação ao mover em diagonal
        float softVelY = std::tanh(velocity.y * velocityInfluence * 0.4f);
        float diagonalComp = -softVelY * (velocity.x >= 0 ? 0.12f : -0.12f) * maxRad * springK;

        float externalTorque = dragTorque + inertiaTorque + diagonalComp;

        // 3. Mola Torsional e Amortecimento Suave Reduzido
        // A mola restauradora com k menor garante um balanço mais lento e majestoso
        springTorque = -springK * angle;

        // Amortecimento suave: permite que o cursor balance organicamente sem frear de forma seca
        dampingTorque = -damping * angularVelocity;
        totalTorque = externalTorque + springTorque + dampingTorque;

        float I = mass * 1.0f;
        angularAccel = totalTorque / I;

        // 4. Integração Numérica (Semi-Implicit Euler com Sub-stepping)
        const int subSteps = 4;
        float subDt = dt / (float)subSteps;

        for (int i = 0; i < subSteps; ++i) {
            angularVelocity += angularAccel * subDt;
            angle += angularVelocity * subDt;

            if (angle > maxRad) {
                angle = maxRad;
                if (angularVelocity > 0.0f) angularVelocity *= 0.3f;
            } else if (angle < -maxRad) {
                angle = -maxRad;
                if (angularVelocity < 0.0f) angularVelocity *= 0.3f;
            }
        }

        float speed = velocity.length();
        float absAngle = std::abs(angle);

        // Zera repouso com transição imperceptível apenas quando estiver quase 100% imóvel
        if (speed < 3.0f && absAngle < 0.003f && std::abs(angularVelocity) < 0.03f) {
            angle *= 0.90f;
            angularVelocity *= 0.90f;
        }

        // 5. Rastro Suave
        if (speed > 30.0f || std::abs(angularVelocity) > 0.3f) {
            trail.push_front({pivotPos, angle, 1.0f});
            if (trail.size() > MAX_TRAIL_POINTS) {
                trail.pop_back();
            }
        }

        for (auto& p : trail) {
            p.alpha -= dt * 3.8f;
            if (p.alpha < 0.0f) p.alpha = 0.0f;
        }

        while (!trail.empty() && trail.back().alpha <= 0.02f) {
            trail.pop_back();
        }
    }
};
