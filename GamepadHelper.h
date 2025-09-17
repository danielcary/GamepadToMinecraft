#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Xinput.h>
#include <iostream>
#include <string>



/// <summary>
/// 
/// </summary>
/// <returns>-1 no device, 0 multiple, </returns>
int listXInputPads(int* idx);
void initializeXInput();
