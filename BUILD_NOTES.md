# Engine ② KiriKiri — Kirikiroid2 @ macOS arm64

## 状态：🚧 地基完成（TJS2 脚本引擎 arm64 原生跑通），运行时未完成

## 上游
- https://github.com/zeas2/Kirikiroid2 （master，2947 stars，2024-06 仍有更新）
- 它是 **Android + cocos2d-x 应用**，不是桌面程序：窗口/输入/渲染全在
  `src/core/environ/cocos2d/MainScene.cpp`（2448 行）里，由 cocos2d-x 提供。

## 已完成（本目录）
### 1. TJS2 脚本引擎 → `libtjs2.a` + `tjsc` CLI（arm64 原生）
TJS2 是 KiriKiri 和 KAG 的宿主语言，是整条链的地基。它自身几乎零依赖
（只有标准库 + Oniguruma 正则）。

```bash
brew install oniguruma
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
./build/tjsc tests/hello.tjs
```

**实测输出（exit 0）**：
```
string: KiriKiri len=8
array: 1,2,3
dict: KiriKiri2 / TJS2
function: 42
class: I am Derived (derived)
regexp: dee@kikyou.info -> dee/kikyou.info
number: 255 + 1 = 256
platform: macOS
caught: thrown on purpose
done
```
覆盖：字符串/数组/字典/函数/类继承/正则(Oniguruma)/类型自动转换/异常。

### 2. darwin 平台层
| 文件 | 职责 |
|---|---|
| `platform/darwin_platform.cpp` | `TVPGetRoughTickCount32`（CLOCK_MONOTONIC）、`TJS::TVPConsoleLog`、`TVPGetMessageByLocale`（走引擎消息表兜底） |
| `platform/darwin_system.cpp` | `System` 对象：`inform`/`print`/`println`/`platform`（原生类宏，照 tjsMath.cpp 的写法） |
| `tools/tjsc.cpp` | CLI 宿主：UTF-8 读脚本、注册 System、跑脚本、UTF-16→UTF-8 输出 |

上游这三个符号属于「应用层」职责，Android 在 `environ/cocos2d/MainScene.cpp`、
Windows 在 `environ/Application.cpp` —— 我们给 macOS 写了自己的那份。

## 构建坑
1. 代码是 C++11 时代：`std::binary_function` 在 C++17 已移除 → 用 **C++14**
2. `tjs_char` 在 clang 下是 `char16_t`（UTF-16）——正确，KiriKiri 原生就是 UTF-16
3. `LocaleConfigManager`（消息本地化）绑死 cocos2d `FileUtils` → 桌面版不用它，
   消息走 `TJSGetMessageMapMessage` 兜底，查不到就返回 key 本身

## 重要兼容性发现
这个 TJS2 是 **2003 年的老版本**（Kirikiroid2 基于 TVP 的 TJS2，不是 krkrz）：
- ❌ 没有 `in` 运算符，没有 `for(k in obj)` —— 后来的 krkrz 才加
- ❌ 核心不含 `String`/`format` 等 TVP 扩展类（那是应用层注册的）
- ✅ 核心语言（类/闭包/正则/异常/字典）完整可用

结论：老游戏脚本没问题；**2013 年后的新 KAG 脚本可能依赖 krkrz 的语法**，
到时候要么走 krkrz 分支，要么加语法补丁。

## 剩余工作（按依赖顺序）
1. **XP3 归档层** → `krkrpack` 工具（列目录/解包）。这是拿到游戏资源的第一步，
   而且**不需要窗口**，可独立验证。相关源码在 `src/core/base/XP3Archive.cpp`。
2. **TVP 扩展类**（`Storage`/`Scripts`/`Window`/`System` 的完整版）——KAG 脚本直接调它们
3. **窗口 + 渲染**：用 SDL2 替换 cocos2d 那 2448 行，渲染用 `visual/ogl/`
4. **音频**：`sound/` 的 WaveIntf 需要 darwin 实现（SDL2 audio 或 CoreAudio）
5. ⚠️ **图像/动画**：上游重度依赖 libgdiplus/cairo/pixman/opencv/ffmpeg/bpg/jxrlib
   这些 vendored 库（tarball 里没有，需单独拉）——这是最大的工作量来源

## 诚实评估
Kirikiroid2 的「桌面化」不是修 bug 级别，是**重写应用层 + 重建 20 个第三方依赖**，
量级远大于引擎①④。已把地基（TJS2）做成可独立验证的一层，后续按上述顺序推进。
真跑 KiriKiri 游戏（如 Fate）在短期内的现实路径仍是 **Sikarugir(Wine)** 兜底。
