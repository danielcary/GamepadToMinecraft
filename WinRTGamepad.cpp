#include "VInput.h"


#include <atomic>
#include <cstdint>
#include <optional>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Gaming.Input.h>


#pragma comment(lib, "windowsapp.lib")

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace Windows::Gaming::Input;

static std::atomic<bool> g_havePad{ false };
static Gamepad g_pad{ nullptr };
static event_token s_added, s_removed;

void initWinRTGamepad()
{
    winrt::init_apartment(); // OK to call once in your program init


    // Subscribe with the exact parameter types:
    s_added = Gamepad::GamepadAdded(
        EventHandler<Gamepad>(
            [](IInspectable const&, Gamepad const& pad)
            {
                if (!g_havePad) { g_pad = pad; g_havePad = true; }
            }
        )
    );

    s_removed = Gamepad::GamepadRemoved(
        EventHandler<Gamepad>(
            [](IInspectable const&, Gamepad const& pad)
            {
                if (g_havePad && pad == g_pad) { g_pad = nullptr; g_havePad = false; }
            }
        )
    );

}

static int listWinRTPads(int* idx)
{
    // Pick first available at startup
    if (Gamepad::Gamepads().Size() > 0) {
        g_pad = Gamepad::Gamepads().GetAt(0);
        g_havePad = true;
    }


    return 0; //TODO
}


// Convert WinRT buttons to a bitmask we can edge-detect like XInput
struct RtState {
    uint32_t buttons = 0;    // our own mask
    float lt = 0.f, rt = 0.f;
    float lx = 0.f, ly = 0.f, rx = 0.f, ry = 0.f;
};
enum : uint32_t {
    BTN_A = 1 << 0, BTN_B = 1 << 1, BTN_X = 1 << 2, BTN_Y = 1 << 3,
    BTN_LB = 1 << 4, BTN_RB = 1 << 5, BTN_BACK = 1 << 6, BTN_START = 1 << 7,
    BTN_LS = 1 << 8, BTN_RS = 1 << 9,
    BTN_DU = 1 << 10, BTN_DD = 1 << 11, BTN_DL = 1 << 12, BTN_DR = 1 << 13,
};

static RtState readRt()
{
    RtState out{};
    if (!g_havePad) return out;

    auto r = g_pad.GetCurrentReading();
    
    // Buttons: these flags exist even for non-Xbox pads; mapping quality depends on driver.
    auto b = r.Buttons;
    auto set = [&](bool v, uint32_t bit) { if (v) out.buttons |= bit; };
    using GB = GamepadButtons;
    set((b & GB::A) != GB::None, BTN_A);
    set((b & GB::B) != GB::None, BTN_B);
    set((b & GB::X) != GB::None, BTN_X);
    set((b & GB::Y) != GB::None, BTN_Y);
    set((b & GB::LeftShoulder) != GB::None, BTN_LB);
    set((b & GB::RightShoulder) != GB::None, BTN_RB);
    set((b & GB::View) != GB::None, BTN_BACK);   // “Back / View”
    set((b & GB::Menu) != GB::None, BTN_START);  // “Start / Menu”
    set((b & GB::LeftThumbstick) != GB::None, BTN_LS);
    set((b & GB::RightThumbstick) != GB::None, BTN_RS);
    set((b & GB::DPadUp) != GB::None, BTN_DU);
    set((b & GB::DPadDown) != GB::None, BTN_DD);
    set((b & GB::DPadLeft) != GB::None, BTN_DL);
    set((b & GB::DPadRight) != GB::None, BTN_DR);

    // Axes (already normalized)
    out.lt = static_cast<float>(r.LeftTrigger);   // 0..1
    out.rt = static_cast<float>(r.RightTrigger);  // 0..1
    out.lx = static_cast<float>(r.LeftThumbstickX);  // -1..1
    out.ly = static_cast<float>(r.LeftThumbstickY);
    out.rx = static_cast<float>(r.RightThumbstickX);
    out.ry = static_cast<float>(r.RightThumbstickY);
    return out;
}