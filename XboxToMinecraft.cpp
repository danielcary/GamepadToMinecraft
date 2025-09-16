// XboxToMinecraft.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

// Build (MSVC):
//   cl /EHsc /W4 /DNOMINMAX main.cpp /link xinput9_1_0.lib
//
//
// Notes:
// - Run as admin for more reliable input injection in some games.
// - Anti-cheat protected games may block injection—use responsibly.
// - This is a user-mode mapper (no driver). It uses SendInput (Win32).

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Xinput.h>
#include <cstdint>
#include <cmath>
#include <array>
#include <optional>
#include <iostream>
#include <mmsystem.h>

#pragma comment(lib, "xinput9_1_0.lib")
#pragma comment(lib, "winmm.lib") // for timeGetDevCaps

// --------------------------- Config ---------------------------
constexpr int   POLL_MS        = 5;     // main loop period
constexpr float STICK_DEADZONE = 0.22f; // 0..1 normalized deadzone
constexpr float MOUSE_SENS     = 12.0f; // pixels per tick at full tilt
constexpr float TRIGGER_THRESH = 0.15f; // 0..1 normalized trigger threshold

// Helper to normalize a signed 16-bit stick axis to [-1, 1]
static inline float normAxis(SHORT v) {
    // XInput range: -32768 .. 32767
    float f = (v >= 0) ? (float)v / 32767.0f : (float)v / 32768.0f;
    // Deadzone
    if (std::fabs(f) < STICK_DEADZONE) return 0.0f;
    // Rescale so post-deadzone starts at 0
    float sign = (f >= 0.f) ? 1.f : -1.f;
    float m = (std::fabs(f) - STICK_DEADZONE) / (1.f - STICK_DEADZONE);
    if (m < 0.f) m = 0.f;
    return sign * m;
}

// Normalize trigger (0..255) to 0..1
static inline float normTrigger(BYTE t) {
    float f = (float)t / 255.0f;
    return (f >= TRIGGER_THRESH) ? (f - TRIGGER_THRESH) / (1.f - TRIGGER_THRESH) : 0.0f;
}

// ----------------------- SendInput helpers -----------------------
void sendKey(WORD vk, bool down) {
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk;
    in.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    ::SendInput(1, &in, sizeof(INPUT));
}

// Sends a unicode char (optional utility)
void sendChar(wchar_t ch) {
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wScan = ch;
    in.ki.dwFlags = KEYEVENTF_UNICODE;
    ::SendInput(1, &in, sizeof(INPUT));
    in.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    ::SendInput(1, &in, sizeof(INPUT));
}

// Relative mouse move
void sendMouseMove(int dx, int dy) {
    if (dx == 0 && dy == 0) return;
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_MOVE;
    in.mi.dx = dx;
    in.mi.dy = dy;
    ::SendInput(1, &in, sizeof(INPUT));
}

void sendMouseButton(bool left, bool down) {
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = left
        ? (down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP)
        : (down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP);
    ::SendInput(1, &in, sizeof(INPUT));
}

void sendMouseWheel(int delta) { // positive = wheel up, negative = down
    if (delta == 0) return;
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_WHEEL;
    in.mi.mouseData = delta; // typically WHEEL_DELTA=120 per notch
    ::SendInput(1, &in, sizeof(INPUT));
}

// Edge-trigger helper to only fire on transitions
struct Edge {
    bool prev = false;
    bool onPress(bool cur)  { bool out = (cur && !prev); prev = cur; return out; }
    bool onRelease(bool cur){ bool out = (!cur && prev); prev = cur; return out; }
    void set(bool cur) { prev = cur; }
};

// ------------------------- Mapping state -------------------------
struct ButtonEdges {
    Edge A, B, X, Y;
    Edge LB, RB, Back, Start, LS, RS;
    Edge DUp, DDown, DLeft, DRight;
    Edge LClick, RClick; // virtual mouse buttons if you map those
} edges;

