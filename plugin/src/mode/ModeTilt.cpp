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

    // 1. Cinemática Linear com Filtro Exponencial
    Vector2D instantVel = (pos - lastPos) / dt;
    lastPos = pos;

    double velDecay = std::exp(-35.0 * dt);
    velocity = velocity * velDecay + instantVel * (1.0 - velDecay);

    Vector2D instantAccel = (velocity - lastVel) / dt;
    lastVel = velocity;
    double accelDecay = std::exp(-25.0 * dt);
    accel = accel * accelDecay + instantAccel * (1.0 - accelDecay);

    // 2. Ângulo Alvo da Mola com Base na Velocidade e Inércia
    double speedX = velocity.x;
    double targetNorm = activation(function, limit, speedX);
    double targetAngle = targetNorm * (std::numbers::pi / (180.0 / full_tilt));

    // Pulso inercial de aceleração (dá o recuo inicial e o overshoot ao frear)
    double inertialFactor = -(accel.x / (double)limit) * 0.20 * (std::numbers::pi / (180.0 / full_tilt));
    targetAngle += inertialFactor;

    // 3. Oscilador Harmônico Elástico Subamortecido (Mola Física com Balanço Real)
    // Frequência natural da mola (rad/s)
    double omega0 = 24.0;

    // Fator de amortecimento zeta: 0.42 = subamortecido!
    // Produz balanço elástico natural e orgânico, com overshoot perceptível ao parar o mouse
    double zeta = 0.42;

    double springAccel = (omega0 * omega0) * (targetAngle - angle);
    double dampingAccel = (2.0 * zeta * omega0) * angularVelocity;
    double totalAngularAccel = springAccel - dampingAccel;

    // Sub-stepping numérico (Semi-Implicit Euler) para estabilidade
    const int subSteps = 4;
    double subDt = dt / subSteps;
    for (int i = 0; i < subSteps; ++i) {
        angularVelocity += totalAngularAccel * subDt;
        angle += angularVelocity * subDt;
    }

    // Limite máximo de segurança
    double maxAngle = full_tilt * (std::numbers::pi / 180.0) * 1.35;
    angle = std::clamp(angle, -maxAngle, maxAngle);

    // Ao atingir repouso quase absoluto, zera suavemente para evitar custo de GPU
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
