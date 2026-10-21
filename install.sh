#!/bin/bash
# 安装：把 dinput8.dll 放进游戏目录（仅在首次覆盖原文件时备份）
# 用法: ./install.sh "D:/DNFALL/ACT12" "D:/DNFALL/ACT5"
set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
DLL="$ROOT/build/dinput8.dll"

if [ $# -eq 0 ]; then
    echo "用法: ./install.sh <游戏目录> [<游戏目录2> ...]"
    echo "例:   ./install.sh 'D:/DNFALL/ACT12' 'D:/DNFALL/ACT5'"
    exit 1
fi
if [ ! -f "$DLL" ]; then
    echo "[!] 找不到 $DLL ，请先运行 ./build.sh"
    exit 1
fi

for d in "$@"; do
    if [ ! -d "$d" ]; then
        echo "[!] 目录不存在，跳过: $d"
        continue
    fi
    if [ ! -f "$d/DNF.exe" ]; then
        echo "[~] 提示: $d 下未发现 DNF.exe，仍将写入 dinput8.dll"
    fi

    # 备份规则：只备份“不是我们产物”的原文件；重复安装不会产生多余 .bak
    if [ -f "$d/dinput8.dll" ] && [ ! -f "$d/dinput8.dll.bak" ]; then
        if cmp -s "$DLL" "$d/dinput8.dll"; then
            :   # 已是我们自己的 DLL，无需备份
        else
            cp -f "$d/dinput8.dll" "$d/dinput8.dll.bak"
            echo "[*] 已备份原文件 -> $d/dinput8.dll.bak"
        fi
    fi

    cp -f "$DLL" "$d/dinput8.dll"
    echo "[+] 安装成功: $d/dinput8.dll"

    # 清理历史版本可能留下的辅助文件，保持目录只多一个 dinput8.dll
    rm -f "$d/dinput8.ini" "$d/dinput8.log"
done

echo
echo "完成。直接启动游戏，无需其他程序；运行期不会产生任何额外文件。"
echo "用法: Alt+[字母/数字] 切换该键连发（按一下开，再按一下关）；开启后按住该键即连发，松开停止。"
