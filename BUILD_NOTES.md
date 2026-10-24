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

### 3. XP3 归档层 → `krkrpack`（arm64 原生）✅ 已完成并验证
游戏的全部资源都在 `data.xp3` 里，所以这是拿到游戏内容的第一步，而且不需要窗口。

```bash
./build/krkrpack list    game/data.xp3
./build/krkrpack extract game/data.xp3 out/
```

**验证方式（无游戏数据的自证）**：用 `tests/make_xp3.py` 造一个符合格式的
XP3，再用 krkrpack 列出/解包，与源文件逐字节比对：

```
3 file(s) in file:///tmp/test.xp3
        11  a.txt
        33  b.txt
      4000  dir/c.bin
--- 3 file(s), 4044 byte(s) uncompressed
a.txt ✓ 一致   b.txt ✓ 一致   dir/c.bin ✓ 一致（4KB 随机二进制）
exit 0
```

为此补齐的「平台层」（上游在各平台各写一份，tarball 里没有）：
- `platform/darwin/Platform.cpp` — `environ/Platform.h` 那 28 个函数：内存统计
  (sysctl + host_statistics64)、消息框（无窗口时走 stderr）、目录/文件辅助、IME、
  `TVP_stat`/`TVP_utime`、语言
- `platform/darwin_host.cpp` — 存储层：`tTVPFileMedia`（"file://" 媒体，10 个方法，
  POSIX 实现）、`tTVPFileStream`、`TVPOpenArchive`（嗅探 XP3 签名）、
  `TVPGetVersion`/`TVPGetCommandLine`/`TVPAddAtExitHandler`、`tTVPApplication::PrintConsole`
- `platform/compat/Protect.h` — win32 反调试子系统在桌面构建里是空操作
- `platform/compat/libarchive/` — Android NDK 头布局的兼容软链（brew 的 libarchive 是 keg-only）

**依赖处理**：`UtilStreams.cpp` 里 7z/unrar 两条路径需要 vendored 库，但上游自己
已经把 7z 分支改成走 libarchive 了，所以桌面构建用 `KRKR_DESKTOP_UNPACK` 把那两段
死代码编译掉，zip/7z/rar 统一走 libarchive，XP3 走引擎原生读取器。

### 4. 脚本层 ↔ 存储层 ↔ 归档层 打通 → `krkrscript` ✅
引擎的脚本通过 TVP 扩展类访问文件，`Storages` 就是那个对象。把它们接起来之后，
**TJS2 脚本可以直接查询 XP3 归档里的文件**（日志里能看到 XP3 读取器真的打开归档）：

```
$ ./build/krkrscript tests/storage.tjs
plain file exists:    1
plain file missing:   0
in archive, present:  1     ← file:///tmp/test.xp3>a.txt
in archive, nested:   1     ← file:///tmp/test.xp3>dir/c.bin
in archive, absent:   0
done
exit 0
```

实现要点：
- `Storages` 类本体在上游 `base/StorageIntf.cpp`（已编译），工厂函数在 `StorageImpl.cpp`
  → 我们在 `platform/darwin_host.cpp` 补了那一个函数体（`new tTJSNC_Storages()`）
- 主机启动时把当前目录加入 auto path（`TVPAddAutoPath`），等价于引擎的启动流程
- 已知限制：引擎会规范化掉相对名里的 `./`，所以**归档内成员要用绝对路径**
  （`file:///abs/path/game.xp3>inner.txt`）；窗口层落地后由游戏目录解析补上

### 5. 软件像素管线（tvpgl）→ `krkrblit` ✅
引擎所有的图层、精灵、转场最终都落进 `visual/tvpgl.cpp`——12600 行的像素管线，
以函数指针表的形式存在，`TVPInitTVPGL()` 把每个指针指到通用 C 实现。

**卡点与解法**：它 `#include "tvpgl_route.h"`，而这个头是 Android 构建**生成**的
（按 CPU 选 SIMD 实现），树里没有。查清 `TVPInitTVPGL()` 里已经直接把指针赋成
`_c` 通用实现，所以这个头**可以是空的**——补一个带说明的空兼容头即可。
（期间顺手给 `tTVPApplication* Application` 补了定义，日志层要用。）

