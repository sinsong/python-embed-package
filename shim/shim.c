// https://bitbucket.org/vinay.sajip/simple_launcher/src/master/launcher.c
/*
 * Copyright (C) 2011-2022 Vinay Sajip. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <stdio.h>
#include <stdlib.h>

#include <windows.h>
#include <Shlwapi.h>

#pragma comment (lib, "Shlwapi.lib")
#pragma comment (lib, "User32.lib")

#define RUN_MODULE L""

#define SWITCH_WORKING_DIR
#define MSGSIZE 1024
#define STARTF_UNDOC_MONITOR 0x400

#if !defined(_CONSOLE)

typedef int (__stdcall *MSGBOXWAPIA)(IN HWND hWnd,
        IN LPCSTR lpText, IN LPCSTR lpCaption,
        IN UINT uType, IN WORD wLanguageId, IN DWORD dwMilliseconds);

typedef int (__stdcall *MSGBOXWAPIW)(IN HWND hWnd,
        IN LPWSTR lpText, IN LPWSTR lpCaption,
        IN UINT uType, IN WORD wLanguageId, IN DWORD dwMilliseconds);

#define MB_TIMEDOUT 32000

int MessageBoxTimeoutA(HWND hWnd, LPCSTR lpText,
    LPCSTR lpCaption, UINT uType, WORD wLanguageId, DWORD dwMilliseconds)
{
    static MSGBOXWAPIA MsgBoxTOA = NULL;
    HMODULE hUser = LoadLibraryA("user32.dll");

    if (!MsgBoxTOA) {
        if (hUser)
            MsgBoxTOA = (MSGBOXWAPIA)GetProcAddress(hUser,
                                      "MessageBoxTimeoutA");
        else {
            /*
             * stuff happened, add code to handle it here
             * (possibly just call MessageBox())
             */
        }
    }

    if (MsgBoxTOA)
        return MsgBoxTOA(hWnd, lpText, lpCaption, uType, wLanguageId,
                         dwMilliseconds);
    if (hUser)
        FreeLibrary(hUser);
    return 0;
}

int MessageBoxTimeoutW(HWND hWnd, LPWSTR lpText,
    LPWSTR lpCaption, UINT uType, WORD wLanguageId, DWORD dwMilliseconds)
{
    static MSGBOXWAPIW MsgBoxTOW = NULL;
    HMODULE hUser = LoadLibraryA("user32.dll");

    if (!MsgBoxTOW) {
        if (hUser)
            MsgBoxTOW = (MSGBOXWAPIW)GetProcAddress(hUser,
                                      "MessageBoxTimeoutW");
        else {
            /*
             * stuff happened, add code to handle it here
             * (possibly just call MessageBox())
             */
        }
    }

    if (MsgBoxTOW)
        return MsgBoxTOW(hWnd, lpText, lpCaption, uType, wLanguageId,
                         dwMilliseconds);
    if (hUser)
        FreeLibrary(hUser);
    return 0;
}

#endif

static void
wassert(BOOL condition, wchar_t * format, ... )
{
    if (!condition) {
        va_list va;
        wchar_t message[MSGSIZE];
        int len;

        va_start(va, format);
        len = _vsnwprintf_s(message, MSGSIZE, MSGSIZE - 1, format, va);
#if defined(_CONSOLE)
        fwprintf(stderr, L"Fatal error in launcher: %s\n", message);
#else
        MessageBoxTimeoutW(NULL, message, L"Fatal Error in Launcher",
                           MB_OK | MB_SETFOREGROUND | MB_ICONERROR,
                           0, 3000);
#endif
        ExitProcess(1);
    }
}

static void
assert(BOOL condition, char * format, ... )
{
    if (!condition) {
        va_list va;
        char message[MSGSIZE];
        int len;

        va_start(va, format);
        len = vsnprintf_s(message, MSGSIZE, MSGSIZE - 1, format, va);
#if defined(_CONSOLE)
        fprintf(stderr, "Fatal error in launcher: %s\n", message);
#else
        MessageBoxTimeoutA(NULL, message, "Fatal Error in Launcher",
                           MB_OK | MB_SETFOREGROUND | MB_ICONERROR,
                           0, 3000);
#endif
        ExitProcess(1);
    }
}

#if defined(DUPLICATE_HANDLES)
static BOOL
safe_duplicate_handle(HANDLE in, HANDLE * pout)
{
    BOOL ok;
    HANDLE process = GetCurrentProcess();
#if defined(_CONSOLE)
    DWORD rc;
#endif

    *pout = NULL;
    /*
     * See https://github.com/pypa/pip/issues/10444 - for the GUI launcher,
     * errors are returned by DuplicateHandle. There may be no good reason
     * why a GUI process would want to use these handles, but for now we
     * attempt duplication but ignore errors in the GUI case.
     */
    ok = DuplicateHandle(process, in, process, pout, 0, TRUE,
                         DUPLICATE_SAME_ACCESS);
#if defined(_CONSOLE)
    if (!ok) {
        rc = GetLastError();
        if (rc == ERROR_INVALID_HANDLE)
            ok = TRUE;
    }
    return ok;
#else
    return TRUE;
#endif
}

