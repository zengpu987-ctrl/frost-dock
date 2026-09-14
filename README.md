# FrostDock

一款轻量化的 Windows 11 桌面程序坞：磨砂玻璃（Acrylic）材质、原生 Win32、占用内存极低、模块化配置。

A lightweight Windows 11 desktop dock with frosted-glass (Acrylic) material, native Win32, minimal memory footprint, and a modular config.

---

## 中文说明

### 特点

- 原生 Win32 + GDI，单窗口运行，无 Electron / 无 .NET 运行时，内存占用通常只有几 MB。
- 使用 Windows 11 的 Acrylic 系统背景实现真正的磨砂玻璃模糊，并带圆角。
- 模块化配置：`launcher`（应用图标）、`separator`（分隔线）、`clock`（时钟）可自由组合。
- 点击图标启动应用；悬停图标放大；点击不抢焦点（`MA_NOACTIVATE`）。
- 常驻屏幕底部中央，置顶显示，不显示任务栏按钮。

### 安装与编译

需要 Windows 11（推荐）和以下任一工具链：

#### 方式 A：MSVC（Visual Studio）

1. 安装 Visual Studio（勾选「使用 C++ 的桌面开发」）。
2. 打开「x64 Native Tools Command Prompt for VS」。
3. 在项目目录运行：

```bat
build.bat
```

生成 `FrostDock.exe`。

#### 方式 B：MinGW-w64

```sh
x86_64-w64-mingw32-g++ -std=c++17 -O2 -fexceptions -municode -mwindows \
  src/main.cpp -o FrostDock.exe \
  -ldwmapi -lshell32 -lgdi32 -luser32
```

#### 方式 C：Zig（可跨平台编译 Windows exe）

```sh
zig c++ -std=c++17 -O2 -fexceptions -target x86_64-windows-gnu \
  src/main.cpp -o FrostDock.exe \
  -ldwmapi -lshell32 -lgdi32 -luser32 \
  -Wl,--subsystem,windows
```

### 使用与配置

把 `FrostDock.exe` 和 `config.json` 放在同一个目录，双击运行即可。没有 `config.json` 时会使用内置默认项。

编辑 `config.json`：

```json
{
  "appearance": {
    "iconSize": 42,
    "padding": 14,
    "gap": 12,
    "margin": 12,
    "tint": "#99000000"
  },
  "modules": [
    {
      "type": "launcher",
      "items": [
        { "name": "记事本", "path": "C:\\Windows\\System32\\notepad.exe" },
        { "name": "画图", "path": "C:\\Windows\\System32\\mspaint.exe" }
      ]
    },
    { "type": "separator" },
    { "type": "clock", "format": "HH:mm:ss" }
  ]
}
```

说明：

- `path` 支持 `.exe` 和 `.lnk`（快捷方式）。
- `tint` 是 `#AARRGGBB` 或 `#RRGGBB`，控制磨砂玻璃色调。
- `clock` 的 `format` 支持 `HH:mm` 或 `HH:mm:ss`。
- 模块顺序即显示顺序，`launcher` 可写多个。

### 开机自启（启用）

把程序放入 Windows 启动目录即可：

1. 按 `Win + R`，输入 `shell:startup` 回车。
2. 把 `FrostDock.exe` 的快捷方式放进打开的文件夹。
3. 重启后会自动运行。

也可以把快捷方式加入「任务计划程序」设置为登录时运行。

### 在虚拟机中运行

推荐用 Windows 11 虚拟机验证：

1. 用 UTM（Apple Silicon）或 VMware / VirtualBox / Hyper-V 创建 Windows 11 虚拟机。
2. 把 `FrostDock.exe` 和 `config.json` 拷贝进虚拟机。
3. 双击 `FrostDock.exe`，即可看到屏幕底部中央的磨砂玻璃程序坞。

### 故障处理

- **看不到磨砂效果**：确认系统为 Windows 11；老系统会回退到 `SetWindowCompositionAttribute` 的 Acrylic 模糊，Win10 仍可显示。
- **图标不显示**：确认 `path` 正确；`SHGetFileInfo` 无法取到图标时会自动跳过该图标。
- **点击图标没反应**：确认 `path` 是完整路径，或该程序名在 PATH 中可被 `ShellExecute` 解析。
- **想换位置 / 换图标大小**：修改 `config.json` 的 `margin`、`iconSize`、`gap`。

---

## English Guide

### Features

- Native Win32 + GDI, single window, no Electron and no .NET runtime; memory usage is typically a few MB.
- Real frosted-glass blur via the Windows 11 Acrylic system backdrop, with rounded corners.
- Modular config: `launcher`, `separator`, and `clock` modules can be freely combined.
- Click an icon to launch an app; hover magnifies the icon; the dock never steals focus.
- Pinned to the bottom-center of the screen, always on top, with no taskbar button.

### Build

Windows 11 and one of the following toolchains are recommended.

#### MSVC

Open "x64 Native Tools Command Prompt for VS" and run:

```bat
build.bat
```

#### MinGW-w64

```sh
x86_64-w64-mingw32-g++ -std=c++17 -O2 -fexceptions -municode -mwindows \
  src/main.cpp -o FrostDock.exe \
  -ldwmapi -lshell32 -lgdi32 -luser32
```

#### Zig (cross-compile a Windows exe)

```sh
zig c++ -std=c++17 -O2 -fexceptions -target x86_64-windows-gnu \
  src/main.cpp -o FrostDock.exe \
  -ldwmapi -lshell32 -lgdi32 -luser32 \
  -Wl,--subsystem,windows
```

### Usage and configuration

Put `FrostDock.exe` and `config.json` in the same folder and double-click the exe. Without `config.json`, built-in defaults are used.

The `config.json` format is documented in the Chinese section above. `path` supports both `.exe` and `.lnk`, and `tint` is an `#AARRGGBB` / `#RRGGBB` color for the frosted tint.

### Enable at login

1. Press `Win + R`, type `shell:startup`, and press Enter.
2. Put a shortcut to `FrostDock.exe` into that folder.

### Run in a VM

Create a Windows 11 VM with UTM, VMware, VirtualBox, or Hyper-V, copy `FrostDock.exe` and `config.json` into it, and double-click the exe.

### Troubleshooting

- **No frosted effect**: make sure the OS is Windows 11; older systems fall back to the legacy Acrylic blur.
- **Missing icons**: check the `path` value; items whose icons cannot be resolved are skipped.
- **Click does nothing**: use a full path or a program name resolvable by `ShellExecute`.
- **Change position or size**: edit `margin`, `iconSize`, and `gap` in `config.json`.

---

## License

MIT License. See [LICENSE](LICENSE).