验证工具 `krkrblit`（按混合器的真实契约断言，定点位移 ±1 容差）：

```
$ ./build/krkrblit
  ok    opaque source replaces the destination         0xff2040bf
  ok    transparent source leaves the destination      0xff808080
  ok    half alpha lands between the colours           r=0x40
  ok    additive blend brightens                       0xff303030
  32 -> 24 bit conversion                              ff 00 80
all passed: 0 check(s) failed
```

两个语义细节（踩过才知道）：
- `TVPAlphaBlend` 是「目标无 alpha」的变体，**不保留目标 alpha**；带 alpha 的目标用 `TVPAlphaBlend_d`
- 定点运算 `((s-d)*a)>>8` 会使结果比精确值低 1，断言必须容差 1

### 6. 图像解码 → 显示 → 逐像素验证 → `krkrview` ✅
把「从 XP3 里取一张图、用引擎自己的解码器解开、显示出来」这条链跑通了：

```bash
./build/krkrview /tmp/view.xp3 img.png out.bmp
# decoded 513x55 from file:///tmp/view.xp3>img.png
```

**验证**（用 BGI 体验版解出的一张带 alpha 的 PNG 装进 XP3，再用引擎解码器解回来，
按原始字节比对）：

```
raw bytes: 112860 for 513x55
  as BGRA: 4431/28215 differ
  as RGBA: 0/28215 differ   ← MATCH
```

### 重要发现：引擎像素格式是 ABGR8888，不是 ARGB8888
引擎的 `tjs_uint32` 像素是 **0xAABBGGRR**（alpha 在高字节，然后 B、G、R）。
内存里（小端）是 R,G,B,A —— 对应 SDL 的 **`SDL_PIXELFORMAT_ABGR8888`**。
我最初按名字猜成 ARGB8888，结果红蓝互换；靠原始字节比对才定论。
**这一条对后面整个渲染层都关键。**

### 位图层第一片：`src/core/visual/darwin/LayerBitmapImpl.cpp`
上游的位图把像素存在 `iTVPTexture2D` 里（类本身只有度量与扫描线），所以先实现
了委托给纹理的那几个方法（GetWidth/GetHeight/GetScanLine/GetScanLineForWrite/
GetPitchBytes）+ 解码器要用的 `TVPClearGraphicCache()` 空钩子。
解码器因此可以链接；完整的位图层（文字/字体/绘制）仍待补。

### 7. 字体与文字渲染 → `krkrfont` ✅（galgame 的核心）
上游的文字管线 `FreeType.cpp` / `FreeTypeFontRasterizer.cpp` / `FontSystem.cpp` /
`PrerenderedFont.cpp` / `CharacterData.cpp` **是平台无关的**，只差几个平台钩子：

| 钩子 | 实现 |
|---|---|
| `TVPGetDefaultFontName()` | Hiragino Sans GB → Arial Unicode → AppleSDGothicNeo（`KRKR_FONT`/`ONS_FONT` 可覆盖） |
| `TVPCreateFontStream()` | 走存储层 `TVPCreateStream` |
| `TVPGetAllFontList()` | 扫 `/System/Library/Fonts`、`/Library/Fonts`、`~/Library/Fonts` |
| `TVPFontSystem` | 一个 `FontSystem` 实例 |

`environ/ConfigManager/*` 绑 cocos2d，桌面版用 `platform/darwin/darwin_config.cpp`
重新实现同一个类（无偏好文件时一律返回调用方默认值）。

**实测（真实日文字形，抗锯齿）**：
```
$ ./build/krkrfont /tmp/font.bmp 40 "あ漢A9"
  U+3042 extent 40x40 blackbox 32x36 pitch 32 origin (4,2) aa=1 full=0
  U+6F22 extent 40x40 blackbox 37x38 pitch 40 origin (1,1) aa=1 full=0
  U+0041 extent 30x40 blackbox 30x31 pitch 32 origin (0,4) aa=1 full=0
$ ./build/krkrfont /tmp/font2.bmp 48 "ナルキッソス"
```
渲染出的「あ漢A9」与「ナルキッソス」都清晰可读（见提交说明里的验证记录）。

