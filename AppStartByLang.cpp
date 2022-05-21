#include <windows.h>
#include <tchar.h>
#include <string>
#include <strsafe.h>
#include <stdio.h>

HANDLE g_hProcess = NULL;
HANDLE g_hThread = NULL;

void version(void)
{
    puts(
        "SetLangAppStart Version 0.4\n"
        "Copyright (C) 2022 Katayama Hirofumi MZ\n"
        "License: MIT"
    );
}

void usage(void)
{
    puts(
        "Usage: SetLangAppStart LangID your_app.exe [parameters]\n"
        "       SetLangAppStart --help\n"
        "       SetLangAppStart --version\n"
        "       SetLangAppStart --langs"
    );
}

void langs(void)
{
    HKL ahKLs[64];
    ZeroMemory(ahKLs, sizeof(ahKLs));

    UINT iKL, chKLs = GetKeyboardLayoutList(_countof(ahKLs), ahKLs);
    CHAR szBuff[MAX_PATH];

    for (iKL = 0; iKL < chKLs; ++iKL)
    {
        LANGID LangID = LOWORD(ahKLs[iKL]);
        if (GetLocaleInfoA(LangID, LOCALE_SABBREVLANGNAME, szBuff, _countof(szBuff)))
            printf("0x%04X: %s\n", LangID, szBuff);
    }
}

void atexit_proc(void)
{
    CloseHandle(g_hThread);
    TerminateProcess(g_hProcess, -1024);
}

inline BOOL IsWindowsVistaOrLater(VOID)
{
    OSVERSIONINFOW osver = { sizeof(osver) };
    return (GetVersionExW(&osver) && osver.dwMajorVersion >= 6);
}

typedef LANGID (WINAPI *FN_SetLang)(LANGID);

FN_SetLang GetLangProc(void)
{
    FN_SetLang fn;
    if (IsWindowsVistaOrLater())
    {
        fn = (FN_SetLang)GetProcAddress(GetModuleHandleA("kernel32"), "SetThreadUILanguage");
        if (fn)
        {
            OutputDebugStringA("SetThreadUILanguage\n");
            return fn;
        }
    }

    fn = (FN_SetLang)GetProcAddress(GetModuleHandleA("kernel32"), "SetThreadLocale");
    if (fn)
        OutputDebugStringA("SetThreadLocale\n");
    return fn;
}

BOOL SetLangToThread(HANDLE hThread, LANGID wLangID)
{
    BOOL ret = FALSE;
    FN_SetLang fn = GetLangProc();
    if (fn)
    {
        ret = (BOOL)QueueUserAPC((PAPCFUNC)fn, hThread, wLangID);
    }
    return ret;
}

INT doRunByLang(LPCWSTR cmdline, LANGID wLangID, INT nCmdShow)
{
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { NULL };

    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = nCmdShow;

    LPTSTR pszCmdLine = wcsdup(cmdline);
    INT ret = CreateProcessW(NULL, pszCmdLine, NULL, NULL, FALSE, CREATE_SUSPENDED,
                             NULL, NULL, &si, &pi);
    if (!ret)
    {
        wprintf(L"FAILED: %s (GetLastError: %ld)\n", cmdline, GetLastError());
    }
    free(pszCmdLine);

    if (!ret)
        return -1;

    g_hThread = pi.hThread;
    g_hProcess = pi.hProcess;
    atexit(atexit_proc);

    if (!SetLangToThread(pi.hThread, wLangID))
        OutputDebugStringA("FAILED: SetLangToThread\n");

    ResumeThread(pi.hThread);

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD dwExitCode = -1;
    GetExitCodeProcess(pi.hProcess, &dwExitCode);

    CHAR szBuf[32];
    StringCchPrintfA(szBuf, _countof(szBuf), "dwExitCode: %d\n", (INT)dwExitCode);
    return (INT)dwExitCode;
}

int wmain(int argc, wchar_t *wargv[])
{
    if (argc <= 1 || lstrcmpiW(wargv[1], L"--help") == 0)
    {
        usage();
        return 0;
    }

    if (lstrcmpiW(wargv[1], L"--version") == 0)
    {
        version();
        return 0;
    }

    if (lstrcmpiW(wargv[1], L"--langs") == 0)
    {
        langs();
        return 0;
    }

    if (!(L'0' <= wargv[1][0] && wargv[1][0] <= L'9'))
    {
        usage();
        return -1;
    }

    LANGID wLangID = wcstoul(wargv[1], NULL, 0);

    std::wstring cmdline;
    for (INT iarg = 2; iarg < argc; ++iarg)
    {
        LPWSTR arg = wargv[iarg];
        if (iarg > 2)
            cmdline += L" ";

        if (wcscspn(arg, L" \t\r\n") == wcslen(arg))
        {
            cmdline += arg;
        }
        else
        {
            cmdline += L"\"";
            cmdline += arg;
            cmdline += L"\"";
        }
    }

    STARTUPINFOW si = { sizeof(si) };
    GetStartupInfoW(&si);

    if (!(si.dwFlags & STARTF_USESHOWWINDOW))
        si.wShowWindow = SW_SHOWNORMAL;

    return doRunByLang(cmdline.c_str(), wLangID, si.wShowWindow);
}
