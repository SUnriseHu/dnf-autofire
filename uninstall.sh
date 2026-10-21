#!/bin/bash
# 卸载：移除游戏目录里的 dinput8.dll（若存在备份则还原）
# 用法: ./uninstall.sh "D:/DNFALL/ACT12" "D:/DNFALL/ACT5"
set -e

if [ $# -eq 0 ]; then
    echo "用法: ./uninstall.sh <游戏目录> [<游戏目录2> ...]"
    exit 1
fi

for d in "$@"; do
    if [ ! -d "$d" ]; then
        echo "[!] 目录不存在，跳过: $d"
        continue
    fi

    if [ -f "$d/dinput8.dll.bak" ]; then
        mv -f "$d/dinput8.dll.bak" "$d/dinput8.dll"
        echo "[+] 已还原原始 dinput8.dll: $d"
    elif [ -f "$d/dinput8.dll" ]; then
        rm -f "$d/dinput8.dll"
        echo "[-] 已删除 $d/dinput8.dll"
    else
        echo "[~] $d 下没有 dinput8.dll，无需处理"
    fi

    rm -f "$d/dinput8.log"
done
echo "卸载完成。"
