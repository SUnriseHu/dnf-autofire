/*
 * test_core.c —— 连发核心逻辑验证（32 位）
 *
 * 直接驱动 autofire_core.h 的 ProcessKeyBuffer，用模拟的键盘状态缓冲
 * 验证全部行为，不注入任何系统按键，对当前桌面零干扰。
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "autofire_core.h"

static int g_fail = 0;

static void check(const char* what, int got, int want)
{
    int ok = (got == want);
    if (!ok) g_fail++;
    printf("  [%s] %-40s got=%d want=%d\n", ok ? "PASS" : "FAIL", what, got, want);
}

/* 模拟“按一下组合键再全部松开”，保证边沿检测能触发 */
static void tap(unsigned char k1, unsigned char k2)
{
    unsigned char keys[256];
    memset(keys, 0, sizeof(keys));
    if (k1) keys[k1] = 0x80;
    if (k2) keys[k2] = 0x80;
    ProcessKeyBuffer(keys, 256);
    memset(keys, 0, sizeof(keys));
    ProcessKeyBuffer(keys, 256);   /* 全松一帧 */
}

int main(void)
{
    unsigned char keys[256];
    int i, on, off;

    printf("=== 连发核心逻辑验证 (half period = %d ms) ===\n\n", AF_CFG_HALF_PERIOD_MS);

    check("初始 X 连发开关应为关", g_toggle[0x2D], 0);

    /* --- 1. Alt+X 应打开 X 连发，且组合键被吃掉 --- */
    memset(keys, 0, sizeof(keys));
    keys[DIK_LALT] = 0x80;
    keys[0x2D] = 0x80;
    ProcessKeyBuffer(keys, 256);
    check("Alt+X 后 X 开关", g_toggle[0x2D], 1);
    check("Alt+X 组合键被吃掉(keys[X])", keys[0x2D], 0x00);

    /* --- 2. 保持按住 X，观察是否在高频通断 --- */
    on = off = 0;
    for (i = 0; i < 400; i++) {
        memset(keys, 0, sizeof(keys));
        keys[0x2D] = 0x80;
        ProcessKeyBuffer(keys, 256);
        if (keys[0x2D] & 0x80) on++; else off++;
        Sleep(1);
    }
    printf("  按住 X 400 帧: 通=%d 断=%d\n", on, off);
    check("按住 X 时既有通也有断", (on > 0 && off > 0) ? 1 : 0, 1);

    /* --- 3. 松开 X 应立刻停止 --- */
    memset(keys, 0, sizeof(keys));
    ProcessKeyBuffer(keys, 256);
    check("松开 X 后 keys[X]", keys[0x2D], 0x00);

    /* --- 4. 再按一次 Alt+X 应关闭 --- */
    tap(DIK_LALT, 0x2D);
    check("再按 Alt+X 后 X 开关", g_toggle[0x2D], 0);

    /* --- 5. 关闭后按住 X，应原样透传 --- */
    memset(keys, 0, sizeof(keys));
    keys[0x2D] = 0x80;
    ProcessKeyBuffer(keys, 256);
    check("关闭后按住 X 原样透传", keys[0x2D], 0x80);

    /* --- 6. 数字键 Alt+1 --- */
    tap(DIK_LALT, 0x02);
    check("Alt+1 后数字1开关", g_toggle[0x02], 1);

    /* --- 7. 未开启连发的其他键不受影响 --- */
    memset(keys, 0, sizeof(keys));
    keys[0x1E] = 0x80;
    ProcessKeyBuffer(keys, 256);
    check("未开启的 A 键原样透传", keys[0x1E], 0x80);

    /* --- 8. 右 Alt 也应生效 --- */
    g_toggle[0x2D] = 0; g_prevDown[0x2D] = 0;
    tap(DIK_RALT, 0x2D);
    check("右 Alt+X 也能切开关", g_toggle[0x2D], 1);

    /* --- 9. 非键盘设备(缓冲<256)应被忽略 --- */
    keys[0x2D] = 0x80;
    ProcessKeyBuffer(keys, 20);
    check("非键盘缓冲被忽略", keys[0x2D], 0x80);

    printf("\n=== 结果: %s (失败 %d 项) ===\n", g_fail ? "FAIL" : "全部通过", g_fail);
    return g_fail ? 1 : 0;
}