**又一个格式坑**：普通字形的 `tTVPCharacterData::Data` 是**8 位覆盖率**（一字节一像素，
`Pitch == BlackBoxX`），不是 4 字节；只有 `FullColored` 字形才是 4 字节 BGRA。
按 4 字节读会得到重影糊字。

### 8. 合成完整画面 → `krkrscene` ✅
三块引擎部件合体，**像素全部出自引擎自己**：

```bash
./build/krkrscene /tmp/scene.xp3 bg.png scene.bmp "この空の向こうに、何があるんだろう。" 120 0 40
# background: 1920x1080 from /tmp/scene.xp3>bg.png
# text: ... at (120,940), 40px
```

- 背景：`TVPLoadPNG` 从 XP3 里解码（存储层）
- 消息窗：`TVPAlphaBlend_d`（tvpgl）半透明混合
- 台词：FreeType 字体栈栅格化 → 覆盖率当 alpha → `TVPAlphaBlend_d` 叠到背景

产物见 `screenshots/scene-composed.png`（1920×1080，真实游戏背景 + 日文台词）。
这一张图说明引擎②的图像、字体、混合三层都是通的——缺的只剩「谁来调度这些层」，
也就是窗口层与 KAG 运行时。

### 9. 软件纹理 + 软件渲染管理器 + 位图分配 ✅（图层的地基）
引擎的位图类本身**不存像素**——像素在 `iTVPTexture2D` 里，由渲染管理器创建。
所以先把这两个部件写出来：

| 部件 | 文件 | 说明 |
|---|---|---|
| 软件纹理 | `visual/darwin/Texture2D_Soft.cpp` | `iTVPTexture2D` 的内存实现，引擎像素格式 |
| 软件渲染管理器 | `visual/darwin/RenderManager_Soft.cpp` | `iTVPRenderManager` 只有 **9 个纯虚**，全部实现；纹理创建、缩放（最近邻）、OperateRect 拷贝 |
| 软件重采样 | `visual/darwin/ResampleImage_Soft.cpp` | `TVPResampleImage`（上游 gl 版绑 cocos 分配器） |
| 位图分配 | `visual/darwin/LayerBitmapImpl.cpp` | ctor/dtor/Recreate/SetSize/GetBPP/Assign/Independ… 委托给纹理 |

**验证**（`krkrbitmap`）：
```
bitmap: 320x200, 32 bpp, pitch 1280
point (10,10) = 0x00ff00ff, (11,10) = 0xff282828
wrote /tmp/bmp.bmp     ← 渐变 + 色块，像素抽样与写入一致
```

### 10. 引擎自带的软件渲染器跑起来了 ✅（重大转折）
上一轮我发现「连 Fill 都要走渲染方法」，正准备自己写。结果发现：**上游
`visual/RenderManager.cpp`（4414 行）里本来就有一整套软件渲染器**——
`tTVPSoftwareRenderManager` + 全部渲染方法（FillARGB / FillColor / Copy /
AlphaBlend / 各色混合与遮罩 / 拉伸 / 透视）+ 自己的压缩纹理类
（`tTVPSoftwareTexture2D`，带 LZ4）。

它只被三样东西卡住，全是**编译期依赖**而非逻辑：

| 缺的东西 | 解法 |
|---|---|
| `renderer/CCTexture2D.h`（cocos2d） | `platform/compat/renderer/CCTexture2D.h`：只声明该文件用到的 4 个名字，适配器函数是空实现（软件后端永不调用） |
| `xxhash/xxhash.h` | `platform/compat/xxhash/xxhash.h`：自己实现的 XXH32（公有领域算法） |
| `libswscale` / `opencv2` / `lz4` | `brew install ffmpeg` / `opencv@4` / `lz4`。**注意必须用 OpenCV 4**：代码用的是 `cv::getAffineTransform` 和 3 参数 `getPerspectiveTransform`，OpenCV 5 已删除/改签名；Homebrew 同时装了 v5 和 v4，所以用**绝对路径**链接 v4 |

