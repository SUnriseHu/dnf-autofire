/*
 * test_host.c —— 32 位宿主程序，用于验证 dinput8 代理 DLL 的挂钩链。
 *
 * 完全模拟 DNF 客户端加载/使用 dinput8 的方式：
 *   LoadLibrary("dinput8.dll") -> DirectInput8Create -> CreateDevice
 *   -> SetDataFormat -> GetDeviceState
 *
 * 不注入任何按键，只验证：
 *   1) 代理 DLL 能被正常加载（导出可解析）；
 *   2) 转发层工作（DirectInput8Create / CreateDevice / GetDeviceState 返回正常）；
 *   3) 挂钩已安装（DLL 同目录 dinput8.log 会出现 [hook] 行）。
 *
 * 用法：把本 exe 与 dinput8.dll 放在同一目录，直接运行。
 */

#define DIRECTINPUT_VERSION 0x0800
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <dinput.h>
#include <stdio.h>

/* 手工定义 GUID，避免依赖 dxguid 库 */
static const GUID IID_DI8A =
    { 0xBF798031, 0x483A, 0x4DA2, { 0xAA, 0x99, 0x5D, 0x64, 0xED, 0x36, 0x97, 0x00 } };
static const GUID GUID_KB =
    { 0x6F1D2B61, 0xD5A0, 0x11CF, { 0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 } };

typedef HRESULT (WINAPI *pfn_DICreate)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);

int main(void)
{
    HMODULE hMod;
    pfn_DICreate fn;
    IDirectInput8A* pDI = NULL;
    IDirectInputDevice8A* pDev = NULL;
    HRESULT hr;
    static char keys[256];

    printf("[test] 1) 加载 dinput8.dll ...\n");
    hMod = LoadLibraryA("dinput8.dll");
    printf("[test]    module = %p (0 = 失败)\n", (void*)hMod);
    if (!hMod) {
        printf("[test] FAIL: 代理 DLL 无法加载 (err=%lu)\n", (unsigned long)GetLastError());
        return 1;
    }

    fn = (pfn_DICreate)GetProcAddress(hMod, "DirectInput8Create");
    printf("[test]    DirectInput8Create = %p (0 = 导出缺失)\n", (void*)fn);
    if (!fn) return 2;

    printf("[test] 2) DirectInput8Create ...\n");
    hr = fn(GetModuleHandleA(NULL), DIRECTINPUT_VERSION, &IID_DI8A, (LPVOID*)&pDI, NULL);
    printf("[test]    hr = 0x%08lX, pDI = %p\n", (unsigned long)hr, (void*)pDI);
    if (FAILED(hr) || !pDI) return 3;

    printf("[test] 3) CreateDevice(GUID_SysKeyboard) ...\n");
    hr = pDI->lpVtbl->CreateDevice(pDI, &GUID_KB, &pDev, NULL);
    printf("[test]    hr = 0x%08lX, pDev = %p  <-- 此处应触发 [hook] device vtable\n",
           (unsigned long)hr, (void*)pDev);

    if (pDev) {
        printf("[test] 4) GetDeviceState ...\n");
        hr = pDev->lpVtbl->GetDeviceState(pDev, 256, keys);
        printf("[test]    GetDeviceState hr = 0x%08lX (未 Acquire 时返回 NOTACQUIRED 属正常)\n",
               (unsigned long)hr);
        printf("[test]    keys[0x2D](X) = 0x%02X, keys[0x38](LAlt) = 0x%02X\n",
               (unsigned char)keys[0x2D], (unsigned char)keys[0x38]);
    }

    printf("[test] DONE —— 请查看同目录 dinput8.log 确认 [hook] 记录\n");
    return 0;
}
