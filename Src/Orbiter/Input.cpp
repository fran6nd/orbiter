// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// =======================================================================
// Input.cpp
// Input manager implementation.
//
// Conditionally compiled:
//   ORBITER_DINPUT=1  — full DirectInput back-end (Windows, no SDL3 override)
//   ORBITER_DINPUT=0  — keyboard via OnKey(); joystick via SDL3 (SDL3 path)
//                       or permanently stubbed (non-SDL3 non-Windows path)
// =======================================================================

#include "Input.h"
#include "Log.h"
#include "Orbiter.h"

// ---------------------------------------------------------------------------
// SDL3 axis / hat helpers — forward-declared here so PollJoystick can use
// them before the SDL3-guarded block further below.
// ---------------------------------------------------------------------------
#ifdef ORBITER_USE_SDL3
static long         SDLAxisToOrbiter(Sint16 raw, int deadzone_units);
static unsigned long SDLHatToAngle(Uint8 hat);
#endif

// ---------------------------------------------------------------------------
// Common constructor / destructor
// ---------------------------------------------------------------------------

DInput::DInput(Orbiter *pOrbiter)
{
    orbiter = pOrbiter;
#if ORBITER_DINPUT
    diframe = nullptr;
    m_hWnd  = nullptr;
#endif
}

DInput::~DInput()
{
    Destroy();
}

// ---------------------------------------------------------------------------
// PollJoystick — common entry point regardless of back-end.
// DInput path: queries the device and translates DIJOYSTATE2 → JoystickState.
// Stub path:   always returns false (no input).
// ---------------------------------------------------------------------------

bool DInput::PollJoystick(orbiter::JoystickState *state)
{
#if ORBITER_DINPUT
    LPDIRECTINPUTDEVICE8 dev = GetJoyDevice();
    if (!dev) return false;

    DIJOYSTATE2 raw = {};
    dev->Poll();
    HRESULT hr = dev->GetDeviceState(sizeof(DIJOYSTATE2), &raw);
    if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
        if (SUCCEEDED(dev->Acquire())) {
            dev->Poll();
            hr = dev->GetDeviceState(sizeof(DIJOYSTATE2), &raw);
        }
    }
    if (hr != S_OK) return false;

    // Translate DIJOYSTATE2 → JoystickState
    state->lX  = raw.lX;
    state->lY  = raw.lY;
    state->lZ  = raw.lZ;
    state->lRx = raw.lRx;
    state->lRy = raw.lRy;
    state->lRz = raw.lRz;
    state->rglSlider[0] = raw.rglSlider[0];
    state->rglSlider[1] = raw.rglSlider[1];
    for (int i = 0; i < 4; ++i)
        state->rgdwPOV[i] = raw.rgdwPOV[i];
    for (int i = 0; i < 128; ++i)
        state->rgbButtons[i] = raw.rgbButtons[i];

    return true;
#elif defined(ORBITER_USE_SDL3)
    if (!m_sdlJoystick) return false;

    SDL_UpdateJoysticks();
    Config *pcfg = orbiter->Cfg();
    int dz = pcfg->CfgJoystickPrm.Deadzone;

    int numAxes    = SDL_GetNumJoystickAxes(m_sdlJoystick);
    int numButtons = SDL_GetNumJoystickButtons(m_sdlJoystick);
    int numHats    = SDL_GetNumJoystickHats(m_sdlJoystick);

    // Axis mapping: index 0=X, 1=Y, 2=Z, 3=Rx, 4=Ry, 5=Rz, 6=Slider0, 7=Slider1
    auto axis = [&](int i) -> long {
        if (i >= numAxes) return 0;
        return SDLAxisToOrbiter(SDL_GetJoystickAxis(m_sdlJoystick, i), dz);
    };

    state->lX          = axis(0);
    state->lY          = axis(1);
    state->lZ          = axis(2);
    state->lRx         = axis(3);
    state->lRy         = axis(4);
    state->lRz         = axis(5);
    state->rglSlider[0]= axis(6);
    state->rglSlider[1]= axis(7);

    // POV hats
    for (int i = 0; i < 4; ++i)
        state->rgdwPOV[i] = (i < numHats)
            ? SDLHatToAngle(SDL_GetJoystickHat(m_sdlJoystick, i))
            : 0xFFFFFFFFu;

    // Buttons (up to 128)
    int nb = (numButtons < 128) ? numButtons : 128;
    for (int i = 0; i < nb; ++i)
        state->rgbButtons[i] = SDL_GetJoystickButton(m_sdlJoystick, i) ? 0x80u : 0x00u;
    for (int i = nb; i < 128; ++i)
        state->rgbButtons[i] = 0;

    return true;