于是把我自己写的 `Texture2D_Soft.cpp` / `RenderManager_Soft.cpp` **删掉了**——
用引擎的。顺带把 `tTVPBitmap`（像素缓冲类，137 行纯可移植）从 win32 版搬进
`visual/darwin/LayerBitmapImpl.cpp`。

**验证**（`krkrbitmap`，全部走引擎 API）：
```
$ ./build/krkrbitmap /tmp/bmp4.bmp
01:57:48 (info) Use malloc for Bitmap
bitmap: 320x200, 32 bpp, pitch 1280
bg    (32, 64, 96)      ← Fill(0xff204060) 精确正确
block (255, 128, 0)     ← FillColor(0x00ff8000, opa=255) 精确正确
point (0, 255, 0)       ← SetPoint(0xff00ff00) 正确
```

**已知缺口**：`FillColor` 在 `opa < 255` 时走 `ConstColorAlphaBlend` 方法，目前不出图
（不崩），待查。`Fill` 与不透明 `FillColor` 正常。

颜色约定（两条要记住）：
- `Fill` / `FillColor` 的入参是 **0xAARRGGBB**，内部会 `TVP_REVRGB` 成 0xAABBGGRR
- `GetPoint` / `SetPoint` / 扫描线用的是**内部格式 0xAABBGGRR**

### 上一轮的发现（保留记录）：连「纯色填充」都要走渲染方法
`iTVPBaseBitmap::Fill` 的实现是：
```cpp
static iTVPRenderMethod* method = TVPGetRenderManager()->GetRenderMethod("FillARGB");
static int paramid = method->EnumParameterID("color");
```
也就是说引擎把所有绘制（Fill / FillColor / 拉伸 / 混合）都抽象成
**`iTVPRenderMethod`（着色器式的对象）**，由渲染管理器按名字提供。软件后端
必须实现这些方法（内部调 tvpgl 即可），否则 `method` 是空指针。
**这就是下一个部件**：`iTVPRenderMethod` 的软件实现（FillARGB / Copy / AlphaBlend…）。

### 11. 引擎纹理上真屏幕 → `krkrpresent` ✅
引擎画进 `iTVPTexture2D`，后端负责把其中一张送显：Windows 是 GDI BitBlt，
Android 是 GL 纹理上传。桌面后端这一跳是 **SDL 纹理 + renderer**。

```bash
$ ./build/krkrpresent /tmp/scene.xp3 bg.png 60 /tmp/present2.bmp
texture: 1920x1080 from file:///tmp/scene.xp3>bg.png
presented 60 frame(s) on 'cocoa'      ← 真的开了一个 Cocoa 窗口
```

**验证**：截图与源图逐像素比对 **0/28215 差异**（用 513×55 带 alpha 的图测）。

### 12. 绘制设备（DrawDevice）→ `DrawDevice_Darwin.cpp` ✅（部分）
`tTVPDrawDevice`（`visual/win32/DrawDevice.cpp`，**零 Win32 调用**）是引擎的合成器，
实现了 54/56 个方法。平台的活只剩 **4 个虚函数**：

| 虚函数 | darwin 实现 |
|---|---|
| `Show()` | 主图层的位图 → SDL 纹理 → `SDL_RenderPresent`（真的开窗口） |
| `StartBitmapCompletion` / `NotifyBitmapCompleted` / `EndBitmapCompletion` | CPU 合成器同步完成，无需等待，空实现 |

写了 `src/core/visual/darwin/DrawDevice_Darwin.cpp`，编译链接通过。

**但接线还需要图层对象**（`tTJSNI_BaseLayer`，在 `LayerIntf.cpp` = 图层系统本体）。
把它加进库后，又拉出整串运行时服务：`TVPPostEvent`（事件）、`TVPGetCursor`（光标）、
`TVPLoadGraphic`/`TVPSaveImage`（图像）、`TVPCreateRectObject`、`ColorToRGB`、
`TVPActionName`/`TVPEventDisabled`/`TVPSimpleImageProvider` 三个全局。

