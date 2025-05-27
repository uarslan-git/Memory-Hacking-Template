#include <windows.h>
#include <iostream>
#include "vars.h"

extern DWORD64 skyrimModuleBase = (DWORD64)GetModuleHandle(L"SkyrimSE.exe");
extern DWORD64 localPlayerBaseAddress = skyrimModuleBase + 0x31874F8;
extern DWORD64 hookedLocalPlayerBaseAddress = 0;
extern Player* localPlayerPtr = *(Player**)(localPlayerBaseAddress);