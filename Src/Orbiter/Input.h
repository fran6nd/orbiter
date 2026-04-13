// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// =======================================================================
// Input.h
// Platform-agnostic input manager.
//
// On Windows without ORBITER_USE_SDL3: backed by DirectInput (Di7frame).
// On Windows with  ORBITER_USE_SDL3, or on non-Windows: stub that returns
// no input; the SDL3 implementation will be wired in a later phase.
// =======================================================================

#ifndef __INPUT_H
#define __INPUT_H

#include "../Platform/JoystickState.h"

// Di7frame is only available on Windows without the SDL3 override.
#if defined(_WIN32) && !defined(ORBITER_USE_SDL3)
#   include "Di7frame.h"
#   define ORBITER_DINPUT 1
#else
#   define ORBITER_DINPUT 0
#endif

class Orbiter;

// ---------------------------------------------------------------------------
// DInput — unified input manager.
//
// JoyProp is exposed on all platforms so that the joystick throttle/rudder
// configuration lives in a single place.  On non-DInput builds the throttle
// axis is surfaced through JoystickState::ThrottleAxis; ThrottleOfs is kept
// for source-level compatibility but is unused outside the DInput back-end.
// ---------------------------------------------------------------------------

class DInput {
    friend class Orbiter;

public:
    DInput(Orbiter *pOrbiter);
    ~DInput();

    struct JoyProp {
        bool  bThrottle  = false;
        bool  bRudder    = false;
        // ThrottleAxis is the canonical cross-platform field.
        // ThrottleOfs retains the original byte-offset-into-DIJOYSTATE2 value
        // so that existing plugins reading joyprop.ThrottleOfs keep working.
        orbiter::JoystickState::ThrottleAxis ThrottleAxis =
            orbiter::JoystickState::ThrottleAxis::None;
        int ThrottleOfs = 0;
    };

#if ORBITER_DINPUT
    HRESULT Create(HINSTANCE hInst);
    void    Destroy();
    void    SetRenderWindow(HWND hWnd);
    bool    CreateKbdDevice();
    bool    CreateJoyDevice();
    void    DestroyDevices();
    void    OptionChanged(DWORD cat, DWORD item);

    // Returns the DirectInput framework — only available on the DInput path.
    inline CDIFramework7 *GetDIFrame() const { return diframe; }

    // Raw device accessors — only available on the DInput path.
    inline LPDIRECTINPUTDEVICE8 GetKbdDevice() const { return diframe->GetKbdDevice(); }
    inline LPDIRECTINPUTDEVICE8 GetJoyDevice() const { return diframe->GetJoyDevice(); }
#else
    // No-op stubs for the SDL3 / non-Windows path.
    // Full SDL3 input will be implemented in a subsequent phase.
    HRESULT Create(void* /*hInst*/) { return 0 /*S_OK*/; }
    void    Destroy()               {}
    void    SetRenderWindow(void*)  {}
    bool    CreateKbdDevice()       { return false; }
    bool    CreateJoyDevice()       { return false; }
    void    DestroyDevices()        {}
    void    OptionChanged(unsigned long /*cat*/, unsigned long /*item*/) {}
#endif

    // Poll joystick — fills *state and returns true if data is available.
    // On the DInput path this translates DIJOYSTATE2 → JoystickState.
    // On the SDL3 / stub path it always returns false.
    bool PollJoystick(orbiter::JoystickState *state);

    JoyProp joyprop;

protected:
#if ORBITER_DINPUT
    HRESULT SetJoystickProperties();
#endif

private:
    Orbiter *orbiter;
#if ORBITER_DINPUT
    CDIFramework7 *diframe;
    HWND           m_hWnd;
#endif
};

#endif // !__INPUT_H
