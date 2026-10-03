#pragma once
#include <algorithm>
#include <cmath>

struct CursorTransitionFrame {
    double opacity = 1;
    double scale = 1;
    double rotation = 0;
};

// CSS cubic-bezier(.34, 1.56, .64, 1), including the elastic overshoot.
inline double cursorTransitionEase(double progress) {
    const double x = std::clamp(progress, 0.0, 1.0);
    double lo = 0, hi = 1;
    const auto bezier = [](double t, double a, double b) {
        const double u = 1 - t;
        return 3 * u * u * t * a + 3 * u * t * t * b + t * t * t;
    };
    for (int i = 0; i < 24; ++i) {
        const double t = (lo + hi) * 0.5;
        if (bezier(t, .34, .64) < x) lo = t; else hi = t;
    }
    if (x == 0 || x == 1) return x;
    return bezier((lo + hi) * 0.5, 1.56, 1);
}

inline CursorTransitionFrame cursorTransitionFrame(double elapsed, double duration,
                                                   bool entering, CursorTransitionFrame source = {}) {
    const double t = std::clamp(elapsed / std::max(duration, .001), 0.0, 1.0);
    const double fade = std::clamp(t / .8, 0.0, 1.0);
    const double alpha = fade * fade * (3 - 2 * fade);
    const double eased = cursorTransitionEase(t);
    constexpr double turn = 0.2617993877991494; // 15 degrees
    if (entering)
        return {alpha, .6 + .4 * eased, -turn * (1 - eased)};
    return {source.opacity * (1 - alpha), source.scale + (.6 - source.scale) * eased,
            source.rotation + (turn - source.rotation) * eased};
}