#endif

static PROCESS_INFORMATION child_process_info;
static JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_info;
static HANDLE job;

static BOOL
control_key_handler(DWORD type)
{
/*
 * See https://github.com/pypa/pip/issues/10444
 */
#if !defined(NEW_LOGIC)
    if ((type == CTRL_C_EVENT) || (type == CTRL_BREAK_EVENT)) {
        return TRUE;
    }
    WaitForSingleObject(child_process_info.hProcess, INFINITE);
#else
    switch (type) {
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        /*
         * Allow the child to outlive the launcher, to carry out any
         * cleanup for a graceful exit. It will either exit or get
         * terminated by the session server.
         */
        if (job != NULL) {
            job_info.BasicLimitInformation.LimitFlags &=
              ~JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(
              job, JobObjectExtendedLimitInformation,
              &job_info, sizeof(job_info));
        }
    }
#endif
    return TRUE;
}

#ifndef _CONSOLE
static void
clear_app_starting_state() {
    MSG msg;
    HWND hwnd;

    PostMessageW(0, 0, 0, 0);
    GetMessageW(&msg, 0, 0, 0);
    /* Proxy the child's input idle event. */
    WaitForInputIdle(child_process_info.hProcess, INFINITE);
    /*
     * Signal the process input idle event by creating a window and pumping
     * sent messages. The window class isn't important, so just use the
     * system "STATIC" class.
     */
    hwnd = CreateWindowExW(0, L"STATIC", L"PyLauncher", 0, 0, 0, 0, 0,
                           HWND_MESSAGE, NULL, NULL, NULL);
    /* Process all sent messages and signal input idle. */
    PeekMessageW(&msg, hwnd, 0, 0, 0);
    DestroyWindow(hwnd);
}
#endif

static BOOL
make_handle_inheritable(HANDLE handle)
{
    DWORD file_type = GetFileType(handle);
    // Ignore an invalid handle, non-file object type, unsupported file type,
    // or a console file prior to Windows 8.
    if (file_type == FILE_TYPE_UNKNOWN ||
        (file_type == FILE_TYPE_CHAR && ((ULONG_PTR)handle & 3))) {
        return TRUE;
    }

    return SetHandleInformation(handle, HANDLE_FLAG_INHERIT,
        HANDLE_FLAG_INHERIT);
}

static void
__cdecl
silent_invalid_parameter_handler(
    wchar_t const* expression,
    wchar_t const* function,
    wchar_t const* file,
    unsigned int line,
    uintptr_t pReserved
)
{
}

static void
cleanup_fds(WORD cbReserved2, LPBYTE lpReserved2)
{
    int handle_count = 0;
    UNALIGNED HANDLE* first_handle = NULL;
    UNALIGNED HANDLE* current_handle = NULL;
    _invalid_parameter_handler old_handler = NULL;

    // The structure is: <handle_count>, <handle_count bytes with flags>, <handle_count HANDLEs>

    if (cbReserved2 < sizeof(int) || NULL == lpReserved2)
    {
        return;
    }

    handle_count = *(UNALIGNED int*)lpReserved2;

    // Verify the buffer is large enough
    if (cbReserved2 < sizeof(int) + handle_count + sizeof(HANDLE) * handle_count)
    {
        return;
    }

    first_handle = (UNALIGNED HANDLE *)(lpReserved2 + sizeof(int) + handle_count);

    old_handler = _set_invalid_parameter_handler(&silent_invalid_parameter_handler);
    {
        // Close all fds inherited from the parent, except for the standard I/O fds.
        // We'll deal with those later.
        for (current_handle = first_handle + 3; current_handle < first_handle + handle_count; ++current_handle)
        {
            // Ignore invalid handles, as that means this fd was not inherited.
            // -2 is a special value (https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/get-osfhandle?view=msvc-170)
            // that we check for just in case.
            if (NULL == *current_handle || INVALID_HANDLE_VALUE == *current_handle || (HANDLE)-2 == *current_handle)
            {
                continue;
            }

            _close((int)(current_handle - first_handle));
        }
    }
    _set_invalid_parameter_handler(old_handler);
}