**这是引擎②最后一段的真实清单**：图层系统 + 这些运行时服务接线。本轮为保证构建
全绿，把 `LayerIntf.cpp` 与对应工具撤下（不留死代码/坏目标），九个工具全部回归通过
（`krkrpresent` 仍逐像素 0 差异）。

### 13. 图层系统的平台服务（部分完成，边界已量化）
把这轮为图层写的东西落了两个文件：

| 文件 | 内容 |
|---|---|
| `visual/darwin/FontRasterizers_Darwin.cpp` | 字体栅格化注册表（从 win32 版搬来，纯可移植）：`TVPSetFontRasterizer`/`TVPGetFontRasterizer`/`GetCurrentRasterizer`/预渲染字体映射 |
| `platform/darwin/darwin_visual_services.cpp` | 引擎会点名的一批平台服务：不支持的图像格式（JPEG/JXR/WebP/BPG/PVR，**明确抛「无解码器」而不是假装**）、`ColorToRGB`、`TVPGetCursor`、剪贴板（走 SDL）、`TVPGetAppPath`、事件钩子、`TVPGL_ASM_Init`、`ExtractFileDir`、`TVPListDir` 等 |

**把 `LayerIntf.cpp`（图层系统本体）+ `ScriptMgnIntf.cpp`（注册全部 TVP 类）加进库后，
暴露出的完整缺口清单**（这就是引擎②还剩的全部工作量，逐项都已知归属）：

```
事件/系统层   TVPInvokeEvents, TVPEventReceived, TVPGetSystemEventDisabledState,
              TVPSystemControl(+Alive), TVPRegisteredPlugins, TVPLoadInternalPlugins,
              TVPCauseAtInstallExtensionClass, TVPCreateAndAddWindow
插件/手柄     TVPLoadInternalPlugin, TVPGetJoyPadAsyncState
脚本层        TVPExecuteExpression, TVPShowScriptException, TVPProcessUnhandledException,
              TVPBeforeProcessUnhandledException
系统信息      TVPDataPath, TVPNativeProjectDir, TVPDetectCPU, TVPDetectCPU,
              TVPJPEGLoadPrecision, TVPGL_ASM_Init
其它          TVPListDir, ExtractFileDir, TVPShowPopMenu, TVPGetWindowListAt
```

**结论**：引擎②最后一段不是「一个大部件」，而是**约 20 个 Windows 系统层符号**的接线，
每个都很小（多数是「无窗口/无插件/无手柄时返回什么」的决定）。这一轮写了其中一半，
剩下的一半需要逐个决定语义，而不是靠猜。

为保证构建全绿，`LayerIntf.cpp` 等仍**没有**加入构建（不留坏目标）；9 个工具全部
回归通过（`krkrpresent` 逐像素 0 差异）。

### 14. 位图层的字体与文字度量（本轮新增，9 个工具全绿）
从 win32 版搬进 `visual/darwin/LayerBitmapImpl.cpp` 的三段**纯可移植**代码：

| 函数 | 作用 |
|---|---|
| `tTVPNativeBaseBitmap::ApplyFont()` | 解析该位图使用的字体（走栅格化器 + 预渲染字体映射） |
| `GetFontList` / `GetTextSize` / `GetTextWidth` / `GetTextHeight` / `GetEscWidthX/Y` / `GetEscHeightX/Y` / `GetFontGlyphDrawRect` | 文字度量：字形推进、行高、转义宽度、绘制矩形 |
| `TVPGlobalFontStateMagic` + `TVPGetPrerenderedMappedFont` | 栅格化器切换计数 + 游戏自带位图字体的映射查询（放进 `FontRasterizers_Darwin.cpp`） |

另加 `platform/compat/xmmlib.h`（空兼容头：`sound/MathAlgorithms.h` 只是 include 它，
不使用其中任何符号），以及 `platform/darwin/darwin_visual_services.cpp` 里
**约 20 个系统层符号**的实实现或空实现（事件、剪贴板、屏幕度量、系统控制、
插件/手柄、脚本异常、LocaleConfigManager…）。

