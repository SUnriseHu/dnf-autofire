# DNF 连发 —— dinput8.dll 代理方案

一个 **随 DNF.exe 启动自动加载** 的进程内连发组件：**只要一个 `dinput8.dll`**，
无外部程序、无窗口、无提示，**运行期不产生任何额外文件**。

---

## 1. 原理

`DNF.exe` 启动时会从**自身目录**加载 `dinput8.dll`（已确认 `dinput8.dll` 不在系统 KnownDLLs，
且 `SafeDllSearchMode` 默认开启 → **exe 所在目录优先**）。

把本代理 DLL 放到游戏目录并命名为 `dinput8.dll`，游戏就会加载它。代理做两件事：

1. **导出转发**：`DirectInput8Create` 等 6 个导出全部原样转发给系统
   `C:\Windows\SysWOW64\dinput8.dll`，游戏功能不受影响。
2. **挂钩调制**：挂钩 `IDirectInput8::CreateDevice` 与
   `IDirectInputDevice8::GetDeviceState`，在游戏读取键盘状态的那一瞬间，
   对已开启连发的键位做**高频通断**（半周期 15ms → 约 33 次/秒）。

因为是在游戏进程内直接修改「游戏读到的按键状态」，**不产生任何系统级按键事件**，
也不影响其他程序，其他按键完全不受影响。

```
DNF.exe 启动
   └─ 从游戏目录加载 dinput8.dll（本代理）
        ├─ 转发层：DirectInput8Create → 系统 dinput8.dll
        ├─ Hook  ：IDirectInputDevice8::GetDeviceState
        └─ 连发引擎：Alt+键 切换开关 / 按键通断调制
```

## 2. 通用性

纯 API 挂钩，**不依赖任何游戏版本、内存地址或偏移**，所以
ACT5 / ACT12 / 其他同样用 `dinput8` 的 DNF 客户端，**同一份 DLL 通用**，游戏更新也不受影响。

## 3. 功能

| 操作 | 效果 |
|---|---|
| `Alt + [字母/数字]` | 切换该键的连发**开关**（按一下开，再按一下关） |
| 连发已开启的键，按住 | **连发** |
| 连发已开启的键，松开 | **立即停止** |
| 其他键 | 完全不受影响 |

支持键位：**26 个字母 A–Z + 10 个数字 0–9**，每键独立、互不影响。

例：`Alt+X` 开启 X 键连发 → 按住 X 连发、松开停止；再按一次 `Alt+X` 关闭。

## 4. 零文件

安装后游戏目录**只多出一个 `dinput8.dll`**。

- 运行期**不写日志、不写配置**（源码中 `ENABLE_LOG` 默认为 `0`）。
- 仅当需要排障时，把 `src/dinput8_proxy.c` 里的 `ENABLE_LOG` 改成 `1`
  再重新编译，才会额外产生 `dinput8.log`。

## 5. 构建

```bash
./build.sh          # 产出 build/dinput8.dll
```

> 编译器要求：32 位 MinGW-w64，且**必须放在纯英文路径**（如 `D:/DNFALL/toolchain32`），
> 因为 MinGW 的 `ld` 无法处理中文路径。

## 6. 部署 / 卸载

```bash
./install.sh "D:/DNFALL/ACT12" "D:/DNFALL/ACT5"     # 安装
./uninstall.sh "D:/DNFALL/ACT12" "D:/DNFALL/ACT5"   # 卸载
```

`install.sh` 只会在「游戏目录原本就有**非本代理的** dinput8.dll」时才备份为
`dinput8.dll.bak`；重复安装不会产生多余文件。之后**直接启动游戏即可**。

## 7. 自定义（可选）

| 位置 | 说明 |
|---|---|
| `src/autofire_core.h` 的 `AF_CFG_HALF_PERIOD_MS` | 连发半周期(ms)，默认 `15`（周期 30ms ≈ 33 次/秒）。改小更快、改大更慢。 |
| `src/dinput8_proxy.c` 的 `ENABLE_LOG` | `0` 静默（默认）；`1` 写 `dinput8.log` 排障用。 |

改完重新 `./build.sh` 并 `./install.sh`。

## 8. 目录结构

```
src/autofire_core.h   连发核心逻辑（DLL 与测试共享）
src/dinput8_proxy.c   代理 DLL 源码
src/dinput8.def       导出定义（6 个导出）
src/test_core.c       逻辑单测（12 项，直接驱动核心逻辑，零干扰）
src/test_host.c       加载链路验证（真实走一遍 DirectInput 调用）
build/                编译产物：dinput8.dll
build.sh              编译
install.sh / uninstall.sh  部署 / 卸载
tools/pe_info.py      PE 结构分析工具（开发用）
```
