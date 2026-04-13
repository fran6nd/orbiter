// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// =======================================================================
// Input.h
// Platform-agnostic input manager.
//
// On Windows without ORBITER_USE_SDL3: backed by DirectInput (Di7frame).
// On Windows with  ORBITER_USE_SDL3 (or future non-Windows): keyboard
// state is collected from WM_KEYDOWN/WM_KEYUP via OnKey(); joystick
// support to be added in a later phase.
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
#   ifdef _WIN32
#       include <windows.h>  // HRESULT, HWND for method signatures
#   endif
#   ifdef ORBITER_USE_SDL3
#       include <SDL3/SDL.h>
#   endif
#endif

// ---------------------------------------------------------------------------
// DIDEVICEOBJECTDATA — buffered key event record.
//
// On the DInput path this struct comes from <dinput.h>.
// On the SDL3 / non-Windows path we define a layout-compatible version so
// that KbdInputBuffered_System / OnRunning and BroadcastBufferedKeyboardEvent
// can be compiled and called on both code paths without modification.
// ---------------------------------------------------------------------------
#if !ORBITER_DINPUT
struct DIDEVICEOBJECTDATA {
    unsigned long dwOfs;        // OAPI_KEY_* code
    unsigned long dwData;       // 0x80 = pressed, 0x00 = released
    unsigned long dwTimeStamp;  // milliseconds (filled by OnKey)
    unsigned long uAppData;     // unused on this path
};
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
    // SDL3 / non-Windows implementations in Input.cpp.
    HRESULT Create(void* /*hInst*/) { return 0 /*S_OK*/; }
    void    Destroy();
    void    SetRenderWindow(void*)  {}
    bool    CreateKbdDevice();   // returns true; state collected via OnKey()
    bool    CreateJoyDevice();   // opens the configured SDL joystick (SDL3 path)
    void    DestroyDevices();    // closes the SDL joystick
    void    OptionChanged(unsigned long cat, unsigned long item);

    // Called by Orbiter::MsgProc for WM_KEYDOWN / WM_KEYUP.
    // oapi_key — OAPI_KEY_* constant; pressed — true=down, false=up.
    void OnKey(unsigned long oapi_key, bool pressed);

    // Called on WM_ACTIVATE(inactive) / WM_KILLFOCUS to avoid stuck keys.
    void ClearKeyboard();

    // Called once per frame from UserInput().  Copies the current immediate
    // state into *state_out[256] and moves pending buffered events into buf[].
    // *count is set to the number of events written; buf must hold at least
    // capacity entries.
    void FlushKeyboard(char *state_out,
                       DIDEVICEOBJECTDATA *buf,
                       unsigned long *count,
                       unsigned long capacity);
#endif

    // Poll joystick — fills *state and returns true if data is available.
    // On the DInput path this translates DIJOYSTATE2 → JoystickState.
    // On the SDL3 / stub path it always returns false (joystick not yet wired).
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
#else
    // Keyboard state for the SDL3 / non-DInput path.
    // m_kbdState: immediate state — bit 7 set = key pressed (matches KEYDOWN macro).
    // m_kbdEvents / m_kbdEventCount: ring of buffered events pending drain.
    static constexpr unsigned long KBD_EVENT_BUF = 64;
    char          m_kbdState[256]           = {};
    DIDEVICEOBJECTDATA m_kbdEvents[KBD_EVENT_BUF] = {};
    unsigned long m_kbdEventCount           = 0;

#ifdef ORBITER_USE_SDL3
    // SDL3 joystick handle (null when no joystick is configured/opened).
    SDL_Joystick *m_sdlJoystick = nullptr;
#endif
#endif
};

#endif // !__INPUT_H
