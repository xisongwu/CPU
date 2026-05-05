#include <windows.h>
#include <cstdio>

#define TIMER_ID 1
#define ID_SHOW 1001
#define ID_HIDE 1002
#define ID_CLOSE 1003
#define WM_TRAYMESSAGE (WM_USER + 100)

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void CreateTrayIcon(HWND hwnd);
void DestroyTrayIcon(HWND hwnd);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"FloatMonitor";

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassW(&wc);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    int windowWidth = 160;
    int windowHeight = 80;
    int windowX = screenWidth - windowWidth - 20;
    int windowY = 20;

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        CLASS_NAME,
        L"",
        WS_POPUP | WS_SYSMENU,
        windowX, windowY, windowWidth, windowHeight,
        NULL, NULL, hInstance, NULL
    );

    SetLayeredWindowAttributes(hwnd, 0, 220, LWA_ALPHA);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    CreateTrayIcon(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static FILETIME prevIdleTime = {0};
    static FILETIME prevKernelTime = {0};
    static FILETIME prevUserTime = {0};
    static double cpuUsage = 0;
    static double memoryUsage = 0;
    static ULONGLONG totalPhys = 0;

    switch (msg) {
    case WM_CREATE: {
        MEMORYSTATUSEX memStatus;
        memStatus.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memStatus)) {
            totalPhys = memStatus.ullTotalPhys;
        }
        GetSystemTimes(&prevIdleTime, &prevKernelTime, &prevUserTime);
        SetTimer(hwnd, TIMER_ID, 1000, NULL);
        break;
    }
    case WM_TIMER: {
        FILETIME idleTime, kernelTime, userTime;
        GetSystemTimes(&idleTime, &kernelTime, &userTime);

        ULONGLONG idleDiff = (ULONGLONG)idleTime.dwHighDateTime * 4294967296ULL + idleTime.dwLowDateTime -
                            ((ULONGLONG)prevIdleTime.dwHighDateTime * 4294967296ULL + prevIdleTime.dwLowDateTime);
        
        ULONGLONG kernelDiff = (ULONGLONG)kernelTime.dwHighDateTime * 4294967296ULL + kernelTime.dwLowDateTime -
                              ((ULONGLONG)prevKernelTime.dwHighDateTime * 4294967296ULL + prevKernelTime.dwLowDateTime);
        
        ULONGLONG userDiff = (ULONGLONG)userTime.dwHighDateTime * 4294967296ULL + userTime.dwLowDateTime -
                            ((ULONGLONG)prevUserTime.dwHighDateTime * 4294967296ULL + prevUserTime.dwLowDateTime);

        ULONGLONG totalDiff = kernelDiff + userDiff;
        if (totalDiff > 0) {
            cpuUsage = (100.0 - (idleDiff * 100.0 / totalDiff));
        }

        prevIdleTime = idleTime;
        prevKernelTime = kernelTime;
        prevUserTime = userTime;

        MEMORYSTATUSEX memStatus;
        memStatus.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memStatus) && totalPhys > 0) {
            memoryUsage = (100.0 - (memStatus.ullAvailPhys * 100.0 / totalPhys));
        }

        InvalidateRect(hwnd, NULL, TRUE);
        break;
    }
    case WM_SETCURSOR: {
        SetCursor(LoadCursor(NULL, IDC_ARROW));
        return TRUE;
    }
    case WM_CONTEXTMENU: {
        POINT pt;
        pt.x = LOWORD(lParam);
        pt.y = HIWORD(lParam);
        ClientToScreen(hwnd, &pt);

        HMENU hMenu = CreatePopupMenu();
        AppendMenuW(hMenu, MF_STRING, ID_HIDE, L"隐藏");
        AppendMenuW(hMenu, MF_STRING, ID_CLOSE, L"关闭");

        TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
        DestroyMenu(hMenu);
        break;
    }
    case WM_TRAYMESSAGE: {
        if (lParam == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);

            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, ID_SHOW, L"显示");
            AppendMenuW(hMenu, MF_STRING, ID_CLOSE, L"退出");

            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        } else if (lParam == WM_LBUTTONUP) {
            ShowWindow(hwnd, SW_SHOW);
        }
        break;
    }
    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case ID_SHOW:
            ShowWindow(hwnd, SW_SHOW);
            break;
        case ID_HIDE:
            ShowWindow(hwnd, SW_HIDE);
            break;
        case ID_CLOSE:
            DestroyTrayIcon(hwnd);
            PostQuitMessage(0);
            break;
        }
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);
        
        HBRUSH bgBrush = CreateSolidBrush(RGB(30, 30, 30));
        FillRect(hdc, &rect, bgBrush);
        DeleteObject(bgBrush);

        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkMode(hdc, TRANSPARENT);

        HFONT font = CreateFontW(12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT oldFont = (HFONT)SelectObject(hdc, font);

        char buffer[50];
        RECT textRect = rect;
        textRect.top += 10;
        textRect.left += 10;

        sprintf(buffer, "CPU: %.1f%%", cpuUsage);
        DrawTextA(hdc, buffer, -1, &textRect, DT_LEFT);

        textRect.top += 25;
        sprintf(buffer, "MEM: %.1f%%", memoryUsage);
        DrawTextA(hdc, buffer, -1, &textRect, DT_LEFT);

        SelectObject(hdc, oldFont);
        DeleteObject(font);

        EndPaint(hwnd, &ps);
        break;
    }
    case WM_DESTROY:
        KillTimer(hwnd, TIMER_ID);
        DestroyTrayIcon(hwnd);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void CreateTrayIcon(HWND hwnd) {
    NOTIFYICONDATAW nid = {0};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYMESSAGE;
    
    HICON hIcon = LoadIcon(NULL, IDI_APPLICATION);
    nid.hIcon = hIcon;
    
    wcscpy(nid.szTip, L"System Monitor");
    
    Shell_NotifyIconW(NIM_ADD, &nid);
    
    if (hIcon) {
        DestroyIcon(hIcon);
    }
}

void DestroyTrayIcon(HWND hwnd) {
    NOTIFYICONDATAW nid = {0};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hwnd;
    nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}
