#include <windows.h>
#include <stdio.h>
#include <ctype.h>
#include <shlwapi.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")

#define TRAY_ICON_ID 1
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_SHOW 1001
#define ID_TRAY_EXIT 1002

#define DIRECTION_LEFT 0
#define DIRECTION_RIGHT 1
#define DIRECTION_UP 2
#define DIRECTION_DOWN 3
#define IS_DOWN 1
#define IS_UP 0
#define whitelist_max_length 200
#define WASD_ID 100
#define ARROWS_ID 200
#define CUSTOM_ID 300
#define ESC_BIND_ID 400

#define KEY_ESC 1
#define KEY_W 0x57
#define KEY_A 0x41
#define KEY_S 0x53
#define KEY_D 0x44
#define KEY_E 0x45
#define KEY_UP VK_UP
#define KEY_LEFT VK_LEFT
#define KEY_DOWN VK_DOWN
#define KEY_RIGHT VK_RIGHT

HWND main_window;
NOTIFYICONDATA nid;

const char* CONFIG_NAME = "socd_config.conf";
const LPCWSTR CLASS_NAME = L"SOCD_CLASS";
char config_line[100];
char focused_program[MAX_PATH];
char programs_whitelist[whitelist_max_length][MAX_PATH] = { 0 };

HHOOK kbhook;
int hook_is_installed = 0;
int listening_for_esc_bind = 0;

int real[4];
int virtual[4];
int DEFUALT_DISABLE_BIND = KEY_E;
int WASD[4] = { KEY_A, KEY_D, KEY_W, KEY_S };
int ARROWS[4] = { KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN };
int CUSTOM_BINDS[4];
int DISABLE_BIND;
int disableKeyPressed;
int ESC_BIND = 0;
int ESC_PRESSED;

int show_error_and_quit(char* text) {
    int error = GetLastError();
    char* error_buffer = malloc(strlen(text) + 10);
    sprintf_s(error_buffer, text, error);
    MessageBox(NULL, error_buffer, "RIP", MB_OK | MB_ICONERROR);
    ExitProcess(1);
}

void paint_esc_label(lParam) {
    LPCWSTR label;
    if (listening_for_esc_bind) {
        label = L"Press button to bind ESC to...";
    }
    else {
        if (!ESC_BIND) {
            label = L"ESC isn't bound";
        }
        else {
            wchar_t label_buffer[100];
            wchar_t key_name_buf[15];
            GetKeyNameTextW(lParam, key_name_buf, 15);
            wprintf(L"key name is %s\nvirtual key is %X", key_name_buf, ESC_BIND);
            wsprintfW(label_buffer, L"ESC is bound to %s", key_name_buf);
            label = label_buffer;
        }
    }
    HWND hwnd = CreateWindowExW(0, L"STATIC", label, WS_VISIBLE | WS_CHILD, 120, 167, 400, 30, main_window, (HMENU)500, (HINSTANCE)main_window, NULL);
    if (hwnd == NULL) {
        show_error_and_quit("Failed to create ESC bind label, error code is %d");
    }
}

void write_settings(int* bindings, int disableBind, int esc_bind) {
    FILE* config_file = fopen(CONFIG_NAME, "w");
    if (config_file == NULL) {
        perror("Couldn't open the config file");
        return;
    }
    for (int i = 0; i < 4; i++) {
        fprintf(config_file, "%X\n", bindings[i]);
    }
    fprintf(config_file, "%X\n", disableBind);
    fprintf(config_file, "%X\n", esc_bind);
    for (int i = 0; i < whitelist_max_length; i++) {
        if (programs_whitelist[i][0] == '\0') {
            break;
        }
        fprintf(config_file, "%s\n", programs_whitelist[i]);
    }
    fclose(config_file);
}

void set_bindings(int* bindings, int disableBind, int esc_bind) {
    CUSTOM_BINDS[0] = bindings[0];
    CUSTOM_BINDS[1] = bindings[1];
    CUSTOM_BINDS[2] = bindings[2];
    CUSTOM_BINDS[3] = bindings[3];
    DISABLE_BIND = disableBind;
    ESC_BIND = esc_bind;
}

