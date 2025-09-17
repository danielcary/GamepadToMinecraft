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
#include "GamepadHelper.h"

#pragma comment(lib, "xinput9_1_0.lib")
#pragma comment(lib, "winmm.lib") // for timeGetDevCaps

// --------------------------- Config ---------------------------
constexpr int   POLL_MS        = 5;     // main loop period
constexpr float STICK_DEADZONE = 0.22f; // 0..1 normalized deadzone
constexpr float MOUSE_SENS     = 10.0f; // pixels per tick at full tilt
constexpr float TRIGGER_THRESH = 0.15f; // 0..1 normalized trigger threshold
constexpr float STICK_WASD_THRES = 0.35f; // press threshold

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

void sendKeyScan(WORD vk, bool down) {
    INPUT in{};
    in.type = INPUT_KEYBOARD;

    // Map VK -> scan code
    UINT sc = MapVirtualKey(vk, MAPVK_VK_TO_VSC);
    in.ki.wVk = 0;                 // When using SCANCODE, wVk should be 0
    in.ki.wScan = static_cast<WORD>(sc);
    in.ki.dwFlags = KEYEVENTF_SCANCODE | (down ? 0 : KEYEVENTF_KEYUP);

    // Extended-key fixups (not needed for TAB, but good to have)
    // e.g., arrow keys, Insert/Delete/Home/End/PageUp/PageDown, Numpad slash, RAlt, etc.
    switch (vk) {
    case VK_LEFT: case VK_RIGHT:
    case VK_UP: case VK_DOWN:
    case VK_PRIOR: case VK_NEXT:     // PageUp/PageDown
    case VK_END: case VK_HOME:
    case VK_INSERT: case VK_DELETE:
    case VK_DIVIDE:                  // Numpad /
    case VK_RMENU:                   // Right Alt
    case VK_RCONTROL:                // Right Ctrl
        in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        break;
    default: break;
    }

    SendInput(1, &in, sizeof(INPUT));
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

// --------------------------- Mappings ---------------------------


// Example mapping: fill these with your own keybinds
void mapButtons(const XINPUT_GAMEPAD& g, bool* rsPressed) {
    static WORD prev = 0;                 // last frame’s wButtons
    WORD cur = g.wButtons;
    WORD pressed = (cur & ~prev);        // bits that just turned on
    WORD released = (~cur & prev);        // bits that just turned off
    prev = cur;

    auto onPress = [&](WORD mask) { return (pressed & mask) != 0; };
    auto onRelease = [&](WORD mask) { return (released & mask) != 0; };

	// idk will probably want to change these so that the user can customize them
	// also RS button toggles mouse sensitivity modifier doesnt feel right, maybe a button swap

    // A -> 'Space' (jump)
    if (onPress(XINPUT_GAMEPAD_A)) sendKeyScan(VK_SPACE, true);
    if (onRelease(XINPUT_GAMEPAD_A)) sendKeyScan(VK_SPACE, false);

    // Y -> 'E' (inventory)
    if (onPress(XINPUT_GAMEPAD_Y)) sendKeyScan('E', true);
    if (onRelease(XINPUT_GAMEPAD_Y)) sendKeyScan('E', false);

    // X -> 'Q' (drop)
    if (onPress(XINPUT_GAMEPAD_X)) sendKeyScan('Q', true);
    if (onRelease(XINPUT_GAMEPAD_X)) sendKeyScan('Q', false);

    // B -> 'Right Click' (attack)
    if (onPress(XINPUT_GAMEPAD_B)) sendMouseButton(false, true);
    if (onRelease(XINPUT_GAMEPAD_B)) sendMouseButton(false, false);

	// LB/RB -> 'mouse wheel' (scroll hotbar)
    if (onPress(XINPUT_GAMEPAD_LEFT_SHOULDER))  sendMouseWheel(+120);
    if (onPress(XINPUT_GAMEPAD_RIGHT_SHOULDER))  sendMouseWheel(-120);

    // Back/Start examples: Back = Tab, Start = Esc
    if (onPress(XINPUT_GAMEPAD_BACK))  sendKeyScan(VK_TAB, true);
    if (onRelease(XINPUT_GAMEPAD_BACK)) sendKeyScan(VK_TAB, false);
    if (onPress(XINPUT_GAMEPAD_START))   sendKeyScan(VK_ESCAPE, true);
    if (onPress(XINPUT_GAMEPAD_START)) sendKeyScan(VK_ESCAPE, false);

    // LS button clicks (run)
    if (onPress(XINPUT_GAMEPAD_LEFT_THUMB))  sendKeyScan(VK_LCONTROL, true);
    if (onRelease(XINPUT_GAMEPAD_LEFT_THUMB)) sendKeyScan(VK_LCONTROL, false);

	// RS Button toggles mouse sensitivity modifier
    if (onRelease(XINPUT_GAMEPAD_RIGHT_THUMB))  *rsPressed = *rsPressed ? false : true;

    //// D-pad to arrow keys
    if (onPress(XINPUT_GAMEPAD_DPAD_UP)) sendKeyScan(VK_UP, true);
    if (onRelease(XINPUT_GAMEPAD_DPAD_UP)) sendKeyScan(VK_UP, false);
    if (onPress(XINPUT_GAMEPAD_DPAD_DOWN)) sendKeyScan(VK_DOWN, true);
    if (onRelease(XINPUT_GAMEPAD_DPAD_DOWN)) sendKeyScan(VK_DOWN, false);
    if (onPress(XINPUT_GAMEPAD_DPAD_LEFT)) sendKeyScan(VK_LEFT, true);
    if (onRelease(XINPUT_GAMEPAD_DPAD_LEFT)) sendKeyScan(VK_LEFT, false);
    if (onPress(XINPUT_GAMEPAD_DPAD_RIGHT)) sendKeyScan(VK_RIGHT, true);
    if (onRelease(XINPUT_GAMEPAD_DPAD_RIGHT)) sendKeyScan(VK_RIGHT, false);
}

constexpr WORD WASD_MASK_W = 0x0001;
constexpr WORD WASD_MASK_A = 0x0002;
constexpr WORD WASD_MASK_S = 0x0004;
constexpr WORD WASD_MASK_D = 0x0008;

// Map sticks to mouse movement and WASD (example)
void mapSticks(const XINPUT_GAMEPAD& g, bool rsPressed) {
    static WORD prev = 0;
    WORD cur = 0, pressed, released;
    auto onPress = [&](WORD mask) { return (pressed & mask) != 0; };
    auto onRelease = [&](WORD mask) { return (released & mask) != 0; };

    
    // Right stick -> mouse look
    float lx = normAxis(g.sThumbLX);
    float ly = normAxis(g.sThumbLY);
    float rx = normAxis(g.sThumbRX);
    float ry = normAxis(g.sThumbRY);

	float mouse_sens = MOUSE_SENS * (rsPressed ? 0.1f : 1.0f);
    int mdx = (int)std::lround(rx * mouse_sens);
    int mdy = (int)std::lround(-ry * mouse_sens); // invert Y for typical look
    sendMouseMove(mdx, mdy);

    // Left stick -> WASD (press while tilted past threshold; release when not)
	cur |= (ly > STICK_WASD_THRES) ? WASD_MASK_W : 0;
	cur |= (ly < -STICK_WASD_THRES) ? WASD_MASK_S : 0;
	cur |= (lx < -STICK_WASD_THRES) ? WASD_MASK_A : 0;
	cur |= (lx > STICK_WASD_THRES) ? WASD_MASK_D : 0;
    pressed = (cur & ~prev);        // bits that just turned on
    released = (~cur & prev);        // bits that just turned off
    prev = cur;

    if (onPress(WASD_MASK_W)) sendKeyScan('W', true);
    if (onRelease(WASD_MASK_W)) sendKeyScan('W', false);
    if (onPress(WASD_MASK_S)) sendKeyScan('S', true);
    if (onRelease(WASD_MASK_S)) sendKeyScan('S', false);
    if (onPress(WASD_MASK_A)) sendKeyScan('A', true);
    if (onRelease(WASD_MASK_A)) sendKeyScan('A', false);
    if (onPress(WASD_MASK_D)) sendKeyScan('D', true);
    if (onRelease(WASD_MASK_D)) sendKeyScan('D', false);
}

// Map triggers to mouse buttons or other actions
void mapTriggers(const XINPUT_GAMEPAD& g) {
    float lt = normTrigger(g.bLeftTrigger);
    float rt = normTrigger(g.bRightTrigger);

    static bool prevLT = false;
    static bool prevRT = false;

    bool curLT = lt > 0.f;
    bool curRT = rt > 0.f;

    // LT = right mouse
    if (curLT && !prevLT) {
        sendKeyScan(VK_LSHIFT, true); // press Shift (sneak)
    }
    if (!curLT && prevLT) {
        sendKeyScan(VK_LSHIFT, false); // press Shift (sneak)
    }

    // RT = left mouse
    if (curRT && !prevRT) {
        sendMouseButton(true, true);   // press
    }
    if (!curRT && prevRT) {
        sendMouseButton(true, false);  // release
    }

    prevLT = curLT;
    prevRT = curRT;
}

// --------------------------- Main loop ---------------------------
int main() {
    int padIndex = 0;

	initializeXInput();
    std::cout << "Welcome to Gamepad to Keyboard/Mouse mapper (C++)\n";
    int cntrlCnt = listXInputPads(&padIndex);
    if (cntrlCnt <= -1) {
        std::cerr << "No XInput controllers found. Exiting.\n";
        return 1;
    }
    else if (cntrlCnt == 0) {
		// select screen for multiple controllers
    }
    else {
		std::cout << "Using controller index " << padIndex << "\n";
    }

    std::cout << "Press Ctrl+C to exit.\n";

    // Optionally, set high timer resolution
    TIMECAPS tc;
    if (timeGetDevCaps(&tc, sizeof(tc)) == TIMERR_NOERROR) {
        timeBeginPeriod(tc.wPeriodMin);
    }

    XINPUT_STATE state{};


    bool rsPressed[XUSER_MAX_COUNT];
    memset(rsPressed, false, sizeof(bool));

    for (;;) {
        DWORD res = XInputGetState(padIndex, &state);
        if (res == ERROR_SUCCESS) {
            const XINPUT_GAMEPAD& g = state.Gamepad;

            // Apply your mappings:
            mapButtons(g, &rsPressed[padIndex]);
            mapSticks(g, rsPressed[padIndex]);
            mapTriggers(g);
        }
        else {
            // Controller not connected (you can scan others if you want)
            // Optionally, try other indices 0..3 to find a connected pad.
        }

        ::Sleep(POLL_MS);
    }

    timeEndPeriod(tc.wPeriodMin);
    return 0;
}