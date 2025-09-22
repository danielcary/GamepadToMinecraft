#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cmath>

constexpr float STICK_DEADZONE = 0.22f; // 0..1 normalized deadzone
constexpr float MOUSE_SENS = 10.0f; // pixels per tick at full tilt
constexpr float TRIGGER_THRESH = 0.15f; // 0..1 normalized trigger threshold
constexpr float STICK_WASD_THRES = 0.35f; // press threshold

extern inline float normAxis(SHORT v);
extern inline float normTrigger(BYTE t);
extern void sendKeyScan(WORD vk, bool down);
extern void sendMouseMove(int dx, int dy);
extern void sendMouseButton(bool left, bool down);
/// <param name="delta">positive = wheel up, negative = down</param>
extern void sendMouseWheel(int delta);


void initWinRTGamepad();
int listWinRTPads(int* idx);
/// <summary>
/// 
/// </summary>
/// <returns>-1 no device, 0 multiple, </returns>
int listXInputPads(int* idx);
void initializeXInput();
