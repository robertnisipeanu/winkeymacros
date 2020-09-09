#include "LowLevelKeyboard.h"

HMODULE hInstance;
HWND hWnd;
HHOOK keyboardHook;
bool (*LlkPress_Callback)(struct KeyboardEventArgs args);

UINT const WM_HOOK = WM_APP + 1;
DWORD maxWaitingTime = 1000;
std::deque<DecisionRecord> decisionBuffer;

std::ofstream debugFile;

std::vector<KeyboardStruct> keyboardList;

const wchar_t *secondKeyboardDeviceName = L"\\\\?\\HID#VID_04F2&PID_1516&MI_00#8&153530d8&0&0000#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}";

DWORD WINAPI Llk(HMODULE hModule) {

    hInstance = hModule;

    debugFile.open("debug.txt");

    WNDCLASSEX wx;
    MSG msg;

    wx.cbSize = sizeof(WNDCLASSEX);
    wx.lpfnWndProc = &WndProc;
    wx.hInstance = GetModuleHandle(NULL);
    wx.lpszClassName = TEXT("LlkRawInput");

    if (!RegisterClassEx(&wx)) {
        return 1;
    }

    hWnd = CreateWindowEx(0, wx.lpszClassName, NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, wx.hInstance, NULL);

    if (!hWnd) {
        return 2;
    }

    RAWINPUTDEVICE Rid[1];
    
    Rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
    Rid[0].usUsage = HID_USAGE_GENERIC_KEYBOARD;
    Rid[0].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
    Rid[0].hwndTarget = hWnd;

    if (RegisterRawInputDevices(Rid, 1, sizeof(Rid[0])) == FALSE) {
        return 3;
    }

    RefreshDevices();

    keyboardHook = SetWindowsHookEx(WH_KEYBOARD, KeyProc, hModule, 0);

    while (GetMessage(&msg, NULL, 0, 0) != 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    UnhookWindowsHookEx(keyboardHook);
    debugFile.close();

    return 0;
}

void LlkPress_Callback_Register(bool(*cb)(struct KeyboardEventArgs args)) {
    LlkPress_Callback = cb;
}

void RefreshDevices() {

    keyboardList.clear(); // Clear previous list

    // Get connected devices
    UINT nDevices;
    UINT result = GetRawInputDeviceList(NULL, &nDevices, sizeof(RAWINPUTDEVICELIST));
    if (result == 0 || result == ERROR_INSUFFICIENT_BUFFER) {
        std::vector<RAWINPUTDEVICELIST> newlist(nDevices);
        nDevices = GetRawInputDeviceList(&newlist[0], &nDevices, sizeof(RAWINPUTDEVICELIST));

        // Loop through devices
        for (auto it : newlist) {

            // Check if it's a keyboard
            if (it.dwType == RIM_TYPEKEYBOARD) {

                // Create keyboard struct
                struct KeyboardStruct kb;

                // Get keyboard hangle
                kb.Handle = it.hDevice;
                
                // Get keyboard device name
                UINT dnSize = 1024;
                TCHAR dnLocalBuffer[1024] = { 0 };
                if (GetRawInputDeviceInfo(it.hDevice, RIDI_DEVICENAME, dnLocalBuffer, &dnSize) < 0) {
                    strcpy_s(kb.DeviceName, sizeof(kb.DeviceName), "UNKNOWN");
                }
                else {
                    // Convert from TCHAR to char
                    size_t nNumCharConverted;
                    wcstombs_s(&nNumCharConverted, kb.DeviceName, sizeof(kb.DeviceName), dnLocalBuffer, dnSize);
                }

                // Push keyboard in list
                keyboardList.push_back(kb);

            }
        }
    }
}

bool LlkPress_GetKeyboardStruct(HANDLE handle, struct KeyboardStruct* kbS) {
    for (const auto& it : keyboardList) {
        if (it.Handle == handle) {
            *kbS = it;
            return true;
        }
    }

    return false;
}