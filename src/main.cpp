#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "json.hpp"

// ---------------------------------------------------------------------------
// DWM / acrylic constants that some MinGW headers do not expose yet.
// ---------------------------------------------------------------------------
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMSBT_TRANSIENTWINDOW
#define DWMSBT_TRANSIENTWINDOW 3
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

typedef enum _ACCENT_STATE {
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_GRADIENT = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
    ACCENT_INVALID_STATE = 5
} ACCENT_STATE;

typedef struct _ACCENT_POLICY {
    ACCENT_STATE AccentState;
    int AccentFlags;
    int GradientColor; // ABGR
    int AnimationId;
} ACCENT_POLICY;

typedef enum _WINDOWCOMPOSITIONATTRIB {
    WCA_ACCENT_POLICY = 19
} WINDOWCOMPOSITIONATTRIB;

typedef struct _WINDOWCOMPOSITIONATTRIBDATA {
    WINDOWCOMPOSITIONATTRIB Attrib;
    void* pvData;
    SIZE_T cbData;
} WINDOWCOMPOSITIONATTRIBDATA;

typedef BOOL(WINAPI* pfnSetWindowCompositionAttribute)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

// ---------------------------------------------------------------------------
// Data model
// ---------------------------------------------------------------------------
struct AppItem {
    std::wstring name;
    std::wstring path;
    HICON icon = nullptr;
};

struct Appearance {
    int iconSize = 40;
    int padding = 12;
    int gap = 10;
    int tint = 0x99000000; // ABGR, dark frosted tint
    int cornerRadius = 18;
    int margin = 12;       // gap above the taskbar / screen bottom
};

enum class CellType { Icon, Separator, Clock };

struct Cell {
    CellType type = CellType::Icon;
    int x = 0;
    int w = 0;
    int itemIndex = -1;
};