**结果**：`LayerIntf.cpp`/`ScriptMgnIntf.cpp`/`Application.cpp`/`LayerManager.cpp`
这一批**都能编译**；剩余缺口 = 文字**绘制**（`DrawTextSingle`/`DrawTextMultiple`，
上游是 GDI 实现，需按预渲染字形 + tvpgl 重写）+ 脚本侧 `Window`/`MenuItem` 类
（定义在 VCL 应用层）。

### 15. 文字绘制跑通 → `krkrtext` ✅（关键里程碑）
意外的好消息：**上游的文字绘制也没有一处 GDI 调用**。`InternalBlendText` 的做法是
「字形覆盖率 → 临时纹理 → 向渲染管理器要 `ApplyColorMap`/`ApplyColorMap_d`/
`AlphaBlend_a`/`RemoveOpacity` 方法 → `OperateRect` 合成」，软件渲染管理器全都提供了。

新增 `visual/darwin/TextDraw_Darwin.cpp`（从 win32 版搬移，非重写）：
`InternalBlendText` / `InternalDrawText` / `DrawGlyph` / `DrawTextSingle` /
`DrawTextMultiple` / `DrawText`，外加字形缓存（`TVPFontCache` + `TVPGetCharacter`）。

**验证**（`krkrtext`，全部走引擎 API）：
```
$ ./build/krkrtext /tmp/text2.bmp 40 "あ漢A9 テスト"
drawn: 6083 non-background pixel(s)
wrote /tmp/text2.bmp
```
产物见 `screenshots/text-on-bitmap.png`：同一句「あ漢A9 テスト」用 `bmAlpha` 与
`bmCopy` 两种混合模式画了两行，字形清晰、抗锯齿正常。

坑：`DrawTextSingle` 只画**一个**字符，字符串要走 `DrawText`（内部会分派到
`DrawTextMultiple`）；字体栅格化器需要**懒初始化**（Windows 版在应用启动时初始化，
工具里没人做，`GetCurrentRasterizer()` 里补了）。

### 16. 图层系统的接线勘察（本轮结论：可以切到「真实现」了）
把 `LayerIntf.cpp`（图层系统本体）、`ScriptMgnIntf.cpp`（注册全部 TVP 类）、
`WindowIntf.cpp`、`Application.cpp`、`EventIntf.cpp`、`LayerManager.cpp`、
`TransIntf`/`RectItf`/`GraphicsLoaderIntf`/`BitmapIntf`、`BasicDrawDevice`、
`GraphicsLoaderImpl`、`NativeEventQueue`、`SysInitImpl` 等**全部加进构建后**：

1. **它们都能编译**
2. 缺口逐项补齐后，链接阶段开始报 **25 个 duplicate symbol** ——
   这 25 个正是我之前为「让代码先编过」写的平台桩，而现在**引擎的真实实现已经进来了**：
   `EventIntf.cpp` 的真实事件队列、`Application.cpp` 的真实应用层、
   `GraphicsLoaderIntf.cpp` 的图形缓存、`WindowIntf.cpp` 的真实 Window 类……

**这意味着移植进入新阶段**：不再需要「桩出一个能链接的世界」，而是把真实应用层接上、
删掉自己的桩。这是引擎② 的**最后一步**，内容明确：

| 要做的 | 说明 |
|---|---|
| 删掉约 25 个平台桩 | 它们和真实现冲突（`darwin_visual_services.cpp` 里的绝大部分） |
| 保留真正平台的部分 | SDL 窗口创建/送显（`DrawDevice_Darwin`）、文件媒体、字体钩子、剪贴板 |
| 补 `tTVPNativeBaseBitmap::ApplyFont` 之外的窗口类 | 脚本侧 `Window` 需要平台窗口对象（50 纯虚的 `tTVPWindow`） |

本轮为保证仓库绿色，构建配置回退到已验证状态（10 个工具全绿），清理工作留给下一轮
一次做完——那是一次性的删除+接线，比继续加桩更有价值。

