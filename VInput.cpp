#include "VInput.h"

// Helper to normalize a signed 16-bit stick axis to [-1, 1]
inline float normAxis(SHORT v) {
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
inline float normTrigger(BYTE t) {
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

void sendMouseWheel(int delta) 
{ 
    if (delta == 0) return;
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_WHEEL;
    in.mi.mouseData = delta; // typically WHEEL_DELTA=120 per notch
    ::SendInput(1, &in, sizeof(INPUT));
}
