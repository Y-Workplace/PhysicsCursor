#pragma once

// Compose visible effects without erasing the mode's ongoing physics state.
// Tilt physics remains active during magnification, even with legacy configs
// that disable the other shake effects.
template <typename Result>
Result composeCursorEffects(Result mode, double shakeScale, bool shakeEffects,
                            bool tiltMode, bool daemonActive, double daemonAngle) {
    if (shakeScale > 1 && !shakeEffects && !tiltMode)
        mode = Result{};
    if (tiltMode && daemonActive)
        mode.rotation = daemonAngle;
    mode.scale *= shakeScale;
    return mode;
}