// Example mapping: fill these with your own keybinds
void mapButtons(const XINPUT_GAMEPAD& g) {
    auto pressed = [&](WORD mask) -> bool { return (g.wButtons & mask) != 0; };

    // A -> Space
    if (edges.A.onPress(pressed(XINPUT_GAMEPAD_A))) sendKey(VK_SPACE, true);
    if (edges.A.onRelease(pressed(XINPUT_GAMEPAD_A))) sendKey(VK_SPACE, false);

    // B -> Left Ctrl (example)
    if (edges.B.onPress(pressed(XINPUT_GAMEPAD_B))) sendKey(VK_LCONTROL, true);
    if (edges.B.onRelease(pressed(XINPUT_GAMEPAD_B))) sendKey(VK_LCONTROL, false);

    // X -> 'R' (reload example)
    if (edges.X.onPress(pressed(XINPUT_GAMEPAD_X))) sendKey(0x52 /* 'R' */, true);
    if (edges.X.onRelease(pressed(XINPUT_GAMEPAD_X))) sendKey(0x52, false);

    // Y -> 'E' (use example)
    if (edges.Y.onPress(pressed(XINPUT_GAMEPAD_Y))) sendKey(0x45 /* 'E' */, true);
    if (edges.Y.onRelease(pressed(XINPUT_GAMEPAD_Y))) sendKey(0x45, false);

    // LB/RB -> Q/E (or mouse wheel)
    if (edges.LB.onPress(pressed(XINPUT_GAMEPAD_LEFT_SHOULDER)))  sendKey(0x51 /* 'Q' */, true);
    if (edges.LB.onRelease(pressed(XINPUT_GAMEPAD_LEFT_SHOULDER)))sendKey(0x51, false);
    if (edges.RB.onPress(pressed(XINPUT_GAMEPAD_RIGHT_SHOULDER))) sendKey(0x45 /* 'E' */, true);
    if (edges.RB.onRelease(pressed(XINPUT_GAMEPAD_RIGHT_SHOULDER)))sendKey(0x45, false);

    // Back/Start examples: Back = Tab, Start = Esc
    if (edges.Back.onPress(pressed(XINPUT_GAMEPAD_BACK)))  sendKey(VK_TAB, true);
    if (edges.Back.onRelease(pressed(XINPUT_GAMEPAD_BACK)))sendKey(VK_TAB, false);
    if (edges.Start.onPress(pressed(XINPUT_GAMEPAD_START)))  sendKey(VK_ESCAPE, true);
    if (edges.Start.onRelease(pressed(XINPUT_GAMEPAD_START)))sendKey(VK_ESCAPE, false);

    // D-pad to arrow keys
    if (edges.DUp.onPress(pressed(XINPUT_GAMEPAD_DPAD_UP))) sendKey(VK_UP, true);
    if (edges.DUp.onRelease(pressed(XINPUT_GAMEPAD_DPAD_UP))) sendKey(VK_UP, false);
    if (edges.DDown.onPress(pressed(XINPUT_GAMEPAD_DPAD_DOWN))) sendKey(VK_DOWN, true);
    if (edges.DDown.onRelease(pressed(XINPUT_GAMEPAD_DPAD_DOWN))) sendKey(VK_DOWN, false);
    if (edges.DLeft.onPress(pressed(XINPUT_GAMEPAD_DPAD_LEFT))) sendKey(VK_LEFT, true);
    if (edges.DLeft.onRelease(pressed(XINPUT_GAMEPAD_DPAD_LEFT))) sendKey(VK_LEFT, false);
    if (edges.DRight.onPress(pressed(XINPUT_GAMEPAD_DPAD_RIGHT))) sendKey(VK_RIGHT, true);
    if (edges.DRight.onRelease(pressed(XINPUT_GAMEPAD_DPAD_RIGHT))) sendKey(VK_RIGHT, false);

    // TODO: Map LS/RS button clicks if you want them to be keyboard or mouse.
}

// Map sticks to mouse movement and WASD (example)
void mapSticks(const XINPUT_GAMEPAD& g) {
    float lx = normAxis(g.sThumbLX);
    float ly = normAxis(g.sThumbLY);
    float rx = normAxis(g.sThumbRX);
    float ry = normAxis(g.sThumbRY);

    // Right stick -> mouse look
    int mdx = (int)std::lround(rx * MOUSE_SENS);
    int mdy = (int)std::lround(-ry * MOUSE_SENS); // invert Y for typical look
    sendMouseMove(mdx, mdy);

    // Left stick -> WASD (press while tilted past threshold; release when not)
    static Edge W, A, S, D;
    const float t = 0.35f; // press threshold

    bool w = (ly >  t), s = (ly < -t);
    bool a = (lx < -t), d = (lx >  t);

    if (W.onPress(w)) sendKey('W', true);
    if (W.onRelease(w)) sendKey('W', false);
    if (S.onPress(s)) sendKey('S', true);
    if (S.onRelease(s)) sendKey('S', false);
    if (A.onPress(a)) sendKey('A', true);
    if (A.onRelease(a)) sendKey('A', false);
    if (D.onPress(d)) sendKey('D', true);
    if (D.onRelease(d)) sendKey('D', false);
}

// Map triggers to mouse buttons or other actions
void mapTriggers(const XINPUT_GAMEPAD& g) {
    float lt = normTrigger(g.bLeftTrigger);
    float rt = normTrigger(g.bRightTrigger);

    static Edge LT, RT;
    bool ltDown = lt > 0.f;
    bool rtDown = rt > 0.f;

    // Example: LT = right mouse, RT = left mouse
    if (LT.onPress(ltDown)) sendMouseButton(false, true);
    if (LT.onRelease(ltDown)) sendMouseButton(false, false);

    if (RT.onPress(rtDown)) sendMouseButton(true, true);
    if (RT.onRelease(rtDown)) sendMouseButton(true, false);

    // You could also map gradual trigger to mouse wheel or hold-to-run (Shift)
    // TODO: customize as needed.
}

// --------------------------- Main loop ---------------------------
int main() {
    std::cout << "Gamepad → Keyboard/Mouse mapper (C++)\n";
    std::cout << "Press Ctrl+C to exit.\n";

    // Optionally, set high timer resolution
    TIMECAPS tc;
    if (timeGetDevCaps(&tc, sizeof(tc)) == TIMERR_NOERROR) {
        timeBeginPeriod(tc.wPeriodMin);
    }

    XINPUT_STATE state{};
    DWORD padIndex = 0; // 0..3, choose controller 0 by default

    for (;;) {
        DWORD res = XInputGetState(padIndex, &state);
        if (res == ERROR_SUCCESS) {
            const XINPUT_GAMEPAD& g = state.Gamepad;

            // Apply your mappings:
            mapButtons(g);
            mapSticks(g);
            mapTriggers(g);
        }
        else {
            // Controller not connected (you can scan others if you want)
            // Optionally, try other indices 0..3 to find a connected pad.
        }

        ::Sleep(POLL_MS);
    }

    // (Unreachable in this simple loop)
    // timeEndPeriod(tc.wPeriodMin);
    // return 0;
}