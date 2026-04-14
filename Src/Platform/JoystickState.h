// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ============================================================================
// JoystickState.h
// Platform-agnostic joystick state snapshot — replaces DIJOYSTATE2 in the
// Orbiter core.  The DirectInput (Windows) and SDL3 input backends both
// translate their native state into this struct before passing it upward.
//
// Field names and ranges deliberately mirror DIJOYSTATE2 so that the bulk of
// the existing joystick-handling code in Orbiter.cpp needs no change.
// ============================================================================

#ifndef ORBITER_JOYSTICKSTATE_H
#define ORBITER_JOYSTICKSTATE_H

#include <cstdint>

namespace orbiter {

struct JoystickState {
    // ---------------------------------------------------------------------------
    // Axes  — range [-1000, +1000] for all backends.
    // ---------------------------------------------------------------------------
    long lX  = 0;   // primary X axis (roll / bank)
    long lY  = 0;   // primary Y axis (pitch)
    long lZ  = 0;   // primary Z axis (throttle or collective)
    long lRx = 0;   // rotational X
    long lRy = 0;   // rotational Y
    long lRz = 0;   // rotational Z (rudder)
    long rglSlider[2] = {};   // slider axes (throttle lever, etc.)

    // ---------------------------------------------------------------------------
    // POV hats — value in hundredths of a degree [0, 35900], or 0xFFFFFFFF when
    // centred.  Matches the DIJOYSTATE2 / SDL_HAT convention after mapping.
    // ---------------------------------------------------------------------------
    unsigned long rgdwPOV[4] = { 0xFFFFFFFFu, 0xFFFFFFFFu,
                                  0xFFFFFFFFu, 0xFFFFFFFFu };

    // ---------------------------------------------------------------------------
    // Buttons — 0x00 = released, 0x80 = pressed (matches DIJOYSTATE2 layout).
    // ---------------------------------------------------------------------------
    unsigned char rgbButtons[128] = {};

    // Resets all fields to their neutral (centred / released) state.
    void clear() noexcept {
        lX = lY = lZ = lRx = lRy = lRz = 0;
        rglSlider[0] = rglSlider[1] = 0;
        for (auto& p : rgdwPOV)     p = 0xFFFFFFFFu;
        for (auto& b : rgbButtons)  b = 0;
    }
};

} // namespace orbiter

#endif // ORBITER_JOYSTICKSTATE_H
