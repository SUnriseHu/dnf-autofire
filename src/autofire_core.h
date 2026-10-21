/*
 * autofire_core.h —— 连发核心逻辑（被代理 DLL 与测试程序共享）
 *
 * 规则（就地修改一帧键盘状态缓冲）：
 *   Alt + [字母/数字]   → 切换该键连发开关（边沿触发，并吃掉组合键）
 *   已开启连发的键且物理按住 → 调制成高频通断
 *   其他键              → 原样不动
 *
 * 只依赖 GetTickCount()；日志通过 AF_LOG 宏注入（默认空），
 * 因此本文件保持纯逻辑、可被单元测试直接驱动。
 */
#ifndef AUTOFIRE_CORE_H
#define AUTOFIRE_CORE_H

/* 连发半周期(ms)：一个通断周期 = 2 × 该值。15 -> 约 33 次/秒 */
#ifndef AF_CFG_HALF_PERIOD_MS
#define AF_CFG_HALF_PERIOD_MS 15
#endif

#ifndef AF_LOG
#define AF_LOG(...) ((void)0)
#endif

#ifndef DIK_LALT
#define DIK_LALT 0x38   /* 左 Alt */
#endif
#ifndef DIK_RALT
#define DIK_RALT 0xB8   /* 右 Alt */
#endif

/* 支持键位：26 字母 + 10 数字（DIK 扫描码），与 KeyName[] 一一对应 */
static const unsigned char g_monCodes[] = {
    0x1E,0x30,0x2E,0x20,0x12,0x21,0x22,0x23,0x17,0x24,0x25,0x26,0x32,0x31, /* A..N */
    0x18,0x19,0x10,0x13,0x1F,0x14,0x16,0x2F,0x11,0x2D,0x15,0x2C,            /* O..Z */
    0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B                       /* 1..9,0 */
};
static const int g_monCount = (int)(sizeof(g_monCodes) / sizeof(g_monCodes[0]));

static int g_toggle[256];    /* 每键：连发是否开启 */
static int g_prevDown[256];  /* 每键：上一帧物理按下状态（边沿检测） */

static const char* KeyName(unsigned char code)
{
    static const char* names[] = {
        "A","B","C","D","E","F","G","H","I","J","K","L","M",
        "N","O","P","Q","R","S","T","U","V","W","X","Y","Z",
        "1","2","3","4","5","6","7","8","9","0"
    };
    int i;
    for (i = 0; i < g_monCount; ++i) if (g_monCodes[i] == code) return names[i];
    return "?";
}

static void ProcessKeyBuffer(unsigned char* keys, DWORD cb)
{
    int altDown, i;
    DWORD now;

    if (!keys || cb < 256) return;   /* 非键盘设备（鼠标/手柄）直接跳过 */

    altDown = ((keys[DIK_LALT] & 0x80) != 0) || ((keys[DIK_RALT] & 0x80) != 0);
    now = GetTickCount();

    for (i = 0; i < g_monCount; ++i) {
        unsigned char code = g_monCodes[i];
        int down = (keys[code] & 0x80) != 0;

        /* Alt + 键：只在按下的一瞬间切换开关 */
        if (altDown && down && !g_prevDown[code]) {
            g_toggle[code] = !g_toggle[code];
            keys[code] = 0;              /* 吃掉组合键，游戏不响应 */
            g_prevDown[code] = 1;
            AF_LOG("[toggle] Alt+%s -> %s", KeyName(code), g_toggle[code] ? "ON" : "OFF");
            continue;
        }

        /* 已开启连发且物理按住：高频通断 */
        if (g_toggle[code] && down) {
            keys[code] = (((now / AF_CFG_HALF_PERIOD_MS) & 1) != 0) ? 0x80 : 0x00;
        }

        g_prevDown[code] = down;
    }
}

#endif /* AUTOFIRE_CORE_H */
