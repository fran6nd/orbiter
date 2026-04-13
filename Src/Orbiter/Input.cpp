// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// =======================================================================
// Input.cpp
// Input manager implementation.
//
// Conditionally compiled:
//   ORBITER_DINPUT=1  — full DirectInput back-end (Windows, no SDL3 override)
//   ORBITER_DINPUT=0  — keyboard state collected from WM_KEYDOWN/WM_KEYUP
//                       via OnKey(); joystick not yet wired.
// =======================================================================

#include "Input.h"
#include "Log.h"
#include "Orbiter.h"

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
#else
    (void)state;
    return false;
#endif
}

// ---------------------------------------------------------------------------
// SDL3 / non-DInput keyboard implementation
// ---------------------------------------------------------------------------

#if !ORBITER_DINPUT

bool DInput::CreateKbdDevice()
{
    // No explicit device creation needed — state is collected via OnKey().
    return true;
}

void DInput::OnKey(unsigned long oapi_key, bool pressed)
{
    if (oapi_key == 0 || oapi_key >= 256) return;

    if (pressed) {
        bool was_up = !(m_kbdState[oapi_key] & 0x80);
        m_kbdState[oapi_key] = 0x80;
        // Only emit a buffered DOWN event on the first press, not on auto-repeat.
        if (was_up && m_kbdEventCount < KBD_EVENT_BUF) {
            DIDEVICEOBJECTDATA &ev = m_kbdEvents[m_kbdEventCount++];
            ev.dwOfs       = oapi_key;
            ev.dwData      = 0x80;
            ev.dwTimeStamp = GetTickCount();
            ev.uAppData    = 0;
        }
    } else {
        m_kbdState[oapi_key] = 0x00;
        if (m_kbdEventCount < KBD_EVENT_BUF) {
            DIDEVICEOBJECTDATA &ev = m_kbdEvents[m_kbdEventCount++];
            ev.dwOfs       = oapi_key;
            ev.dwData      = 0x00;
            ev.dwTimeStamp = GetTickCount();
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
