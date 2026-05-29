#include "Dev/SmokeLog.h"

#include <Windows.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>

namespace
{
    bool_t HasSmokeFlag()
    {
        const wchar_t* pCommandLine = GetCommandLineW();
        if (!pCommandLine)
            return false;

        return wcsstr(pCommandLine, L"--banpick-smoke") != nullptr
            || wcsstr(pCommandLine, L"--smoke-log") != nullptr;
    }

    bool_t BuildLogPath(char* pOut, size_t outBytes)
    {
        if (!pOut || outBytes == 0)
            return false;

        char exePath[MAX_PATH]{};
        if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) == 0)
            return false;

        char* pSlash = strrchr(exePath, '\\');
        if (!pSlash)
            return false;
        *pSlash = '\0';

        char logDir[MAX_PATH]{};
        sprintf_s(logDir, "%s\\SmokeLogs", exePath);
        CreateDirectoryA(logDir, nullptr);

        sprintf_s(
            pOut,
            outBytes,
            "%s\\WintersGame_%lu.log",
            logDir,
            static_cast<unsigned long>(GetCurrentProcessId()));
        return true;
    }

    bool_t StartsWith(const char* pText, const char* pPrefix)
    {
        if (!pText || !pPrefix)
            return false;

        const size_t prefixLen = strlen(pPrefix);
        return std::strncmp(pText, pPrefix, prefixLen) == 0;
    }
}

namespace Winters::DevSmoke
{
    bool_t IsEnabled()
    {
        static i32_t s_enabled = -1;
        if (s_enabled < 0)
            s_enabled = HasSmokeFlag() ? 1 : 0;

        return s_enabled == 1;
    }

    void Log(const char* pFormat, ...)
    {
        if (!pFormat)
            return;

        char line[2048]{};
        va_list args;
        va_start(args, pFormat);
        vsprintf_s(line, pFormat, args);
        va_end(args);

        OutputDebugStringA(line);

        const bool_t bForceFileLog =
            StartsWith(line, "[YawTrace]") ||
            StartsWith(line, "[ClientNetwork]") ||
            StartsWith(line, "[GameSessionClient]") ||
            StartsWith(line, "[BanPick]") ||
            StartsWith(line, "[Scene_CustomMode]") ||
            StartsWith(line, "[Scene_InGame]") ||
            StartsWith(line, "[Scene_MatchLoading]");
        if (!bForceFileLog && !IsEnabled())
            return;

        char logPath[MAX_PATH]{};
        if (!BuildLogPath(logPath, sizeof(logPath)))
            return;

        FILE* pFile = nullptr;
        if (fopen_s(&pFile, logPath, "ab") != 0 || !pFile)
            return;

        SYSTEMTIME now{};
        GetLocalTime(&now);
        fprintf(
            pFile,
            "[%02u:%02u:%02u.%03u pid=%lu] ",
            static_cast<unsigned>(now.wHour),
            static_cast<unsigned>(now.wMinute),
            static_cast<unsigned>(now.wSecond),
            static_cast<unsigned>(now.wMilliseconds),
            static_cast<unsigned long>(GetCurrentProcessId()));
        fputs(line, pFile);

        const size_t len = strlen(line);
        if (len == 0 || line[len - 1] != '\n')
            fputc('\n', pFile);

        fclose(pFile);
    }
}