static HANDLE
get_stream_handle(FILE * stream)
{
    _invalid_parameter_handler old_handler = NULL;
    int fd = -1;
    HANDLE handle = INVALID_HANDLE_VALUE;

    old_handler = _set_invalid_parameter_handler(&silent_invalid_parameter_handler);
    {
        fd = _fileno(stream);
        if (fd >= 0)
        {
            handle = (HANDLE)_get_osfhandle(fd);
        }
        else
        {
            handle = INVALID_HANDLE_VALUE;
        }
    }
    _set_invalid_parameter_handler(old_handler);

    return handle;
}

static void
cleanup_standard_io(void)
{
    HANDLE stdin_handle = INVALID_HANDLE_VALUE;
    HANDLE stdout_handle = INVALID_HANDLE_VALUE;
    HANDLE stderr_handle = INVALID_HANDLE_VALUE;
    HANDLE hStdIn = INVALID_HANDLE_VALUE;
    HANDLE hStdOut = INVALID_HANDLE_VALUE;
    HANDLE hStdErr = INVALID_HANDLE_VALUE;

    // We need to close both the C streams stdin and stdout,
    // and the Windows standard I/O handles. However, these may be equal,
    // so care must be taken not to close a handle twice. Moreover,
    // handles for several C streams may be equal as well.
    // Fun all around.

    // Get the handles associated with the standard I/O streams.
    stdin_handle = get_stream_handle(stdin);
    stdout_handle = get_stream_handle(stdout);
    stderr_handle = get_stream_handle(stderr);

    // If any two underlying handles are equal, drop everything and return.
    // There's bound to be trouble if we continue with closing the streams.
    if (((INVALID_HANDLE_VALUE != stdin_handle) && (stdin_handle == stdout_handle || stdin_handle == stderr_handle))
        || ((INVALID_HANDLE_VALUE != stdout_handle) && (stdout_handle == stdin_handle || stdout_handle == stderr_handle))
        || ((INVALID_HANDLE_VALUE != stderr_handle) && (stderr_handle == stdin_handle || stderr_handle == stdout_handle)))
    {
        return;
    }

    // Get the Windows I/O handles before we do anything.
    hStdIn = GetStdHandle(STD_INPUT_HANDLE);
    hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    hStdErr = GetStdHandle(STD_ERROR_HANDLE);

    // At this point, we have confirmed that the I/O streams all have different
    // handles, and we have the Windows standard I/O handles as well.
    // Proceed with closing the streams.

    fclose(stdin);
    fclose(stdout);

    // Now we need to close the Windows standard I/O handles, as they might
    // differ from the handles for the C streams.

    // First, make sure we don't close handles that we have already closed
    // by closing the streams.
    if (stdin_handle == hStdIn || stdout_handle == hStdIn)
    {
        SetStdHandle(STD_INPUT_HANDLE, NULL);
        hStdIn = NULL;
    }
    if (stdin_handle == hStdOut || stdout_handle == hStdOut)
    {
        SetStdHandle(STD_OUTPUT_HANDLE, NULL);
        hStdOut = NULL;
    }
    if (stdin_handle == hStdErr || stdout_handle == hStdErr)
    {
        SetStdHandle(STD_ERROR_HANDLE, NULL);
        hStdErr = NULL;
    }

    // Ensure we don't accidentally close the standard error handle.
    if (stderr_handle == hStdIn)
    {
        hStdIn = NULL;
    }
    if (stderr_handle == hStdOut)
    {
        hStdOut = NULL;
    }
    if (stderr_handle == hStdErr)
    {
        hStdErr = NULL;
    }

    // Close 'em.
    if (NULL != hStdIn && INVALID_HANDLE_VALUE != hStdIn)
    {
        CloseHandle(hStdIn);
        SetStdHandle(STD_INPUT_HANDLE, NULL);
    }
    if (NULL != hStdOut && INVALID_HANDLE_VALUE != hStdOut)
    {
        CloseHandle(hStdOut);
        SetStdHandle(STD_OUTPUT_HANDLE, NULL);
    }
    if (NULL != hStdErr && INVALID_HANDLE_VALUE != hStdErr)
    {
        CloseHandle(hStdErr);
        SetStdHandle(STD_ERROR_HANDLE, NULL);
    }
}

#if defined(SWITCH_WORKING_DIR)
static void
switch_working_directory() {
    WCHAR tempDir[MAX_PATH + 1];
    DWORD len = GetTempPathW(MAX_PATH + 1, tempDir);
    if (len > 0 && len <= MAX_PATH) {
        SetCurrentDirectoryW(tempDir);
    }
}
#endif

static void
post_spawn_cleanup(WORD cbReserved2, LPBYTE lpReserved2)
{
    cleanup_fds(cbReserved2, lpReserved2);

    cleanup_standard_io();
#if defined(SWITCH_WORKING_DIR)
    switch_working_directory();
#endif
}

