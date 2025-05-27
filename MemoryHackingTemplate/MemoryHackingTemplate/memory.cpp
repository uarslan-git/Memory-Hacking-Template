#include <Windows.h>
#include <string>
#include <iostream>
#include <Psapi.h>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include "vars.h"

#include <ProcessSnapshot.h>

uintptr_t getGameBaseAddress() {
    // When your DLL is injected, GetModuleHandle(NULL) will return the base address
    // of the main executable (.exe) of the process.
    HMODULE hGameModule = GetModuleHandle(NULL);
    if (hGameModule == nullptr) {
        return 0;
    }
    return reinterpret_cast<uintptr_t>(hGameModule);
}

uintptr_t getModuleBaseAddress(const std::string& moduleName) {
    HMODULE hModule = GetModuleHandleA(moduleName.c_str());
    if (hModule == nullptr) {
        return 0;
    }
    return reinterpret_cast<uintptr_t>(hModule);
}

// To get the size of a module:
size_t getModuleSize(uintptr_t baseAddress, const std::string& moduleName) {
    HMODULE hModule = GetModuleHandleA(moduleName.c_str());
    if (hModule) {
        MODULEINFO moduleInfo;
        if (GetModuleInformation(GetCurrentProcess(), hModule, &moduleInfo, sizeof(MODULEINFO))) {
            return moduleInfo.SizeOfImage;
        }
    }
    return 0;
}

std::vector<uint8_t> hexStringToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        uint8_t byte = (uint8_t)strtol(byteString.c_str(), NULL, 16);
        bytes.push_back(byte);
    }
    return bytes;
}

std::string byteArrayToHexString(const uint8_t* data, size_t size) {
    std::stringstream ss;
    for (size_t i = 0; i < size; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i] << " ";
    }
    return ss.str();
}

uintptr_t sigScan(uintptr_t baseAddress, size_t size, const std::string& signature, const std::string& mask) {
    if (signature.empty() || mask.empty() || signature.length() != mask.length()) {
        return 0; // Invalid input
    }

    std::vector<uint8_t> signatureBytes = hexStringToBytes(signature);
    const uint8_t* data = reinterpret_cast<const uint8_t*>(baseAddress);

    for (size_t i = 0; i < size - signatureBytes.size(); ++i) {
        bool found = true;
        for (size_t j = 0; j < signatureBytes.size(); ++j) {
            if (mask[j] != '?' && data[i + j] != signatureBytes[j]) {
                found = false;
                break;
            }
        }
        if (found) {
            return baseAddress + i;
        }
    }
    return 0; // Not found
}

void PlaceJMP32(BYTE* address, DWORD destination, DWORD length) {
    DWORD oldProtect, backup, relAddr;

    VirtualProtect(address, length, PAGE_EXECUTE_READWRITE, &oldProtect);

    relAddr = (DWORD)(destination - (DWORD)address) - 5; //-5 because rel32 is 5 bytes long (1 byte for 0xE9 and 4 bytes for rel32) and that one is calculated from the next instructions address (address + 5)

    *address = 0xE9; //Relative near jump -> 0xE9 JMP rel32

    *((DWORD*)(address + 0x1)) = relAddr; //next one rel32 is reladdress

    for (DWORD x = 0x5; x < length; x++) {
        *(address + x) = 0x90;
    } //fill the rest of the bytes to do nothing 
    VirtualProtect(address, length, oldProtect, &backup);
}

void PlaceJMP64(BYTE* address, void* destination, DWORD length) {
    DWORD oldProtect;
    VirtualProtect(address, length, PAGE_EXECUTE_READWRITE, &oldProtect);

    // We need at least 14 bytes for a safe absolute jump in x64
    if (length < 14) {
        // You can choose to error out or pad manually
        VirtualProtect(address, length, oldProtect, &oldProtect);
        return;
    }

    // Absolute jump: mov rax, <destination>; jmp rax
    address[0] = 0x48;                        // REX.W prefix
    address[1] = 0xB8;                        // MOV RAX, imm64
    *((void**)(address + 2)) = destination;   // 8-byte immediate address
    address[10] = 0xFF;                       // JMP RAX
    address[11] = 0xE0;

    // NOP the rest
    for (DWORD i = 12; i < length; i++) {
        address[i] = 0x90;
    }

    DWORD temp;
    VirtualProtect(address, length, oldProtect, &temp);
}



bool Hook64(void* toHook, void* hk_func, int len) {
    if (len < 13) {
        return false;
    }

    DWORD curProtection;
    VirtualProtect(toHook, len, PAGE_EXECUTE_READWRITE, &curProtection);

    memset(toHook, 0x90, len);

    unsigned char patch[] = {
        0x50,                               // push rax (1 byte)
        0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, // movabs rax, [immediate 64-bit address] (2 bytes opcode + 8 bytes immediate = 10 bytes)
        0x00, 0x00, 0x00, 0x00,
        0xFF, 0xE0,                         // jmp rax (2 bytes)
        0x90, 0x90                          // NOP, NOP (2 bytes for padding to 15)
    }; // Total size of patch array is 1 + 10 + 2 + 2 = 15 bytes

    *(DWORD64*)&patch[3] = (DWORD64)hk_func;
    memcpy((void*)toHook, patch, sizeof(patch));
    DWORD temp;
    VirtualProtect(toHook, len, curProtection, &temp);
    return true;
}

DWORD64 jmpBackAddress = skyrimModuleBase + 0x74452C + 15;
DWORD64 g_raxAddress = 0;
 
void __declspec(naked) hk_coords() {

    __asm {
        movss xmm5, [rax + 0x54] // Original instruction 1
        movss xmm4, [rax + 0x58] // Original instruction 2
        movss xmm3, [rax + 0x5C] // Original instruction 3
        mov g_raxAddress, rax // Store the RAX value in the global variable

        jmp[jmpBackAddress]
    }
}