### 17. 合并成单一引擎库，接上真实应用层 ✅（阶段性节点）
上一轮发现「25 个 duplicate symbol 全是我的桩」，这一轮把这件事做完了：

1. **两个静态库合并成一个**（`krkr`）——`krkrbase` 和 `krkrvis` 本来就互相引用
   （脚本管理器注册视觉类、图层抛脚本异常），分成两个库只会造成符号在两库间反复
   跳成「既重复又缺失」。
2. **删掉冲突的桩**：`Application` 全局、`PrintConsole`、`TVPAdd/RemoveContinuousEventHook`、
   `TVPAddCompactEventHook`、`TVPFireOnApplicationActivateEvent`、`TVPGetMessageByLocale`、
   `TVPShowSimpleMessageBox`、`TVPInitWindowOptions`、`TVPClearGraphicCache`、
   `ColorToRGB`、`ExePath`、`ExtractFileDir`、`TVPGetCursor`、`TVPInvokeEvents`、
   `TVPEventReceived`、`TVPGetWindowCount/ListAt`、`TVPDoSaveSystemVariables`、
   `tTJSNI_BaseWindow::UpdateContent` …… 全部由引擎的真实实现接管。
3. **删掉我那个 `System` 替身类**（`darwin_system.cpp`）——引擎自带 `tTJSNC_System`，
   工具改为注册 `TVPCreateNativeClass_System()`。
4. 现在库里有：真实的事件队列（`EventIntf`+`NativeEventQueue`）、真实应用层
   （`Application.cpp`）、真实图层系统（`LayerIntf`+`LayerManager`+`WindowIntf`）、
   真实绘制设备（`DrawDevice`+`BasicDrawDevice`）、真实脚本管理器（`ScriptMgnIntf`）。

**10 个工具全部构建通过并回归正常**：`krkrblit` 全过、`krkrfont` 渲染、
`krkrpresent` 逐像素 0 差异、`krkrtext` 正常绘制。

**仍由桌面提供的**（真正平台专属，不可替代）：SDL 窗口与送显、文件存储媒体、
字体钩子、剪贴板、软件渲染管理器的纹理、`LayerBitmapImpl`/`TextDraw` 的平台实现。

### 18. 图层首次填充崩溃：定位进展（未解）

**现象**：脚本 `new Window()` 成功，紧接着 `new Layer(...)` 在图层构造函数里崩：

```
tTVPRenderMethod_FillARGB::DoRender   ← RenderManager.cpp:1038
  ← tTVPSoftwareRenderManager::OperateRect
    ← iTVPBaseBitmap::Fill
      ← tTJSNI_BaseLayer::tTJSNI_BaseLayer()
```

**已知**：崩在 `RenderManager.cpp:1038`：
```cpp
tjs_uint8 *dest = (tjs_uint8*)_tar->GetScanLineForWrite(rect.top) + rect.left * (is32bpp ? 4 : 1);
```
这条语句在 `GetScanLineForWrite` 返回空指针时会崩（空指针 + 偏移后立即写入）。

**已排查**：
- 加了 `KRKR_DEBUG_BITMAP=1` 的探针到我们位图层的 `GetScanLineForWrite`/`GetTextureForRender`
  → **没有输出**，说明崩溃时的目标纹理不是我们的位图对象，而是引擎自己创建的软件纹理
- 已修两处相关问题（都保留）：位图请求**未压缩**纹理（`RENDER_CREATE_TEXTURE_FLAG_NO_COMPRESS`）；
  `SetSize` 也把 0 尺寸钳到 1（之前只钳了 `Recreate`）

**本轮又推进两层**（都验证过）：

1. **`tTVPBitmap` 分配是好的**：直接构造 `tTVPBitmap(32,32,32)` → `pitch 128, bits 非空, 扫描线可用`
   → 排除「像素缓冲分配失败」这个可能。