struct Dock {
    HWND hwnd = nullptr;
    HINSTANCE hInstance = nullptr;
    HFONT font = nullptr;
    Appearance appearance;
    std::vector<AppItem> items;
    std::vector<Cell> cells;
    bool showClock = true;
    bool showSeconds = false;
    int hoverIndex = -1;
    int windowWidth = 0;
    int windowHeight = 0;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static std::wstring utf8ToWide(const std::string& text) {
    if (text.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (size <= 0) return L"";
    std::wstring out(size - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &out[0], size);
    return out;
}

static std::wstring pad2(int value) {
    std::wstring s = std::to_wstring(value);
    if (s.size() < 2) s = std::wstring(L"0") + s;
    return s;
}

static std::string readFile(const std::wstring& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return "";
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static std::wstring exeDirectory() {
    wchar_t buf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring path(buf);
    std::size_t pos = path.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"" : path.substr(0, pos);
}

static HICON iconForPath(const std::wstring& path) {
    SHFILEINFOW info = {};
    DWORD_PTR res = SHGetFileInfoW(path.c_str(), 0, &info, sizeof(info), SHGFI_ICON | SHGFI_LARGEICON);
    return res ? info.hIcon : nullptr;
}

static int parseHexColor(const std::string& text, int fallback) {
    // Accepts "#AARRGGBB" or "#RRGGBB".
    if (text.empty() || text[0] != '#') return fallback;
    std::string hex = text.substr(1);
    if (hex.size() == 6) hex = "FF" + hex;
    if (hex.size() != 8) return fallback;
    unsigned value = 0;
    for (char c : hex) {
        value <<= 4;
        if (c >= '0' && c <= '9') value |= c - '0';
        else if (c >= 'a' && c <= 'f') value |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') value |= c - 'A' + 10;
        else return fallback;
    }
    // Convert ARGB -> ABGR for the accent policy.
    unsigned a = (value >> 24) & 0xFF;
    unsigned r = (value >> 16) & 0xFF;
    unsigned g = (value >> 8) & 0xFF;
    unsigned b = value & 0xFF;
    return static_cast<int>((a << 24) | (b << 16) | (g << 8) | r);
}

// ---------------------------------------------------------------------------
// Acrylic / frosted glass
// ---------------------------------------------------------------------------
static void applyLegacyAcrylic(HWND hwnd, int tint) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    auto fn = reinterpret_cast<pfnSetWindowCompositionAttribute>(
        GetProcAddress(user32, "SetWindowCompositionAttribute"));
    if (!fn) return;

    ACCENT_POLICY policy = {};
    policy.AccentState = ACCENT_ENABLE_ACRYLICBLURBEHIND;
    policy.AccentFlags = 2;
    policy.GradientColor = tint;

    WINDOWCOMPOSITIONATTRIBDATA data = {};
    data.Attrib = WCA_ACCENT_POLICY;
    data.pvData = &policy;
    data.cbData = sizeof(policy);
    fn(hwnd, &data);
}

static void applyAcrylic(HWND hwnd, int tint) {
    int backdrop = DWMSBT_TRANSIENTWINDOW;
    HRESULT hr = DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
    if (FAILED(hr)) {
        applyLegacyAcrylic(hwnd, tint);
    }

    int corner = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
static void layout(Dock& dock) {
    const Appearance& a = dock.appearance;
    dock.cells.clear();

    int x = a.padding;
    for (std::size_t i = 0; i < dock.items.size(); i++) {
        Cell cell;
        cell.type = CellType::Icon;
        cell.x = x;
        cell.w = a.iconSize;
        cell.itemIndex = static_cast<int>(i);
        dock.cells.push_back(cell);
        x += a.iconSize + a.gap;
    }

    if (dock.showClock) {
        if (!dock.items.empty()) {
            Cell sep;
            sep.type = CellType::Separator;
            sep.x = x;
            sep.w = 1 + a.gap * 2;
            dock.cells.push_back(sep);
            x += sep.w;
        }
        Cell clock;
        clock.type = CellType::Clock;
        clock.x = x;
        clock.w = 58;
        dock.cells.push_back(clock);
        x += clock.w;
    }

    dock.windowWidth = x + a.padding;
    dock.windowHeight = a.padding * 2 + a.iconSize;
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------
static void paint(Dock& dock, HDC hdc) {
    const Appearance& a = dock.appearance;
    int centerY = a.padding;

    for (const Cell& cell : dock.cells) {
        if (cell.type == CellType::Icon) {
            const AppItem& item = dock.items[cell.itemIndex];
            bool hovered = cell.itemIndex == dock.hoverIndex;
            int size = hovered ? a.iconSize + 8 : a.iconSize;
            int x = cell.x - (hovered ? 4 : 0);
            int y = centerY + (a.iconSize - size) / 2;
            if (item.icon) {
                DrawIconEx(hdc, x, y, item.icon, size, size, 0, nullptr, DI_NORMAL);
            }
        } else if (cell.type == CellType::Separator) {
            HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
            RECT r = {cell.x + a.gap, centerY + 6, cell.x + a.gap + 1, centerY + a.iconSize - 6};
            FillRect(hdc, &r, brush);
            DeleteObject(brush);
        } else if (cell.type == CellType::Clock) {
            SYSTEMTIME st;
            GetLocalTime(&st);
            std::wstring text = pad2(st.wHour) + L":" + pad2(st.wMinute);
            if (dock.showSeconds) {
                text += L":" + pad2(st.wSecond);
            }
            SetTextColor(hdc, RGB(255, 255, 255));
            SetBkMode(hdc, TRANSPARENT);
            RECT r = {cell.x, centerY, cell.x + cell.w, centerY + a.iconSize};
            DrawTextW(hdc, text.c_str(), -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
    }
}

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------
static void loadConfig(Dock& dock) {
    std::wstring path = exeDirectory() + L"\\config.json";
    std::string text = readFile(path);
    if (text.empty()) {
        // Built-in defaults: a few Windows 11 apps plus a clock.
        dock.appearance = Appearance();
        dock.showClock = true;
        const wchar_t* defaults[] = {
            L"C:\\Windows\\System32\\notepad.exe",
            L"C:\\Windows\\System32\\mspaint.exe",
            L"C:\\Windows\\explorer.exe",
            L"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe",
        };
        for (const wchar_t* p : defaults) {
            AppItem item;
            item.path = p;
            item.icon = iconForPath(item.path);
            dock.items.push_back(item);
        }
        return;
    }

    try {
        json::Value root = json::parse(text);
        const json::Value* appearance = root.find("appearance");
        if (appearance && appearance->type == json::Value::Obj) {
            Appearance a;
            a.iconSize = static_cast<int>(appearance->getNumber("iconSize", a.iconSize));
            a.padding = static_cast<int>(appearance->getNumber("padding", a.padding));
            a.gap = static_cast<int>(appearance->getNumber("gap", a.gap));
            a.margin = static_cast<int>(appearance->getNumber("margin", a.margin));
            a.tint = parseHexColor(appearance->getString("tint", ""), a.tint);
            dock.appearance = a;
        }

        const json::Value* modules = root.find("modules");
        if (modules && modules->type == json::Value::Arr) {
            for (const json::Value& mod : modules->array) {
                std::string type = mod.getString("type", "");
                if (type == "launcher") {
                    const json::Value* items = mod.find("items");
                    if (items && items->type == json::Value::Arr) {
                        for (const json::Value& it : items->array) {
                            AppItem item;
                            item.name = utf8ToWide(it.getString("name", ""));
                            item.path = utf8ToWide(it.getString("path", ""));
                            if (!item.path.empty()) {
                                item.icon = iconForPath(item.path);
                                dock.items.push_back(item);
                            }
                        }
                    }
                } else if (type == "clock") {
                    dock.showClock = true;
                    std::string format = mod.getString("format", "HH:mm");
                    dock.showSeconds = format.find("ss") != std::string::npos;
                }
            }
        }
    } catch (...) {
        // Keep defaults on parse errors.
    }
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------
static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Dock* dock = reinterpret_cast<Dock*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_CREATE:
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (dock) paint(*dock, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1; // Let DWM render the acrylic backdrop.

        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lParam);
            int hover = -1;
            for (const Cell& cell : dock->cells) {
                if (cell.type == CellType::Icon && x >= cell.x && x < cell.x + cell.w) {
                    hover = cell.itemIndex;
                    break;
                }
            }
            if (hover != dock->hoverIndex) {
                dock->hoverIndex = hover;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tme);
            return 0;
        }

        case WM_MOUSELEAVE:
            dock->hoverIndex = -1;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lParam);
            for (const Cell& cell : dock->cells) {
                if (cell.type == CellType::Icon && x >= cell.x && x < cell.x + cell.w) {
                    const AppItem& item = dock->items[cell.itemIndex];
                    ShellExecuteW(hwnd, L"open", item.path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                    break;
                }
            }
            return 0;
        }

        case WM_TIMER:
            if (dock && dock->showClock) {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;

        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE; // Clicking the dock never steals focus.

        case WM_NCHITTEST:
            return HTCLIENT;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    SetProcessDPIAware();

    Dock dock;
    dock.hInstance = hInstance;
    loadConfig(dock);
    layout(dock);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"FrostDockWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    RegisterClassW(&wc);

    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    int x = workArea.left + (workArea.right - workArea.left - dock.windowWidth) / 2;
    int y = workArea.bottom - dock.windowHeight - dock.appearance.margin;

    dock.hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP,
        wc.lpszClassName,
        L"FrostDock",
        WS_POPUP,
        x, y, dock.windowWidth, dock.windowHeight,
        nullptr, nullptr, hInstance, nullptr);

    if (!dock.hwnd) return 1;

    SetWindowLongPtrW(dock.hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&dock));
    applyAcrylic(dock.hwnd, dock.appearance.tint);

    dock.font = CreateFontW(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
        L"Segoe UI");

    ShowWindow(dock.hwnd, SW_SHOW);
    UpdateWindow(dock.hwnd);
    SetTimer(dock.hwnd, 1, 1000, nullptr);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    for (AppItem& item : dock.items) {
        if (item.icon) DestroyIcon(item.icon);
    }
    if (dock.font) DeleteObject(dock.font);
    return 0;
}
