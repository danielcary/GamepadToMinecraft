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
#include <functional>
#include "VInput.h"

#pragma comment(lib, "xinput9_1_0.lib")
#pragma comment(lib, "winmm.lib") // for timeGetDevCaps


static bool sensitivityToggle = false;
constexpr int   POLL_MS        = 4;     // main loop period


// ---------------------------- Types -------------------

enum Button
{
    BUTTON_JUMP,
    BUTTON_INVENTORY,
    BUTTON_DROP,
    BUTTON_SNEAK,
    BUTTON_LHOTBAR,
    BUTTON_RHOTBAR,
    BUTTON_PLAYERLIST,
    BUTTON_MENU,
    BUTTON_RUN,
    BUTTON_ARROWUP,
    BUTTON_ARROWDOWN,
    BUTTON_ARROWLEFT,
    BUTTON_ARROWRIGHT,
    // Custom button for cursor sensitivity toggle
    BUTTON_SENSITIVITY,
    BUTTON_CNT,
};

using MappingArray = std::function<void(bool)>[Button::BUTTON_CNT];

// --------------------------- Mappings ---------------------------

// these are example mappings for Minecraft Java Edition (as of 1.20)
static void send_jump(bool isDown) { sendKeyScan(VK_SPACE, isDown); }
static void send_sneak(bool isDown) { sendKeyScan(VK_LSHIFT, isDown); }
static void send_useitem(bool isDown) { sendMouseButton(false, isDown); }
static void send_attack(bool isDown) { sendMouseButton(true, isDown); }
static void send_drop(bool isDown) { sendKeyScan('Q', isDown); }
static void send_inventory(bool isDown) { sendKeyScan('E', isDown); }
static void send_run(bool isDown) { sendKeyScan(VK_LCONTROL, isDown); }
static void send_lhotbar(bool isDown) { if(isDown) sendMouseWheel(+120); }
static void send_rhotbar(bool isDown) { if (isDown) sendMouseWheel(-120); }
static void send_playerlist(bool isDown) { sendKeyScan(VK_TAB, isDown); }
static void send_menu(bool isDown) { sendKeyScan(VK_ESCAPE, isDown); }
static void send_arrowup(bool isDown) { sendKeyScan(VK_UP, isDown); }
static void send_arrowdown(bool isDown) { sendKeyScan(VK_DOWN, isDown); }
static void send_arrowleft(bool isDown) { sendKeyScan(VK_LEFT, isDown); }
static void send_arrowright(bool isDown) { sendKeyScan(VK_RIGHT, isDown); }

static void send_sensitivity(bool isDown) { if (isDown) sensitivityToggle = !sensitivityToggle; }


class Gamepad
{
private:
    const MappingArray* mappings = nullptr;

public:
    bool init(const MappingArray* mapping)
    {
		this->mappings = mapping;
        return this->_init();
    }
    virtual void update() = 0;

protected:
	virtual bool _init() = 0;
    virtual bool onPress(Button button) = 0;
    virtual bool onRelease(Button button) = 0;