void read_settings() {
    FILE* config_file = fopen(CONFIG_NAME, "r+");
    if (config_file == NULL) {
        set_bindings(WASD, DEFUALT_DISABLE_BIND, ESC_BIND);
        write_settings(WASD, DEFUALT_DISABLE_BIND, ESC_BIND);
        return;
    }
    for (int i = 0; i < 4; i++) {
        char* result = fgets(config_line, 100, config_file);
        int button = (int)strtol(result, NULL, 16);
        CUSTOM_BINDS[i] = button;
    }
    char* result = fgets(config_line, 100, config_file);
    DISABLE_BIND = (int)strtol(result, NULL, 16);
    result = fgets(config_line, 100, config_file);
    ESC_BIND = (int)strtol(result, NULL, 16);
    int i = 0;
    while (fgets(programs_whitelist[i], MAX_PATH, config_file) != NULL) {
        programs_whitelist[i][strcspn(programs_whitelist[i], "\r\n")] = 0;
        i++;
    }
    for (int i = 0; i < whitelist_max_length; i++) {
        if (programs_whitelist[i][0] == '\0') {
            break;
        }
    }
    fclose(config_file);
}

int find_opposing_key(int key) {
    if (key == CUSTOM_BINDS[DIRECTION_LEFT]) {
        return CUSTOM_BINDS[DIRECTION_RIGHT];
    }
    if (key == CUSTOM_BINDS[DIRECTION_RIGHT]) {
        return CUSTOM_BINDS[DIRECTION_LEFT];
    }
    if (key == CUSTOM_BINDS[DIRECTION_UP]) {
        return CUSTOM_BINDS[DIRECTION_DOWN];
    }
    if (key == CUSTOM_BINDS[DIRECTION_DOWN]) {
        return CUSTOM_BINDS[DIRECTION_UP];
    }
    return -1;
}

int find_index_by_key(int key) {
    if (key == CUSTOM_BINDS[DIRECTION_LEFT]) {
        return DIRECTION_LEFT;
    }
    if (key == CUSTOM_BINDS[DIRECTION_RIGHT]) {
        return DIRECTION_RIGHT;
    }
    if (key == CUSTOM_BINDS[DIRECTION_UP]) {
        return DIRECTION_UP;
    }
    if (key == CUSTOM_BINDS[DIRECTION_DOWN]) {
        return DIRECTION_DOWN;
    }
    return -1;
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    KBDLLHOOKSTRUCT* kbInput = (KBDLLHOOKSTRUCT*)lParam;
    if (nCode != HC_ACTION || kbInput->flags & LLKHF_INJECTED) {
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }
    INPUT input;
    int key = kbInput->vkCode;
    if (key == DISABLE_BIND) {
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            disableKeyPressed = IS_DOWN;
        }
        else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
            disableKeyPressed = IS_UP;
        }
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }
    if (key == ESC_BIND) {
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            ESC_PRESSED = IS_DOWN;
            input.type = INPUT_KEYBOARD;
            input.ki = (KEYBDINPUT){ 0, KEY_ESC, KEYEVENTF_SCANCODE, 0, 0 };
            SendInput(1, &input, sizeof(INPUT));
        }
        else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
            ESC_PRESSED = IS_UP;
            input.type = INPUT_KEYBOARD;
            input.ki = (KEYBDINPUT){ 0, KEY_ESC, KEYEVENTF_KEYUP | KEYEVENTF_SCANCODE, 0, 0 };
            SendInput(1, &input, sizeof(INPUT));
        }
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }
    int opposing = find_opposing_key(key);
    if (opposing < 0) {
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }
    int index = find_index_by_key(key);
    int opposing_index = find_index_by_key(opposing);
    opposing = MapVirtualKeyW(opposing, MAPVK_VK_TO_VSC_EX);
    if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
        real[index] = IS_DOWN;
        virtual[index] = IS_DOWN;
        if (real[opposing_index] == IS_DOWN && virtual[opposing_index] == IS_DOWN && disableKeyPressed == IS_UP) {
            input.type = INPUT_KEYBOARD;
            input.ki = (KEYBDINPUT){ 0, opposing, KEYEVENTF_KEYUP | KEYEVENTF_SCANCODE, 0, 0 };
            SendInput(1, &input, sizeof(INPUT));
            virtual[opposing_index] = IS_UP;
        }
    }
    else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
        real[index] = IS_UP;
        virtual[index] = IS_UP;
        if (real[opposing_index] == IS_DOWN && disableKeyPressed == IS_UP) {
            input.type = INPUT_KEYBOARD;
            input.ki = (KEYBDINPUT){ 0, opposing, KEYEVENTF_SCANCODE, 0, 0 };
            SendInput(1, &input, sizeof(INPUT));
            virtual[opposing_index] = IS_DOWN;
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void set_kb_hook(HINSTANCE instance) {
    if (!hook_is_installed) {
        kbhook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, instance, 0);
        if (kbhook != NULL) {
            hook_is_installed = 1;
        }
        else {
            printf("hook failed\n");
        }
    }
}

