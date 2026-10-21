/*
 * dinput8.dll 代理 —— DNF 连发 (x86)
 * ---------------------------------------------------------------
 * 原理：
 *   把本 DLL 放到 DNF.exe 所在目录，命名为 dinput8.dll。
 *   Windows 加载器按“exe 目录优先”的顺序加载，于是游戏启动时
 *   自动加载本代理 DLL（无需外部程序、无窗口、无提示）。
 *
 *   本 DLL 把 dinput8 的全部导出函数原样转发给系统
 *   C:\Windows\SysWOW64\dinput8.dll，同时挂钩
 *   IDirectInput8::CreateDevice 与 IDirectInputDevice8::GetDeviceState，
 *   在游戏读取键盘状态的瞬间，对指定键位做“高频通断”调制。
 *
 * 功能（核心逻辑见 autofire_core.h）：
 *   Alt + [字母/数字]  = 切换该键的连发开关（按一下开，再按一下关）
 *   连发开启的键按住   = 连发；松开 = 立刻停止
 *   其他键              = 完全不受影响
 *
 * 零文件：
 *   运行期不创建/写入任何文件（无日志、无配置）。唯一产物就是这个 dll。
 *   ENABLE_LOG=0 为默认；仅在开发排障时才需改成 1（会额外产生 dinput8.log）。
 *
 * 通用性：
 *   纯 API 挂钩，不依赖任何游戏版本/地址，ACT5 ~ ACT12 通用。
 *
 * 依赖：
 *   仅 KERNEL32.dll / USER32.dll（全部 Win32 API，用 -nostdlib 切断 CRT），
 *   任意 Windows 都能加载，不会因缺 UCRT 组件导致游戏启动失败。
 * ---------------------------------------------------------------
 */

#define DIRECTINPUT_VERSION 0x0800
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <dinput.h>
#include <stdarg.h>

/* ================= 配置 ================= */

/* 0 = 完全不写任何文件（默认）；1 = 在 DLL 同目录写 dinput8.log（仅排障用） */
#define ENABLE_LOG 0

static HMODULE g_hSelf = NULL;

#if ENABLE_LOG
static void LogLine(const char* fmt, ...);
#define AF_LOG(...) LogLine(__VA_ARGS__)
#else
#define AF_LOG(...) ((void)0)
#endif

/* ================= 连发核心（共享逻辑） ================= */

#include "autofire_core.h"

/* ================= 日志（仅在 ENABLE_LOG=1 时编译，纯 Win32 不依赖 CRT） ================= */

#if ENABLE_LOG
static void LogLine(const char* fmt, ...)
{
    char path[MAX_PATH];
    char msg[512];
    char* p;
    va_list ap;
    HANDLE h;
    DWORD written = 0;

    if (!g_hSelf) return;
    if (!GetModuleFileNameA(g_hSelf, path, MAX_PATH)) return;

    p = path + lstrlenA(path);
    while (p > path && p[-1] != '\\') p--;
    if (p <= path) return;
    lstrcpynA(p, "dinput8.log", MAX_PATH - (int)(p - path));

    va_start(ap, fmt);
    wvsprintfA(msg, fmt, ap);
    va_end(ap);
    lstrcatA(msg, "\r\n");

    h = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        WriteFile(h, msg, (DWORD)lstrlenA(msg), &written, NULL);
        CloseHandle(h);
    }
}
#endif

/* ================= 系统 dinput8 转发 ================= */

typedef HRESULT (WINAPI *fn_DirectInput8Create)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
typedef HRESULT (STDMETHODCALLTYPE *fn_CreateDevice)(void*, REFGUID, void**, LPUNKNOWN);
typedef HRESULT (STDMETHODCALLTYPE *fn_GetDeviceState)(void*, DWORD, LPVOID);
typedef HRESULT (STDMETHODCALLTYPE *fn_GetDeviceData)(void*, DWORD, void*, LPDWORD, DWORD);

#define VT_CREATEDEVICE    3
#define VT_GETDEVICESTATE  9
#define VT_GETDEVICEDATA   10

static HMODULE g_sys = NULL;
static fn_DirectInput8Create p_DirectInput8Create = NULL;
static fn_CreateDevice       p_CreateDevice       = NULL;
static fn_GetDeviceState     p_GetDeviceState     = NULL;
static fn_GetDeviceData      p_GetDeviceData      = NULL;
static int g_diHooked  = 0;
static int g_devHooked = 0;

static HMODULE SysDinput8(void)
{
    char path[MAX_PATH];
    UINT n;
    if (g_sys) return g_sys;
    n = GetSystemDirectoryA(path, MAX_PATH);   /* x86 进程下返回 SysWOW64 */
    if (n == 0 || n + 12 >= MAX_PATH) return NULL;
    lstrcatA(path, "\\dinput8.dll");
    g_sys = LoadLibraryA(path);
    if (!g_sys) AF_LOG("[proxy] load system dinput8 FAILED err=%lu", (unsigned long)GetLastError());
    return g_sys;
}

/* ================= 挂钩实现 ================= */

static HRESULT STDMETHODCALLTYPE Hook_GetDeviceState(void* dev, DWORD cb, LPVOID data)
{
    HRESULT hr;
    if (!p_GetDeviceState) return DIERR_NOTINITIALIZED;
    hr = p_GetDeviceState(dev, cb, data);
    if (SUCCEEDED(hr) && data) ProcessKeyBuffer((unsigned char*)data, cb);
    return hr;
}