#else
    (void)state;
    return false;
#endif
}

// ---------------------------------------------------------------------------
// SDL3 / non-DInput keyboard + joystick implementation
// ---------------------------------------------------------------------------

#if !ORBITER_DINPUT

void DInput::Destroy()
{
    DestroyDevices();
}

bool DInput::CreateKbdDevice()
{
    // No explicit device creation needed — state is collected via OnKey().
    return true;
}

#ifdef ORBITER_USE_SDL3

// Helper: clamp a raw Sint16 axis value to [-1000, +1000] with deadzone.
static long SDLAxisToOrbiter(Sint16 raw, int deadzone_units)
{
    // raw ∈ [-32768, 32767]; map to [-1000, +1000]
    long v = (long)raw * 1000L / 32767L;
    // Apply deadzone in same units as result range.
    int dz = deadzone_units / 10; // CfgJoystickPrm.Deadzone is 0-10000; scale to 0-1000
    if (v > -dz && v < dz) v = 0;
    return v;
}

// Helper: map SDL hat mask to DInput-style angle in hundredths of a degree.
static unsigned long SDLHatToAngle(Uint8 hat)
{
    // SDL_HAT_* bits: UP=0x01 RIGHT=0x02 DOWN=0x04 LEFT=0x08
    switch (hat) {
    case SDL_HAT_UP:        return 0;
    case SDL_HAT_RIGHTUP:   return 4500;
    case SDL_HAT_RIGHT:     return 9000;
    case SDL_HAT_RIGHTDOWN: return 13500;
    case SDL_HAT_DOWN:      return 18000;
    case SDL_HAT_LEFTDOWN:  return 22500;
    case SDL_HAT_LEFT:      return 27000;
    case SDL_HAT_LEFTUP:    return 31500;
    default:                return 0xFFFFFFFFu; // centred
    }
}

bool DInput::CreateJoyDevice()
{
    using ThrottleAxis = orbiter::JoystickState::ThrottleAxis;
    Config *pcfg = orbiter->Cfg();
    if (!pcfg->CfgJoystickPrm.Joy_idx) return false; // 0 = disabled

    int count = 0;
    SDL_JoystickID *ids = SDL_GetJoysticks(&count);
    int requested = (int)pcfg->CfgJoystickPrm.Joy_idx - 1; // convert to 0-based
    if (!ids || requested >= count) {
        SDL_free(ids);
        LOGOUT_ERR("SDL3: requested joystick index not available");
        return false;
    }

    m_sdlJoystick = SDL_OpenJoystick(ids[requested]);
    SDL_free(ids);
    if (!m_sdlJoystick) {
        LOGOUT_ERR("SDL3: could not open joystick");
        return false;
    }

    int numAxes = SDL_GetNumJoystickAxes(m_sdlJoystick);

    // Detect rudder: needs at least 6 axes (X, Y, Z, Rx, Ry, Rz).
    joyprop.bRudder = (numAxes >= 6);

    // Throttle axis selection from config.
    joyprop.bThrottle    = false;
    joyprop.ThrottleAxis = ThrottleAxis::None;
    joyprop.ThrottleOfs  = 0;

    switch (pcfg->CfgJoystickPrm.ThrottleAxis) {
    case 1: // Z-axis (axis index 2)
        if (numAxes >= 3) {
            joyprop.bThrottle    = true;
            joyprop.ThrottleAxis = ThrottleAxis::Z;
        }
        break;
    case 2: // Slider 0 (axis index 6)
        if (numAxes >= 7) {
            joyprop.bThrottle    = true;
            joyprop.ThrottleAxis = ThrottleAxis::Slider0;
        }
        break;
    case 3: // Slider 1 (axis index 7)
        if (numAxes >= 8) {
            joyprop.bThrottle    = true;
            joyprop.ThrottleAxis = ThrottleAxis::Slider1;
        }
        break;
    default:
        break;
    }

    LOGOUT("SDL3 joystick opened");
    return true;
}