void unset_kb_hook() {
    if (hook_is_installed) {
        UnhookWindowsHookEx(kbhook);
        real[DIRECTION_LEFT] = IS_UP;
        real[DIRECTION_RIGHT] = IS_UP;
        real[DIRECTION_UP] = IS_UP;
        real[DIRECTION_DOWN] = IS_UP;
        virtual[DIRECTION_LEFT] = IS_UP;
        virtual[DIRECTION_RIGHT] = IS_UP;
        virtual[DIRECTION_UP] = IS_UP;
        virtual[DIRECTION_DOWN] = IS_UP;
        hook_is_installed = 0;
    }
}

void get_focused_program() {
    HWND inspected_window = GetForegroundWindow();
    DWORD process_id = 0;
    GetWindowThreadProcessId(inspected_window, &process_id);
    if (process_id == 0) {
        return;
    }
    HANDLE hproc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, 0, process_id);
    if (hproc == NULL) {
        if (GetLastError() == 5) {
            return;
        }
        show_error_and_quit("Couldn't open active process, error code is: %d");
    }
    DWORD filename_size = MAX_PATH;
    QueryFullProcessImageName(hproc, 0, focused_program, &filename_size);
    CloseHandle(hproc);
    PathStripPath(focused_program);
}

void detect_focused_program(
    HWINEVENTHOOK hWinEventHook,
    DWORD event,
    HWND window,
    LONG idObject,
    LONG idChild,
    DWORD idEventThread,
    DWORD dwmsEventTime
) {
    get_focused_program();
    HINSTANCE hInstance = (HINSTANCE)GetModuleHandle(NULL);
    for (int i = 0; i < whitelist_max_length; i++) {
        if (strcmp(focused_program, programs_whitelist[i]) == 0) {
            set_kb_hook(hInstance);
            return;
        }
        else if (programs_whitelist[i][0] == '\0') {
            break;
        }
    }
    unset_kb_hook();
}

void ShowContextMenu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();
    if (hMenu) {
        InsertMenu(hMenu, -1, MF_BYPOSITION, ID_TRAY_SHOW, TEXT("Show Window"));
        InsertMenu(hMenu, -1, MF_BYPOSITION, ID_TRAY_EXIT, TEXT("Exit"));
        SetForegroundWindow(hwnd);
        TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_RIGHTALIGN, pt.x, pt.y, 0, hwnd, NULL);
        DestroyMenu(hMenu);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        memset(&nid, 0, sizeof(NOTIFYICONDATA));
        nid.cbSize = sizeof(NOTIFYICONDATA);
        nid.hWnd = hwnd;
        nid.uID = TRAY_ICON_ID;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAYICON;
        nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        lstrcpy(nid.szTip, TEXT("SOCD Cleaner"));
        Shell_NotifyIcon(NIM_ADD, &nid);
        break;
    case WM_DESTROY:
        Shell_NotifyIcon(NIM_DELETE, &nid);
        PostQuitMessage(0);
        return 0;
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            ShowContextMenu(hwnd);
        }
        else if (lParam == WM_LBUTTONDBLCLK) {
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
        }
        break;
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_TRAY_SHOW) {
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
        }
        else if (LOWORD(wParam) == ID_TRAY_EXIT) {
            DestroyWindow(hwnd);
        }
        SetFocus(main_window);
        if (wParam == WASD_ID) {
            set_bindings(WASD, DEFUALT_DISABLE_BIND, ESC_BIND);
            write_settings(WASD, DEFUALT_DISABLE_BIND, ESC_BIND);
            return 0;
        }
        else if (wParam == ARROWS_ID) {
            set_bindings(ARROWS, DEFUALT_DISABLE_BIND, ESC_BIND);
            write_settings(ARROWS, DEFUALT_DISABLE_BIND, ESC_BIND);
            return 0;
        }
        else if (wParam == ESC_BIND_ID) {
            listening_for_esc_bind = !listening_for_esc_bind;
            paint_esc_label(lParam);
            return 0;
        }
        break;
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