2. **位图现在自己持有像素缓冲**：因为 `tTVPSoftwareTexture2D` 虽然会自分配，但
   `CreateTexture2D(pixel,…)` 在有指针时创建的是**静态视图**；位图契约是「拥有自己的像素」，
   所以 `CreateOwnedTexture()` 自己 `calloc`、以静态视图包装、并在替换/析构时释放。

**仍未解，但候选已缩到两个**：
- `tTVPSoftwareTexture2D_static::Update()` 会**重新赋值 BmpData**——若某处以 `nullptr` 调 Update，
  之后写入必崩
- `CreateTexture2D(neww, newh, tex)` 构造的 `tTVPSoftwareTexture2D(tex,…)` 以一个
  `tTVPSoftwareTexture2D_static(nullptr, 0, w, h, format)` 起手，若其后的分配没接上，
  BmpData 留空

**下一步该查**：`tTVPSoftwareTexture2D::GetScanLineForWrite` 的实现——什么情况下返回空？
（压缩变体 `tTVPSoftwareTexture2D_compress` 的写法需要先解压，若它被创建出来就会走那条路径；
另一个方向是 `rect.top` 超出了纹理高度，即某处的尺寸约定不一致。）

## 窗口层的真实工作量（下一步，已勘察清楚）

渲染要落地，缺的是**平台位图层与窗口层**，上游是 Win32 GDI 实现：

| 文件 | 行数 | 作用 |
|---|---|---|
| `visual/win32/LayerBitmapImpl.cpp` | 1607 | `tTVPNativeBaseBitmap`（GetWidth/GetScanLine…）——**图像解码器和所有图层都依赖它** |
| `visual/win32/WindowImpl.cpp` | 2432 | 窗口/DrawDevice |
| `visual/win32/BasicDrawDevice.cpp` | 1009 | 绘制设备 |
| `visual/win32/DrawDevice.cpp` `DInputMgn.cpp` `VideoOvlImpl.cpp` `PassThroughDrawDevice.cpp` | ~3400 | 其余 |

**勘察结论（本轮实测）**：
- `visual/*.cpp` 里 26/33 直接可编译，`win32/` 那批里 `WindowImpl` / `BasicDrawDevice` /
  `DrawDevice` / `LayerImpl` / `VideoOvlImpl` 也**全部可编译且零 Win32 调用**——它们是纯抽象 C++
- 但 `environ/win32/` 的**应用窗口**这批是真的 Win32/VCL 专用（`windowsx.h`、`Classes.hpp`、
  `vcl.h` 是 Borland C++ Builder），**无法复用**；`tTVPWindow` 有 **50 个纯虚函数**
- `environ/Application.cpp` 可编译，它要的是平台自己的窗体类（`TTVPWindowForm` 的等价物）

所以要写的就剩：一个 darwin 窗体（50 个纯虚，多数是琐碎的标题/尺寸/光标/IME/按键注入，
真正有分量的是 `UpdateDrawBuffer` 送显——**已由 `krkrpresent` 验证可行**）+ 与
`tTVPBasicDrawDevice` 的接线。

## 剩余工作（按依赖顺序）
1. ~~XP3 归档层~~ ✅
2. **TVP 扩展类**（`Storage`/`Scripts`/`Window`/`System` 的完整版）——KAG 脚本直接调它们
3. **窗口 + 渲染**：用 SDL2 替换 cocos2d 那 2448 行，渲染用 `visual/ogl/`
4. **音频**：`sound/` 的 WaveIntf 需要 darwin 实现（SDL2 audio 或 CoreAudio）
5. ⚠️ **图像/动画**：上游重度依赖 libgdiplus/cairo/pixman/opencv/ffmpeg/bpg/jxrlib
   这些 vendored 库（tarball 里没有，需单独拉）——这是最大的工作量来源

## 诚实评估
Kirikiroid2 的「桌面化」不是修 bug 级别，是**重写应用层 + 重建 20 个第三方依赖**，
量级远大于引擎①④。已把地基（TJS2）做成可独立验证的一层，后续按上述顺序推进。
真跑 KiriKiri 游戏（如 Fate）在短期内的现实路径仍是 **Sikarugir(Wine)** 兜底。