void DInput::DestroyDevices()
{
    if (m_sdlJoystick) {
        SDL_CloseJoystick(m_sdlJoystick);
        m_sdlJoystick = nullptr;
    }
}

void DInput::OptionChanged(unsigned long cat, unsigned long item)
{
    if (cat == OPTCAT_JOYSTICK) {
        switch (item) {
        case OPTITEM_JOYSTICK_DEVICE:
            DestroyDevices();
            CreateJoyDevice();
            break;
        case OPTITEM_JOYSTICK_PARAM:
            // Re-evaluate joyprop without reopening the device.
            if (m_sdlJoystick) {
                DestroyDevices();
                CreateJoyDevice();
            }
            break;
        }
    }
}

#else // !ORBITER_USE_SDL3 but still !ORBITER_DINPUT (future non-SDL3 non-Windows)

bool DInput::CreateJoyDevice()  { return false; }
void DInput::DestroyDevices()   {}
void DInput::OptionChanged(unsigned long /*cat*/, unsigned long /*item*/) {}

#endif // ORBITER_USE_SDL3

void DInput::OnKey(unsigned long oapi_key, bool pressed)
{
    if (oapi_key == 0 || oapi_key >= 256) return;

#ifdef ORBITER_USE_SDL3
    unsigned long now = (unsigned long)SDL_GetTicks();
#else
    unsigned long now = (unsigned long)GetTickCount();
#endif

    if (pressed) {
        bool was_up = !(m_kbdState[oapi_key] & 0x80);
        m_kbdState[oapi_key] = 0x80;
        // Only emit a buffered DOWN event on the first press, not on auto-repeat.
        if (was_up && m_kbdEventCount < KBD_EVENT_BUF) {
            DIDEVICEOBJECTDATA &ev = m_kbdEvents[m_kbdEventCount++];
            ev.dwOfs       = oapi_key;
            ev.dwData      = 0x80;
            ev.dwTimeStamp = now;
            ev.uAppData    = 0;
        }
    } else {
        m_kbdState[oapi_key] = 0x00;
        if (m_kbdEventCount < KBD_EVENT_BUF) {
            DIDEVICEOBJECTDATA &ev = m_kbdEvents[m_kbdEventCount++];
            ev.dwOfs       = oapi_key;
            ev.dwData      = 0x00;
            ev.dwTimeStamp = now;
            ev.uAppData    = 0;
        }
    }
}

void DInput::ClearKeyboard()
{
    memset(m_kbdState, 0, sizeof(m_kbdState));
    m_kbdEventCount = 0;
}

void DInput::FlushKeyboard(char *state_out,
                            DIDEVICEOBJECTDATA *buf,
                            unsigned long *count,
                            unsigned long capacity)
{
    memcpy(state_out, m_kbdState, 256);
    unsigned long n = (m_kbdEventCount < capacity) ? m_kbdEventCount : capacity;
    if (n) memcpy(buf, m_kbdEvents, n * sizeof(DIDEVICEOBJECTDATA));
    *count = n;
    m_kbdEventCount = 0;
}

