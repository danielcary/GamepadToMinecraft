#include "GamepadHelper.h"



static const char* subtypeName(BYTE s) {
    switch (s) {
    case XINPUT_DEVSUBTYPE_GAMEPAD:        return "Gamepad";
    case XINPUT_DEVSUBTYPE_WHEEL:          return "Racing Wheel";
    case XINPUT_DEVSUBTYPE_ARCADE_STICK:   return "Arcade Stick";
    case XINPUT_DEVSUBTYPE_FLIGHT_STICK:   return "Flight Stick";
    case XINPUT_DEVSUBTYPE_DANCE_PAD:      return "Dance Pad";
    case XINPUT_DEVSUBTYPE_GUITAR:         return "Guitar";
    case XINPUT_DEVSUBTYPE_GUITAR_ALTERNATE:return "Guitar (Alt)";
    case XINPUT_DEVSUBTYPE_DRUM_KIT:       return "Drum Kit";
    case XINPUT_DEVSUBTYPE_GUITAR_BASS:    return "Guitar (Bass)";
    case XINPUT_DEVSUBTYPE_ARCADE_PAD:     return "Arcade Pad";
    default: return "Unknown";
    }
}

static const char* batTypeName(BYTE t) {
    switch (t) {
    case BATTERY_TYPE_DISCONNECTED: return "Disconnected";
    case BATTERY_TYPE_WIRED:        return "Wired";
    case BATTERY_TYPE_ALKALINE:     return "Alkaline";
    case BATTERY_TYPE_NIMH:         return "NiMH";
    case BATTERY_TYPE_UNKNOWN:      return "Unknown";
    default: return "Unknown";
    }
}

static const char* batLevelName(BYTE l) {
    switch (l) {
    case BATTERY_LEVEL_EMPTY: return "Empty";
    case BATTERY_LEVEL_LOW:   return "Low";
    case BATTERY_LEVEL_MEDIUM:return "Medium";
    case BATTERY_LEVEL_FULL:  return "Full";
    default: return "Unknown";
    }
}

using PFN_XInputGetBatteryInformation = decltype(&XInputGetBatteryInformation);
PFN_XInputGetBatteryInformation pGetBattery = nullptr;
HMODULE hXInput = nullptr;

void initializeXInput() {
	if (hXInput) return; // already initialized

    // Try to load XInput DLL (try several versions in order)
    const wchar_t* xinputDLLs[] = {
        L"xinput1_4.dll",    // Windows 8 and later
        L"xinput1_3.dll",    // DirectX SDK
        L"xinput9_1_0.dll",  // Windows Vista, 7
    };
    for (auto dll : xinputDLLs) {
        hXInput = LoadLibraryW(dll);
        if (hXInput) {
            std::wcout << L"Loaded " << dll << L"\n";
            break;
        }
    }
    if (!hXInput) {
        std::cerr << "Failed to load any XInput DLL.\n";
        exit(1);
    }

	// 9_1_0 does not have XInputGetBatteryInformation, so we load it manually from newer DLLs
    pGetBattery = reinterpret_cast<PFN_XInputGetBatteryInformation>(
        GetProcAddress(hXInput, "XInputGetBatteryInformation"));
    if (!pGetBattery) {
        std::cerr << "Failed to load any XInput XInputGetBatteryInformation.\n";
        exit(1);
    }

}

int listXInputPads(int* idx) {
    int cnt = 0;
    std::cout << "XInput controllers:\n";
    for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i) {
        XINPUT_CAPABILITIES caps{};
        if (XInputGetCapabilities(i, XINPUT_FLAG_GAMEPAD, &caps) == ERROR_SUCCESS) {
			// Battery info (works on wireless; wired reports WIRED) known issues with some wireless controllers
            XINPUT_BATTERY_INFORMATION bat{};
            if (pGetBattery(i, BATTERY_DEVTYPE_GAMEPAD, &bat) != ERROR_SUCCESS) {
                bat.BatteryType = BATTERY_TYPE_UNKNOWN;
                bat.BatteryLevel = BATTERY_LEVEL_EMPTY;
            }

            std::cout << "\t[" << i << "] "
                << subtypeName(caps.SubType)
                << "  battery=" << batTypeName(bat.BatteryType)
                << " / " << batLevelName(bat.BatteryLevel)
                << "  caps: ";
            if (caps.Flags & XINPUT_CAPS_FFB_SUPPORTED)     std::cout << "FFB ";
            if (caps.Flags & XINPUT_CAPS_WIRELESS)          std::cout << "Wireless ";
            if (caps.Flags & XINPUT_CAPS_PMD_SUPPORTED)     std::cout << "PMD ";
            if (caps.Flags & XINPUT_CAPS_NO_NAVIGATION)     std::cout << "NoNav ";
            std::cout << "\n";

            cnt++;
            *idx = i;
        }
        else {
            std::cout << " \t[" << i << "] (no device)\n";
        }
        if (cnt == 0) return -1; // no devices
        if (cnt  > 1) return 0;  // multiple device
		return cnt; // return the only device index

    }

}