int main() {
    FreeConsole();

    ESC_PRESSED = IS_UP;
    disableKeyPressed = IS_UP;
    real[DIRECTION_LEFT] = IS_UP;
    real[DIRECTION_RIGHT] = IS_UP;
    real[DIRECTION_UP] = IS_UP;
    real[DIRECTION_DOWN] = IS_UP;
    virtual[DIRECTION_LEFT] = IS_UP;
    virtual[DIRECTION_RIGHT] = IS_UP;
    virtual[DIRECTION_UP] = IS_UP;
    virtual[DIRECTION_DOWN] = IS_UP;

    HINSTANCE hInstance = (HINSTANCE)GetModuleHandle(NULL);
    read_settings();
    if (programs_whitelist[0][0] == '\0') {
        set_kb_hook(hInstance);
    }
    else {
        SetWinEventHook(EVENT_OBJECT_FOCUS, EVENT_OBJECT_FOCUS, hInstance, (WINEVENTPROC)detect_focused_program, 0, 0, WINEVENT_OUTOFCONTEXT);
    }

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS_NAME;
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (RegisterClassExW(&wc) == 0) {
        show_error_and_quit("Failed to register window class, error code is %d");
    }

    main_window = CreateWindowExW(0, CLASS_NAME, L"SOCD Cleaner", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 250, 85, NULL, NULL, hInstance, NULL);
    if (main_window == NULL) {
        show_error_and_quit("Failed to create a window, error code is %d");
    }

    HWND wasd_hwnd = CreateWindowExW(0, L"BUTTON", L"WASD", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON, 10, 0, 100, 30, main_window, (HMENU)WASD_ID, hInstance, NULL);
    if (wasd_hwnd == NULL) {
        show_error_and_quit("Failed to create WASD radiobutton, error code is %d");
    }

    HWND arrows_hwnd = CreateWindowExW(0, L"BUTTON", L"Arrows", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON, 10, 20, 100, 30, main_window, (HMENU)ARROWS_ID, hInstance, NULL);
    if (arrows_hwnd == NULL) {
        show_error_and_quit("Failed to create Arrows radiobutton, error code is %d");
    }

    int code = MapVirtualKeyW(ESC_BIND, MAPVK_VK_TO_VSC_EX);
    int code_lparam = code << 16;
    if (0xE000 & code) {
        code_lparam = (1 << 24) | code_lparam;
    }
    paint_esc_label(code_lparam);

    int check_id;
    if (memcmp(CUSTOM_BINDS, WASD, sizeof(WASD)) == 0) {
        check_id = WASD_ID;
    }
    else if (memcmp(CUSTOM_BINDS, ARROWS, sizeof(ARROWS)) == 0) {
        check_id = ARROWS_ID;
    }
    else {
        check_id = CUSTOM_ID;
    }
    if (CheckRadioButton(main_window, WASD_ID, CUSTOM_ID, check_id) == 0) {
        show_error_and_quit("Failed to select default keybindings, error code is %d");
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN) {
            if (listening_for_esc_bind) {
                UINT real_virtual_key_code = msg.wParam;
                listening_for_esc_bind = 0;
                ESC_BIND = 0;
                UINT scancode = (msg.lParam & 0x00ff0000) >> 16;
                int extended = msg.lParam >> 24 & 1;
                switch (real_virtual_key_code) {
                case VK_SHIFT:
                    real_virtual_key_code = MapVirtualKeyW(scancode, MAPVK_VSC_TO_VK_EX);
                    break;
                case VK_CONTROL:
                    real_virtual_key_code = extended ? VK_RCONTROL : VK_LCONTROL;
                    break;
                case VK_MENU:
                    real_virtual_key_code = extended ? VK_RMENU : VK_LMENU;
                    break;
                }
                if (real_virtual_key_code != VK_ESCAPE) {
                    ESC_BIND = real_virtual_key_code;
                    write_settings(CUSTOM_BINDS, DISABLE_BIND, ESC_BIND);
                }
                paint_esc_label(msg.lParam);
            }
        }
    }
    return 0;
}