#endif // !ORBITER_DINPUT

// ---------------------------------------------------------------------------
// DirectInput-only implementations
// ---------------------------------------------------------------------------

#if ORBITER_DINPUT

HRESULT DInput::Create(HINSTANCE hInst)
{
    if (nullptr == (diframe = new CDIFramework7())) {
        LOGOUT_ERR("DirectInput: Could not create DI environment");
        return E_OUTOFMEMORY;
    }
    return diframe->Create(hInst);
}

void DInput::Destroy()
{
    if (diframe) {
        delete diframe;
        diframe = nullptr;
    }
}

void DInput::SetRenderWindow(HWND hWnd)
{
    if (diframe)
        diframe->DestroyDevices();
    m_hWnd = hWnd;
}

bool DInput::CreateKbdDevice()
{
    if (!m_hWnd) return false;
    if (FAILED(diframe->CreateKbdDevice(m_hWnd))) {
        LOGOUT("ERROR: Could not create keyboard device");
        return false;
    }
    GetKbdDevice()->Acquire();
    return true;
}

bool DInput::CreateJoyDevice()
{
    if (!m_hWnd) return false;
    Config *pcfg = orbiter->Cfg();
    if (!pcfg->CfgJoystickPrm.Joy_idx) return false;

    if (FAILED(diframe->CreateJoyDevice(m_hWnd, pcfg->CfgJoystickPrm.Joy_idx - 1))) {
        LOGOUT_ERR("Could not create joystick device");
        return false;
    }

    HRESULT hr = GetJoyDevice()->Acquire();
    if (hr == DIERR_OTHERAPPHASPRIO) {
        Sleep(1000);
        hr = GetJoyDevice()->Acquire();
    }
    // DIERR_OTHERAPPHASPRIO after retry is non-fatal; device will be acquired on first poll
    if (hr == DIERR_OTHERAPPHASPRIO) hr = DI_OK;

    if (SetJoystickProperties() != DI_OK) {
        LOGOUT_ERR("Could not set joystick properties");
        return false;
    }
    return true;
}

void DInput::DestroyDevices()
{
    diframe->DestroyDevices();
}

void DInput::OptionChanged(DWORD cat, DWORD item)
{
    if (cat == OPTCAT_JOYSTICK) {
        switch (item) {
        case OPTITEM_JOYSTICK_DEVICE:
            diframe->DestroyJoyDevice();
            CreateJoyDevice();
            break;
        case OPTITEM_JOYSTICK_PARAM:
            SetJoystickProperties();
            break;
        }
    }
}

