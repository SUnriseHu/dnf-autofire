#!/bin/bash
# 编译 DNF 连发用的 dinput8.dll (x86 / 32 位)
#
# 三条硬性约束（都是踩过的坑）：
#   1. 工具链必须位于【纯英文路径】——MinGW 的 ld 无法解析中文路径，会找不到库。
#   2. 传给 gcc 的参数一律用相对路径 —— 工程目录含中文，绝对路径同样会乱码。
#   3. 用 -nostdlib 切断 CRT —— 否则 DLL 会依赖 api-ms-win-crt-*.dll，
#      目标机若缺 UCRT 组件会加载失败，进而让游戏启动不了。
#      代码里已全部改用 Win32 API，故可安全切断 CRT。
set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

# ---------- 定位 i686 交叉编译器（纯英文路径） ----------
GCC=""
for tc in \
    "/d/DNFALL/toolchain32" \
    "D:/DNFALL/toolchain32" \
    "/c/Users/JAME/.workbuddy/binaries/mingw32"
do
    if [ -x "$tc/bin/i686-w64-mingw32-gcc.exe" ]; then
        GCC="$tc/bin/i686-w64-mingw32-gcc.exe"
        break
    fi
done
if [ -z "$GCC" ]; then
    echo "[!] 未找到 i686 (32 位) gcc。工具链须放在纯英文路径，如 D:/DNFALL/toolchain32"
    exit 1
fi
echo "[*] 编译器: $GCC"
"$GCC" --version | head -1

mkdir -p build

# ---------- 编译 ----------
# -nostdlib       不链接 CRT（避免 UCRT 依赖）
# -ffreestanding  无宿主环境，编译器不假设有 libc
# --entry         直接把 DllMain 作为 DLL 入口点（省掉 CRT startup）
"$GCC" -m32 -O2 -s -shared \
    -nostdlib -ffreestanding \
    -o build/dinput8.dll \
    src/dinput8_proxy.c src/dinput8.def \
    -Wl,--kill-at \
    -Wl,--entry,_DllMain@12 \
    -lkernel32 -luser32

echo "[*] 编译完成 -> build/dinput8.dll"
ls -la build/dinput8.dll
