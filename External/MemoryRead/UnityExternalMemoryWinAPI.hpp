#pragma once

#if defined(_WIN32) || defined(_WIN64)
#define UNITY_EXTERNAL_WINDOWS 1
#else
#define UNITY_EXTERNAL_WINDOWS 0
#endif

#if UNITY_EXTERNAL_WINDOWS

#include <windows.h>
#include "../Core/UnityExternalMemory.hpp"

namespace UnityExternal {

// Default cross-process memory accessor based on WinAPI.
// Users can implement their own IMemoryAccessor (e.g. driver / custom API)
// and pass it to SetGlobalMemoryAccessor instead.
struct WinAPIMemoryAccessor : IMemoryAccessor {
    HANDLE process;

    explicit WinAPIMemoryAccessor(HANDLE hProcess = GetCurrentProcess())
        : process(hProcess) {}

    bool Read(std::uintptr_t address, void* buffer, std::size_t size) const override {
        if (!process || !buffer || size == 0) {
            return false;
        }

        SIZE_T bytesRead = 0;
        if (!ReadProcessMemory(process,
                               reinterpret_cast<LPCVOID>(address),
                               buffer,
                               static_cast<SIZE_T>(size),
                               &bytesRead)) {
            return false;
        }

        return bytesRead == size;
    }

    bool Write(std::uintptr_t address, const void* buffer, std::size_t size) const override {
        if (!process || !buffer || size == 0) {
            return false;
        }

        SIZE_T bytesWritten = 0;
        if (!WriteProcessMemory(process,
                                reinterpret_cast<LPVOID>(address),
                                buffer,
                                static_cast<SIZE_T>(size),
                                &bytesWritten)) {
            return false;
        }

        return bytesWritten == size;
    }
};

} // namespace UnityExternal

#endif // UNITY_EXTERNAL_WINDOWS
