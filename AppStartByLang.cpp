#include <windows.h>
#include <tchar.h>
#include <string>
#include <strsafe.h>
#include <stdio.h>

#ifdef UNICODE
    typedef std::wstring tstr_t;
#else
    typedef std::string tstr_t;
#endif

HANDLE g_hProcess = NULL;
HANDLE g_hThread = NULL;

void version(void)
{
    puts("SetLangAppStart Version 0.1 by katahiromz");
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
    HKL ahKLs[32];
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
    puts("atexit");
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
            puts("SetThreadUILanguage");
            return fn;
        }
    }

    fn = (FN_SetLang)GetProcAddress(GetModuleHandleA("kernel32"), "SetThreadLocale");
    if (fn)
        puts("SetThreadLocale");
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

INT doRunByLang(LPCTSTR cmdline, LANGID wLangID, INT nCmdShow)
{
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi = { NULL };

    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = nCmdShow;

    LPTSTR pszCmdLine = _tcsdup(cmdline);
    INT ret = CreateProcess(NULL, pszCmdLine, NULL, NULL, FALSE, CREATE_SUSPENDED,
                            NULL, NULL, &si, &pi);
    if (!ret)
    {
        _tprintf(TEXT("FAILED: %s (GetLastError: %ld)\n"), cmdline, GetLastError());
    }
    free(pszCmdLine);

    if (!ret)
        return -1;

    g_hThread = pi.hThread;
    g_hProcess = pi.hProcess;
    atexit(atexit_proc);

    if (!SetLangToThread(pi.hThread, wLangID))
        puts("FAILED: SetLangToThread");

    ResumeThread(pi.hThread);

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD dwExitCode = -1;
    GetExitCodeProcess(pi.hProcess, &dwExitCode);
    printf("dwExitCode: %d\n", (INT)dwExitCode);
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

    LANGID wLangID = _tcstoul(wargv[1], NULL, 0);

    tstr_t cmdline;
    for (INT iarg = 2; iarg < argc; ++iarg)
    {
        LPTSTR arg = wargv[iarg];
        if (iarg > 2)
            cmdline += TEXT(" ");

        if (_tcscspn(arg, TEXT(" \t\r\n")) == _tcslen(arg))
        {
            cmdline += arg;
        }
        else
        {
            cmdline += TEXT("\"");
            cmdline += arg;
            cmdline += TEXT("\"");
        }
    }

    STARTUPINFO si = { sizeof(si) };
    GetStartupInfo(&si);

    if (!(si.dwFlags & STARTF_USESHOWWINDOW))
        si.wShowWindow = SW_SHOWNORMAL;

    return doRunByLang(cmdline.c_str(), wLangID, si.wShowWindow);
}