static void
run_child(wchar_t *cmdline)
{
    DWORD rc;
    BOOL ok;
    STARTUPINFOW si;

    job = CreateJobObject(NULL, NULL);
    assert(job != NULL, "Job creation failed");
    ok = QueryInformationJobObject(job, JobObjectExtendedLimitInformation, &job_info, sizeof(job_info), &rc);

    assert(ok && (rc == sizeof(job_info)), "Job information querying failed");
    job_info.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
                                                 JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK;
    ok = SetInformationJobObject(job, JobObjectExtendedLimitInformation, &job_info, sizeof(job_info));

    assert(ok, "Job information setting failed");
    memset(&si, 0, sizeof(si));
    GetStartupInfoW(&si);

    if ((si.dwFlags & (STARTF_USEHOTKEY | STARTF_UNDOC_MONITOR)) == 0) {
        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);

#if defined(DUPLICATE_HANDLES)
        ok = safe_duplicate_handle(hIn, &si.hStdInput);
        assert(ok, "stdin duplication failed");
        CloseHandle(hIn);

        ok = safe_duplicate_handle(hOut, &si.hStdOutput);
        assert(ok, "stdout duplication failed");
        CloseHandle(hOut);
        /* We might need stderr late, so don't close it but mark as non-inheritable */
        SetHandleInformation(hErr, HANDLE_FLAG_INHERIT, 0);

        ok = safe_duplicate_handle(hErr, &si.hStdError);
        assert(ok, "stderr duplication failed");
#else
        /*
         * See https://github.com/pypa/pip/issues/10444#issuecomment-1055392299
         */
        ok = make_handle_inheritable(hIn);
        assert(ok, "making stdin inheritable failed");
        ok = make_handle_inheritable(hOut);
        assert(ok, "making stdout inheritable failed");
        ok = make_handle_inheritable(hErr);
        assert(ok, "making stderr inheritable failed");
        si.hStdInput = hIn;
        si.hStdOutput = hOut;
        si.hStdError = hErr;
#endif
        si.dwFlags |= STARTF_USESTDHANDLES;
    }
    ok = CreateProcessW(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &child_process_info);
    if (!ok) {
        // Failed to create process. See if we can find out why.
        DWORD err = GetLastError();
        wchar_t emessage[MSGSIZE];
        FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err,
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), emessage, MSGSIZE, NULL);
        wassert(ok, L"Unable to create process using '%ls': %ls", cmdline, emessage);
    }
    // Assign the process to the job straight away. See https://github.com/pypa/distlib/issues/175
    AssignProcessToJobObject(job, child_process_info.hProcess);
    post_spawn_cleanup(si.cbReserved2, si.lpReserved2);
    /*
     * Control handler setting is now done after process creation because the handler needs access
     * to the child_process_info structure, which is populated by the CreateProcessW call above.
     */
    ok = SetConsoleCtrlHandler((PHANDLER_ROUTINE) control_key_handler, TRUE);
    assert(ok, "control handler setting failed");
#if !defined(_CONSOLE)
    clear_app_starting_state(&child_process_info);
#endif
    CloseHandle(child_process_info.hThread);
    WaitForSingleObjectEx(child_process_info.hProcess, INFINITE, FALSE);
    ok = GetExitCodeProcess(child_process_info.hProcess, &rc);
    assert(ok, "Failed to get exit code of process");
    ExitProcess(rc);
}

static int
process(int argc, char *argv[])
{
    wchar_t self_path[MAX_PATH];
    size_t len = GetModuleFileNameW(NULL, self_path, MAX_PATH);

    HRESULT hr = PathRemoveFileSpecW(self_path);
    wchar_t interpreter[MAX_PATH];
#if defined(_CONSOLE)
    wchar_t interpreter_name[] = L"python.exe";
#else
    wchar_t interpreter_name[] = L"pythonw.exe";
#endif
    PathCombineW(interpreter, self_path, interpreter_name);

    wchar_t module[] = RUN_MODULE;
    wchar_t *cmdp;
    len = wcslen(interpreter) + 12 + wcslen(module);
    cmdp = (wchar_t *) calloc(len, sizeof(wchar_t));
    // TODO: vsnprintf can measure buffer size
    // https://learn.microsoft.com/zh-cn/cpp/c-runtime-library/reference/vsnprintf-s-vsnprintf-s-vsnprintf-s-l-vsnwprintf-s-vsnwprintf-s-l?view=msvc-170
    _snwprintf_s(cmdp, len, len, L"\"%ls\" -s -m \"%ls\"", interpreter, module);
    run_child(cmdp);
    free(cmdp);
    return 0;
}

#if defined (_CONSOLE)
int main(int argc, char *argv[])
{
    return process(argc, argv);
}
#else
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    return process(__argc, __argv);
}
#endif