    void mapButtons() {
        for (int i = 0; i < Button::BUTTON_CNT; i++)
        {
            if (this->onPress((Button)i)) (*this->mappings)[i](true);
            if (this->onRelease((Button)i)) (*this->mappings)[i](false);
        }
    }
};

 class XInputGamepad : public Gamepad
 {
 private:
     int padIndex = 0; // only one supported for now
	 XINPUT_STATE g = XINPUT_STATE{ 0 };
     WORD prev = 0;        // last frame’s wButtons
     WORD cur = 0;
     WORD pressed = 0, released = 0;
     bool _onPress(WORD mask) const { return (pressed & mask) != 0; };
     bool _onRelease(WORD mask) const { return (released & mask) != 0; };

     WORD MAPPINGS[BUTTON_CNT] = {
         XINPUT_GAMEPAD_A,              // BUTTON_JUMP
		 XINPUT_GAMEPAD_Y,              // BUTTON_INVENTORY
		 XINPUT_GAMEPAD_X,              // BUTTON_DROP
		 XINPUT_GAMEPAD_B,              // BUTTON_SNEAK
		 XINPUT_GAMEPAD_LEFT_SHOULDER,  // BUTTON_LHOTBAR
         XINPUT_GAMEPAD_RIGHT_SHOULDER, // BUTTON_RHOTBAR
         XINPUT_GAMEPAD_BACK,           // BUTTON_PLAYERLIST
         XINPUT_GAMEPAD_START,          // BUTTON_MENU
         XINPUT_GAMEPAD_LEFT_THUMB,     // BUTTON_RUN
         XINPUT_GAMEPAD_DPAD_UP,        // BUTTON_ARROWUP
		 XINPUT_GAMEPAD_DPAD_DOWN,      // BUTTON_ARROWDOWN
		 XINPUT_GAMEPAD_DPAD_LEFT,      // BUTTON_ARROWLEFT
		 XINPUT_GAMEPAD_DPAD_RIGHT,     // BUTTON_ARROWRIGHT
		 XINPUT_GAMEPAD_RIGHT_THUMB,    // BUTTON_SENSITIVITY
     };

     bool _init() override {
         initializeXInput();
         int cntrlCnt = listXInputPads(&padIndex);
         if (cntrlCnt <= -1) {
             std::cerr << "No XInput controllers found. Exiting.\n";
             return true;
         }
         else if (cntrlCnt == 0) {
             // TODO: select screen for multiple controllers
			 throw std::runtime_error("Multiple controllers. Not yet implemented.\n");
         }
         else {
             std::cout << "Using controller index " << padIndex << "\n";
         }

         return false;
     }

 public:
     void update() override
     {
         DWORD res = XInputGetState(padIndex, &g);
         if (res == ERROR_SUCCESS) {
             cur = g.Gamepad.wButtons;
             pressed = (cur & ~prev);        // bits that just turned on
             released = (~cur & prev);        // bits that just turned off
             prev = cur;

             this->mapButtons();
             this->mapSticks();
             this->mapTriggers();
         }
     }

     bool onPress(Button button) override
     {
         return this->_onPress(MAPPINGS[button]);
     }

     bool onRelease(Button button) override
     {
         return this->_onRelease(MAPPINGS[button]);
     }
 private:
      const WORD WASD_MASK_W = 0x0001;
      WORD WASD_MASK_A = 0x0002;
      WORD WASD_MASK_S = 0x0004;
      WORD WASD_MASK_D = 0x0008;

     // Map sticks to mouse movement and WASD (example)
     void mapSticks() const {
         static WORD prev = 0;
         WORD cur = 0, pressed, released;
         auto onPress = [&](WORD mask) { return (pressed & mask) != 0; };
         auto onRelease = [&](WORD mask) { return (released & mask) != 0; };


         // Right stick -> mouse look
         float lx = normAxis(this->g.Gamepad.sThumbLX);
         float ly = normAxis(this->g.Gamepad.sThumbLY);
         float rx = normAxis(this->g.Gamepad.sThumbRX);
         float ry = normAxis(this->g.Gamepad.sThumbRY);

         float mouse_sens = MOUSE_SENS * (sensitivityToggle ? 0.1f : 1.0f);
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
     void mapTriggers() const {
         float lt = normTrigger(this->g.Gamepad.bLeftTrigger);
         float rt = normTrigger(this->g.Gamepad.bRightTrigger);

         static bool prevLT = false;
         static bool prevRT = false;

         bool curLT = lt > 0.f;
         bool curRT = rt > 0.f;

         if (curLT && !prevLT) {
             send_useitem(true);   // press Use Item (right mouse)
         }
         if (!curLT && prevLT) {
             send_useitem(false);  // release Use Item (right mouse)
         }

         // RT = left mouse
         if (curRT && !prevRT) {
             send_attack(true);   // press
         }
         if (!curRT && prevRT) {
             send_attack(false);  // release
         }

         prevLT = curLT;
         prevRT = curRT;
     }

 };

 const MappingArray mappings_default = {
     send_jump,         // BUTTON_JUMP
     send_inventory,    // BUTTON_INVENTORY
	 send_drop,         // BUTTON_DROP
	 send_sneak,        // BUTTON_SNEAK
	 send_lhotbar,      // BUTTON_LHOTBAR
     send_rhotbar,      // BUTTON_RHOTBAR
     send_playerlist,   // BUTTON_PLAYERLIST
     send_menu,         // BUTTON_MENU
     send_run,          // BUTTON_RUN
     send_arrowup,      // BUTTON_ARROWUP
	 send_arrowdown,    // BUTTON_ARROWDOWN
	 send_arrowleft,    // BUTTON_ARROWLEFT
	 send_arrowright,   // BUTTON_ARROWRIGHT
     send_sensitivity,  // BUTTON_SENSITIVITY
 };

  const MappingArray mappings_alt =  {
    send_jump,          // BUTTON_JUMP
    send_inventory,     // BUTTON_INVENTORY
    send_sensitivity,   // BUTTON_DROP         ****
    send_drop,          // BUTTON_SNEAK        ****
    send_lhotbar,       // BUTTON_LHOTBAR
    send_rhotbar,       // BUTTON_RHOTBAR
    send_playerlist,    // BUTTON_PLAYERLIST
    send_menu,          // BUTTON_MENU
    send_run,           // BUTTON_RUN
    send_arrowup,       // BUTTON_ARROWUP
    send_arrowdown,     // BUTTON_ARROWDOWN
    send_arrowleft,     // BUTTON_ARROWLEFT
    send_arrowright,    // BUTTON_ARROWRIGHT
    send_sneak,         // BUTTON_SENSITIVITY  ****
 };

// --------------------------- Main loop ---------------------------
 int main(int argc, const char* argv[]) {
     Gamepad* g;
     TIMECAPS tc;

     g = new XInputGamepad();
     const MappingArray* mapping = &mappings_default;
     for (int i = 0; i < argc; i++) {
         if (strcmp(argv[i], "--alt") == 0 || strcmp(argv[i], "-a") == 0) {
             std::cout << "Using alternative button layout\n";
             mapping = &mappings_alt; // already set
         }
	 }
     g->init(mapping);

     std::cout << "Welcome to Gamepad to Keyboard/Mouse mapper\n";
     std::cout << "Press Ctrl+C to exit.\n";

     // Use high resolution timer if avaialble for MS res polling
     if (timeGetDevCaps(&tc, sizeof(tc)) == TIMERR_NOERROR) {
         timeBeginPeriod(tc.wPeriodMin);
     }

     for (;;) {
         g->update();
         ::Sleep(POLL_MS);
     }

     if (timeGetDevCaps(&tc, sizeof(tc)) == TIMERR_NOERROR) {
         timeEndPeriod(tc.wPeriodMin);
     }

     delete g;
     return 0;
 }