HRESULT DInput::SetJoystickProperties()
{
    using ThrottleAxis = orbiter::JoystickState::ThrottleAxis;

    LPDIRECTINPUTDEVICE8 dev = GetJoyDevice();
    if (!dev) return DI_OK;

    HRESULT hr;
    DIPROPRANGE diprg;
    DIPROPDWORD diprw;
    joyprop.bRudder      = false;
    joyprop.bThrottle    = false;
    joyprop.ThrottleAxis = ThrottleAxis::None;
    joyprop.ThrottleOfs  = 0;
    Config *pcfg = orbiter->Cfg();

    // x-axis range and deadzone
    diprg.diph = { sizeof(diprg), sizeof(diprg.diph), DIJOFS_X, DIPH_BYOFFSET };
    diprg.lMin = -1000; diprg.lMax = +1000;
    if ((hr = dev->SetProperty(DIPROP_RANGE, &diprg.diph)) != DI_OK) return hr;

    diprw.diph = { sizeof(diprw), sizeof(diprw.diph), DIJOFS_X, DIPH_BYOFFSET };
    diprw.dwData = pcfg->CfgJoystickPrm.Deadzone;
    if ((hr = dev->SetProperty(DIPROP_DEADZONE, &diprw.diph)) != DI_OK) return hr;

    // y-axis range and deadzone
    diprg.diph = { sizeof(diprg), sizeof(diprg.diph), DIJOFS_Y, DIPH_BYOFFSET };
    diprg.lMin = -1000; diprg.lMax = +1000;
    if ((hr = dev->SetProperty(DIPROP_RANGE, &diprg.diph)) != DI_OK) return hr;

    diprw.diph = { sizeof(diprw), sizeof(diprw.diph), DIJOFS_Y, DIPH_BYOFFSET };
    diprw.dwData = pcfg->CfgJoystickPrm.Deadzone;
    if ((hr = dev->SetProperty(DIPROP_DEADZONE, &diprw.diph)) != DI_OK) return hr;

    // Rz axis (rudder)
    joyprop.bRudder   = true;
    joyprop.bThrottle = true;

    diprg.diph = { sizeof(diprg), sizeof(diprg.diph), DIJOFS_RZ, DIPH_BYOFFSET };
    diprg.lMin = -1000; diprg.lMax = +1000;
    if (dev->SetProperty(DIPROP_RANGE, &diprg.diph) != DI_OK) joyprop.bRudder = false;

    diprw.diph = { sizeof(diprw), sizeof(diprw.diph), DIJOFS_RZ, DIPH_BYOFFSET };
    diprw.dwData = pcfg->CfgJoystickPrm.Deadzone;
    if (dev->SetProperty(DIPROP_DEADZONE, &diprw.diph) != DI_OK) joyprop.bRudder = false;

    // Throttle axis selection — set both ThrottleAxis (new) and ThrottleOfs (legacy).
    DIJOYSTATE2 js2 = {};
    DWORD thaxis;
    switch (pcfg->CfgJoystickPrm.ThrottleAxis) {
    case 1:
        LOGOUT("Joystick throttle: Z-AXIS");
        thaxis = DIJOFS_Z;
        joyprop.ThrottleAxis = ThrottleAxis::Z;
        joyprop.ThrottleOfs  = (int)((BYTE*)&js2.lZ - (BYTE*)&js2);
        break;
    case 2:
        LOGOUT("Joystick throttle: SLIDER 0");
        thaxis = DIJOFS_SLIDER(0);
        joyprop.ThrottleAxis = ThrottleAxis::Slider0;
        joyprop.ThrottleOfs  = (int)((BYTE*)&js2.rglSlider[0] - (BYTE*)&js2);
        break;
    case 3:
        LOGOUT("Joystick throttle: SLIDER 1");
        thaxis = DIJOFS_SLIDER(1);
        joyprop.ThrottleAxis = ThrottleAxis::Slider1;
        joyprop.ThrottleOfs  = (int)((BYTE*)&js2.rglSlider[1] - (BYTE*)&js2);
        break;
    default:
        joyprop.bThrottle = false;
        LOGOUT("Joystick throttle disabled by user");
        return DI_OK;
    }

    diprg.diph = { sizeof(diprg), sizeof(diprg.diph), thaxis, DIPH_BYOFFSET };
    diprg.lMin = -1000; diprg.lMax = 0;
    if ((hr = dev->SetProperty(DIPROP_RANGE, &diprg.diph)) != DI_OK) {
        joyprop.bThrottle = false;
        LOGOUT("No joystick throttle control detected");
        LOGOUT_DIERR(hr);
        return DI_OK;
    }
    LOGOUT("Joystick throttle control detected");

    diprw.diph = { sizeof(diprw), sizeof(diprw.diph), thaxis, DIPH_BYOFFSET };
    diprw.dwData = pcfg->CfgJoystickPrm.ThrottleSaturation;
    if (dev->SetProperty(DIPROP_SATURATION, &diprw.diph) != DI_OK)
        LOGOUT_ERR("Setting joystick throttle saturation failed");

    return DI_OK;
}

#endif // ORBITER_DINPUT
