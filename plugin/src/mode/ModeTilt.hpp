#pragma once

#include "Mode.hpp"
#include <hyprutils/math/Vector2D.hpp>
#include <chrono>

class CModeTilt : public IMode {
  public:
    virtual EModeUpdate strategy();
    virtual SModeResult update(Vector2D pos);
    virtual void        reset();
    virtual void        warp(Vector2D old, Vector2D pos);

  private:
    Vector2D lastPos{0, 0};
    Vector2D velocity{0, 0};
    Vector2D lastVel{0, 0};
    Vector2D accel{0, 0};

    double angle = 0.0;
    double angularVelocity = 0.0;

    std::chrono::high_resolution_clock::time_point lastTime;
    bool hasLastTime = false;
    bool initialized = false;
};