static HRESULT STDMETHODCALLTYPE Hook_GetDeviceData(void* dev, DWORD cbObjectData,
                                                    void* rgdod, LPDWORD pdwInOut, DWORD dwFlags)
{
    if (!p_GetDeviceData) return DIERR_NOTINITIALIZED;
    return p_GetDeviceData(dev, cbObjectData, rgdod, pdwInOut, dwFlags);
}

static void HookDevice(void* dev)
{
    void** vt;
    DWORD old;

    if (!dev || g_devHooked) return;
    vt = *(void***)dev;
    if (!vt) return;
    if (!VirtualProtect(vt, 16 * sizeof(void*), PAGE_EXECUTE_READWRITE, &old)) return;

    p_GetDeviceState = (fn_GetDeviceState)vt[VT_GETDEVICESTATE];
    p_GetDeviceData  = (fn_GetDeviceData)vt[VT_GETDEVICEDATA];
    vt[VT_GETDEVICESTATE] = (void*)Hook_GetDeviceState;
    vt[VT_GETDEVICEDATA]  = (void*)Hook_GetDeviceData;

    VirtualProtect(vt, 16 * sizeof(void*), old, &old);
    g_devHooked = 1;
    AF_LOG("[hook] IDirectInputDevice8 vtable hooked (orig GetDeviceState=%p)", (void*)p_GetDeviceState);
}

static HRESULT STDMETHODCALLTYPE Hook_CreateDevice(void* self, REFGUID rguid,
                                                   void** ppDevice, LPUNKNOWN unk)
{
    HRESULT hr;
    if (!p_CreateDevice) return DIERR_NOTINITIALIZED;
    hr = p_CreateDevice(self, rguid, ppDevice, unk);
    if (SUCCEEDED(hr) && ppDevice && *ppDevice) HookDevice(*ppDevice);
    return hr;
}

static void HookDirectInput(void* pDI)
{
    void** vt;
    DWORD old;

    if (!pDI || g_diHooked) return;
    vt = *(void***)pDI;
    if (!vt) return;
    if (!VirtualProtect(vt, 8 * sizeof(void*), PAGE_EXECUTE_READWRITE, &old)) return;

    p_CreateDevice = (fn_CreateDevice)vt[VT_CREATEDEVICE];
    vt[VT_CREATEDEVICE] = (void*)Hook_CreateDevice;

    VirtualProtect(vt, 8 * sizeof(void*), old, &old);
    g_diHooked = 1;
    AF_LOG("[hook] IDirectInput8::CreateDevice hooked");
}

/* ================= dinput8 导出（原样转发） ================= */

HRESULT WINAPI DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf,
                                  LPVOID* ppvOut, LPUNKNOWN punkOuter)
{
    HRESULT hr;
    if (!p_DirectInput8Create) {
        HMODULE h = SysDinput8();
        if (h) p_DirectInput8Create = (fn_DirectInput8Create)GetProcAddress(h, "DirectInput8Create");
    }
    if (!p_DirectInput8Create) return E_FAIL;

    hr = p_DirectInput8Create(hinst, dwVersion, riidltf, ppvOut, punkOuter);
    if (SUCCEEDED(hr) && ppvOut && *ppvOut) {
        AF_LOG("[init] DirectInput8Create ok -> install hooks");
        HookDirectInput(*ppvOut);
    }
    return hr;
}

STDAPI DllCanUnloadNow(void)
{
    typedef HRESULT (WINAPI *f_t)(void);
    static f_t f = NULL;
    if (!f) { HMODULE h = SysDinput8(); if (h) f = (f_t)GetProcAddress(h, "DllCanUnloadNow"); }
    return f ? f() : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    typedef HRESULT (WINAPI *f_t)(REFCLSID, REFIID, LPVOID*);
    static f_t f = NULL;
    if (!f) { HMODULE h = SysDinput8(); if (h) f = (f_t)GetProcAddress(h, "DllGetClassObject"); }
    return f ? f(rclsid, riid, ppv) : E_FAIL;
}

STDAPI DllRegisterServer(void)
{
    typedef HRESULT (WINAPI *f_t)(void);
    static f_t f = NULL;
    if (!f) { HMODULE h = SysDinput8(); if (h) f = (f_t)GetProcAddress(h, "DllRegisterServer"); }
    return f ? f() : E_FAIL;
}

STDAPI DllUnregisterServer(void)
{
    typedef HRESULT (WINAPI *f_t)(void);
    static f_t f = NULL;
    if (!f) { HMODULE h = SysDinput8(); if (h) f = (f_t)GetProcAddress(h, "DllUnregisterServer"); }
    return f ? f() : E_FAIL;
}

LPCDIDATAFORMAT WINAPI GetdfDIJoystick(void)
{
    typedef LPCDIDATAFORMAT (WINAPI *f_t)(void);
    static f_t f = NULL;
    if (!f) { HMODULE h = SysDinput8(); if (h) f = (f_t)GetProcAddress(h, "GetdfDIJoystick"); }
    return f ? f() : NULL;
}

/* ================= 入口（-nostdlib 下由 --entry 指定） ================= */

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        g_hSelf = hInst;
        DisableThreadLibraryCalls(hInst);
        AF_LOG("[proxy] dinput8 proxy loaded");
    }
    return TRUE;
}
