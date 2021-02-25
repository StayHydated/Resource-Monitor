#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <stdlib.h>
#include <gdiplus.h> // menu drawing
#include <winuser.h> // systemwide hotkey
#include <pdh.h> // pdh
#include <pdhmsg.h> // pdh
#include <stdio.h> // pdh
#include <conio.h> // pdh
#include <fileapi.h> // ini
#include <winbase.h> // ini
#include <shlobj_core.h> //ini
#include <wingdi.h> // color
#include <winreg.h> // open on startup
#include <tchar.h> // open on startup
#include "Resource.h"
#pragma comment (lib,"Gdiplus.lib") // menu drawing
#pragma comment(lib, "pdh.lib") // pdh
using namespace Gdiplus; // menu drawing
using namespace std;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK MenuProc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK AddDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK CounterProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

HINSTANCE hInst;
HWND dialogHwnd = NULL;
HWND parent;
HWND editCounter;

static bool menuToggle = false;
static bool viewToggle;
const int menuHotkey = 1001;
const int viewHotkey = 1002;
const int quitHotkey = 1003;
const int dialogHotkey = 1004;
const int closeDialogs = 2001;
const int delCounter = 2002;
const int ID_TIMER = 1;
wchar_t counterWinClass[] = L"Counter Class";
char* settingsPath;
unsigned int numCounters;
unsigned int uniqueCounterID = 1;

struct SysInfo {
    double cpuPercent = 0;
    double cpuFreq = 0;
    unsigned long int memPercent = 0;
    double memInUse = 0;
    double diskPercent = 0;
    double diskSpeed = 0;
    double diskRead = 0;
    double diskWrite = 0;
} sysState;

struct CounterInfo {
    unsigned int counterID;
    char counterIDStr[10];
    char counterType[30];
    char label[15];
    char font[20];
    int r;
    int g;
    int b;
    int fontSize;
    bool noBkg;
    int underline;
};
CounterInfo* editState;

//setup memory and pdh query
MEMORYSTATUSEX memState;
PDH_HQUERY pdhQuery;
PDH_STATUS pdhStatus;
//counters
PDH_HCOUNTER cpuLoadCounter;
PDH_HCOUNTER cpuFreqCounter;
PDH_HCOUNTER diskLoadCounter;
PDH_HCOUNTER diskSpeedCounter;
PDH_HCOUNTER diskReadCounter;
PDH_HCOUNTER diskWriteCounter;
//countervals
PDH_FMT_COUNTERVALUE cpuLoadVal;
PDH_FMT_COUNTERVALUE cpuFreqVal;
PDH_FMT_COUNTERVALUE diskLoadVal;
PDH_FMT_COUNTERVALUE diskSpeedVal;
PDH_FMT_COUNTERVALUE diskReadVal;
PDH_FMT_COUNTERVALUE diskWriteVal;

int CALLBACK WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
    // open or create settings file
    PWSTR appDataPath = NULL;
    SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &appDataPath);
    char* vOut = new char[wcslen(appDataPath) + 1];
    wcstombs_s(NULL, vOut, wcslen(appDataPath) + 1, appDataPath, wcslen(appDataPath) + 1);
    char fileName[] = "\\Resource Monitor\\settings.ini";
    settingsPath = new char[strlen(vOut) + strlen(fileName) + 1];
    strcpy_s(settingsPath, strlen(vOut) + strlen(fileName) + 1, vOut);
    strcat_s(settingsPath, strlen(vOut) + strlen(fileName) + 1, fileName);
    if (CreateFileA(settingsPath,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,
        NULL) == INVALID_HANDLE_VALUE) { // if directory or file does not exist, create it
        char folderName[] = "\\Resource Monitor";
        char* folderPath = new char[strlen(vOut) + strlen(folderName) + 1];
        strcpy_s(folderPath, strlen(vOut) + strlen(folderName) + 1, vOut);
        strcat_s(folderPath, strlen(vOut) + strlen(folderName) + 1, folderName);
        CreateDirectoryA(folderPath, NULL);
        delete[] folderPath;
        CreateFileA(settingsPath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
    }

    // setup memory
    memState.dwLength = sizeof(memState);
    GlobalMemoryStatusEx(&memState);

    // setup pdh query
    pdhStatus = PdhOpenQueryA(NULL, 0, &pdhQuery);
    if (pdhStatus != ERROR_SUCCESS) {
        MessageBoxA(NULL, "pdh Query Creation Failed", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    // add pdh counters
    wchar_t processorTimePath[] = L"\\Processor Information(_Total)\\% Processor Time";
    pdhStatus = PdhAddEnglishCounterW(pdhQuery, processorTimePath, 0, &cpuLoadCounter);
    if (pdhStatus != ERROR_SUCCESS) {
        MessageBoxA(NULL, "processor counter", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    wchar_t freqPath[] = L"\\Processor Information(_Total)\\Processor Frequency";
    pdhStatus = PdhAddEnglishCounterW(pdhQuery, freqPath, 0, &cpuFreqCounter);
    if (pdhStatus != ERROR_SUCCESS) {
        MessageBoxA(NULL, "freq counter", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    wchar_t diskLoadPath[] = L"\\LogicalDisk(_Total)\\% Idle Time";
    pdhStatus = PdhAddEnglishCounterW(pdhQuery, diskLoadPath, 0, &diskLoadCounter);
    if (pdhStatus != ERROR_SUCCESS) {
        MessageBoxA(NULL, "disk load counter", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    wchar_t diskSpeedPath[] = L"\\LogicalDisk(_Total)\\Disk Bytes/sec";
    pdhStatus = PdhAddEnglishCounterW(pdhQuery, diskSpeedPath, 0, &diskSpeedCounter);
    if (pdhStatus != ERROR_SUCCESS) {
        MessageBoxA(NULL, "disk speed counter", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    wchar_t diskReadPath[] = L"\\LogicalDisk(_Total)\\Disk Read Bytes/sec";
    pdhStatus = PdhAddEnglishCounterW(pdhQuery, diskReadPath, 0, &diskReadCounter);
    if (pdhStatus != ERROR_SUCCESS) {
        MessageBoxA(NULL, "disk read counter", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    wchar_t diskWritePath[] = L"\\LogicalDisk(_Total)\\Disk Write Bytes/sec";
    pdhStatus = PdhAddEnglishCounterW(pdhQuery, diskWritePath, 0, &diskWriteCounter);
    if (pdhStatus != ERROR_SUCCESS) {
        MessageBoxA(NULL, "disk write counter", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
        
    WNDCLASSEX mainWin;
    HBRUSH bgBrush = CreateSolidBrush(RGB(1, 1, 1));
    wchar_t mainWinClass[] = L"Main Class";
    mainWin.cbSize = sizeof(WNDCLASSEX);
    mainWin.style = CS_HREDRAW | CS_VREDRAW;
    mainWin.lpfnWndProc = WndProc;
    mainWin.cbClsExtra = 0;
    mainWin.cbWndExtra = 0;
    mainWin.hInstance = hInstance;
    mainWin.hIcon = NULL;
    mainWin.hCursor = LoadCursor(NULL, IDC_ARROW);
    mainWin.hbrBackground = bgBrush;
    mainWin.lpszMenuName = NULL;
    mainWin.lpszClassName = mainWinClass;
    mainWin.hIconSm = NULL;
    if (!RegisterClassEx(&mainWin))
    {
        MessageBoxA(NULL, "Main Window Registration Failed", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    WNDCLASSEX counterWin;
    HBRUSH counterBgBrush = CreateSolidBrush(RGB(1, 1, 1));
    counterWin.cbSize = sizeof(WNDCLASSEX);
    counterWin.style = CS_HREDRAW | CS_VREDRAW;
    counterWin.lpfnWndProc = CounterProc;
    counterWin.cbClsExtra = 0;
    counterWin.cbWndExtra = 0;
    counterWin.hInstance = hInst;
    counterWin.hIcon = NULL;
    counterWin.hCursor = LoadCursor(NULL, IDC_HAND);
    counterWin.hbrBackground = counterBgBrush;
    counterWin.lpszMenuName = NULL;
    counterWin.lpszClassName = counterWinClass;
    counterWin.hIconSm = NULL;
    if (!RegisterClassEx(&counterWin))
    {
        MessageBoxA(NULL, "Counter Window Registration Failed", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
 
    HWND hwnd = CreateWindowEx(
        WS_EX_COMPOSITED | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        mainWinClass,
        NULL,
        WS_VISIBLE | WS_MAXIMIZE,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL,       // Parent window    
        NULL,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
    );
    if (!hwnd)
    {
        MessageBoxA(NULL, "Main Window Creation Failed", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }


    hInst = hInstance;
    parent = hwnd;
    if (GetPrivateProfileIntA("init", "view", 1, settingsPath) == 1) { // depending on settings, set view to either be on or off
        SetLayeredWindowAttributes(hwnd, RGB(1, 1, 1), 255, LWA_ALPHA | LWA_COLORKEY);
        viewToggle = true;
    }
    else {
        SetLayeredWindowAttributes(hwnd, RGB(1, 1, 1), 0, LWA_ALPHA | LWA_COLORKEY);
        viewToggle = false;
    }
    SetWindowLong(hwnd, GWL_STYLE, 0);   // removes all styling including title bar

    ShowWindow(hwnd, SW_SHOWMAXIMIZED);
    UpdateWindow(hwnd);

    // GDI startup
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR           gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // hotkeys
    RegisterHotKey(hwnd, viewHotkey, MOD_CONTROL | MOD_NOREPEAT, 0xC0);
    RegisterHotKey(hwnd, menuHotkey, MOD_CONTROL | MOD_NOREPEAT, 0x45);
    RegisterHotKey(hwnd, quitHotkey, MOD_CONTROL | MOD_NOREPEAT, 0x51);
    RegisterHotKey(hwnd, dialogHotkey, MOD_CONTROL | MOD_NOREPEAT, 0x54);

  
    // add fonts
    char InconsolataPath[] = "Inconsolata.ttf";
    if (AddFontResourceA(InconsolataPath) == 0)
    {
        MessageBox(NULL, L"Font Load Error!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    char TitilliumWebPath[] = "TitilliumWeb.ttf";
    if (AddFontResourceA(TitilliumWebPath) == 0)
    {
        MessageBox(NULL, L"Font Load Error!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    char TajawalPath[] = "Tajawal.ttf";
    if (AddFontResourceA(TajawalPath) == 0)
    {
        MessageBox(NULL, L"Font Load Error!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    char HandleePath[] = "Handlee.ttf";
    if (AddFontResourceA(HandleePath) == 0)
    {
        MessageBox(NULL, L"Font Load Error!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    // get all of the section names in settings.ini
    // create the counters the user previously had open
    numCounters = GetPrivateProfileIntA("init", "NumCounters", 0, settingsPath); // get the number of counters
    char sectionNames[1000];
    GetPrivateProfileSectionNamesA(sectionNames, 1000, settingsPath); // retreive all section names into string sectionNames
    char* counterID = sectionNames; // the first section is [init] so advance past it
    counterID += strlen(counterID) + 1;
    for (int i = 0; i < numCounters; i++) {
        CounterInfo* counterState = new CounterInfo;
        if (counterState == NULL) {
            return 0;
        }
        // add counter type from settings to counterState
        char counterType[30];
        GetPrivateProfileStringA(counterID, "type", "CPULoad", counterType, sizeof(counterType), settingsPath);
        strcpy_s(counterState->counterType, 30, counterType);
        // add the string and int version of the counter ID to counterState
        strcpy_s(counterState->counterIDStr, 10, counterID);
        counterState->counterID = atoi(counterID);
        // add the label from settings to counterState
        char label[15];
        GetPrivateProfileStringA(counterID, "label", "", label, sizeof(label), settingsPath);
        strcat_s(label, 15, "  ");
        strcpy_s(counterState->label, 15, label);
        // add the font from settings to counterState
        char font[20];
        GetPrivateProfileStringA(counterID, "font", "Inconsolata", font, sizeof(font), settingsPath);
        strcpy_s(counterState->font, 20, font);
        // add the font color from settings to counterState
        counterState->r = GetPrivateProfileIntA(counterID, "r", 0, settingsPath);
        counterState->g = GetPrivateProfileIntA(counterID, "g", 0, settingsPath);
        counterState->b = GetPrivateProfileIntA(counterID, "b", 0, settingsPath);
        // add the font size from settings to counterState
        counterState->fontSize = GetPrivateProfileIntA(counterID, "fontSize", 30, settingsPath);
        // add the background status from settings to counterState
        if (GetPrivateProfileIntA(counterID, "noBkg", 1, settingsPath) == 1) {
            counterState->noBkg = true;
        }
        else {
            counterState->noBkg = false;
        }
        // add the underline status from settings to counterState
        counterState->underline = GetPrivateProfileIntA(counterID, "underline", 1, settingsPath);

        int x = GetPrivateProfileIntA(counterID, "x", 300, settingsPath);
        int y = GetPrivateProfileIntA(counterID, "y", 200, settingsPath);
        HWND counterHwnd = CreateWindowEx(
            0,
            counterWinClass,
            NULL,
            WS_CHILD | WS_VISIBLE,
            x, y, 100, 100,
            parent,       // Parent window    
            reinterpret_cast<HMENU>(atoi(counterID)),       // Menu
            hInst,      // Instance handle
            counterState        // Additional application data
        );
        if (!counterHwnd)
        {
            MessageBoxA(NULL, "Counter Window Creation Failed", "Error!", MB_ICONEXCLAMATION | MB_OK);
            return 0;
        }
        ShowWindow(counterHwnd, SW_SHOW);
        UpdateWindow(counterHwnd);


        if (atoi(counterID) >= uniqueCounterID) {
            uniqueCounterID = atoi(counterID) + 1;
        }
        counterID += strlen(counterID) + 1;
    }

    // Main message loop:
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (!IsDialogMessage(dialogHwnd, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return 0;
}

VOID PrintMenu(HDC hdc, HWND hwnd)
{
    //get the client coords
    RECT client;
    GetClientRect(hwnd, &client);
    Graphics g(hdc);
    Pen dashRect(Color(255, 255, 229, 180), 8);
    REAL dashValues[2] = { 5, 5 };
    dashRect.SetDashPattern(dashValues, 2);
    int rectWidth = client.right / 4;
    int rectHeight = client.bottom / 4;
    g.DrawRectangle(&dashRect, rectWidth, rectHeight, rectWidth * 2, rectHeight * 2);

    SolidBrush grayRect(Color(255, 86, 85, 84));
    g.FillRectangle(&grayRect, rectWidth + 10, rectHeight + 10, rectWidth*2 - 20, rectHeight*2 - 20);

    int linesX = client.right / 16;
    int linesY = client.bottom / 16;
    Pen diagLines(Color(255, 255, 229, 180), 20);
    diagLines.SetStartCap(LineCapTriangle);
    diagLines.SetEndCap(LineCapTriangle);
    g.DrawLine(&diagLines, linesX, linesY, linesX*3, linesY*3);
    g.DrawLine(&diagLines, linesX, linesY*15, linesX * 3, linesY * 13);
    g.DrawLine(&diagLines, linesX*13, linesY*3, linesX * 15, linesY);
    g.DrawLine(&diagLines, linesX*13, linesY*13, linesX * 15, linesY * 15);

    SetBkMode(hdc, TRANSPARENT);
    HFONT hFont = CreateFont(48, 0, 0, 0, FW_THIN, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, TEXT("Inconsolata"));
    SelectObject(hdc, hFont);
    SetTextColor(hdc, RGB(255, 229, 180));
    wchar_t menuTxt[] = 
        L"Resource Monitor Edit Menu\n"
        "Ctrl+e .... to open/exit edit menu\n"
        "Ctrl+` .... to toggle view\n"
        "            (` is the key left to 1)\n"
        "Ctrl+t .... to add counters and manage settings\n"
        "Ctrl+q .... to quit resource monitor\n"
        "Left click and drag a counter to move it around\n"
        "Right click a counter to edit it";
    RECT menuBox;
    menuBox.left = rectWidth + 15;
    menuBox.top = rectHeight + 15;
    menuBox.right = menuBox.left + rectWidth * 2 - 30;
    menuBox.bottom = menuBox.top + rectHeight * 2 - 30;
    DrawText(hdc, menuTxt, -1, &menuBox, DT_BOTTOM);
}

BOOL CALLBACK RedrawChildWins(HWND hwndChild, LPARAM lParam) {
    RedrawWindow(hwndChild, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_INTERNALPAINT);
    return true;
}

VOID CALLBACK TimerRoutine(PVOID lpParam, BOOLEAN TimerOrWaitFired) {
    // collect pdh data
    PdhCollectQueryData(pdhQuery);
    Sleep(1000);
    PdhCollectQueryData(pdhQuery);
    GlobalMemoryStatusEx(&memState);
    // move data into counter val
    PdhGetFormattedCounterValue(cpuLoadCounter, PDH_FMT_DOUBLE, NULL, &cpuLoadVal);
    PdhGetFormattedCounterValue(cpuFreqCounter, PDH_FMT_DOUBLE, NULL, &cpuFreqVal);
    PdhGetFormattedCounterValue(diskLoadCounter, PDH_FMT_DOUBLE, NULL, &diskLoadVal);
    PdhGetFormattedCounterValue(diskSpeedCounter, PDH_FMT_LARGE, NULL, &diskSpeedVal);
    PdhGetFormattedCounterValue(diskReadCounter, PDH_FMT_LARGE, NULL, &diskReadVal);
    PdhGetFormattedCounterValue(diskWriteCounter, PDH_FMT_LARGE, NULL, &diskWriteVal);
    // move data from counter val to global struct
    sysState.cpuPercent = cpuLoadVal.doubleValue;
    sysState.cpuFreq = cpuFreqVal.doubleValue/1000;
    sysState.memPercent = memState.dwMemoryLoad;
    unsigned long long int memInUseTemp = (memState.ullTotalPhys - memState.ullAvailPhys) / 1000000;
    sysState.memInUse = (double)memInUseTemp / 1000; // in gb
    sysState.diskPercent = 100 - diskLoadVal.doubleValue;
    unsigned long long int diskSpeedTemp = diskSpeedVal.largeValue/100000;
    sysState.diskSpeed = (double)diskSpeedTemp / 10;
    unsigned long long int diskReadTemp = diskReadVal.largeValue / 100000;
    sysState.diskRead = (double)diskReadTemp / 10;
    unsigned long long int diskWriteTemp = diskWriteVal.largeValue / 100000;
    sysState.diskWrite = (double)diskWriteTemp / 10;

    EnumChildWindows(parent, RedrawChildWins, NULL);   
}

BOOL CALLBACK UpdateSettings(HWND hwndChild, LPARAM lParam) {
    char counterID[10];
    _itoa_s(GetDlgCtrlID(hwndChild), counterID, sizeof(counterID), 10);
    RECT windowRect;
    GetWindowRect(hwndChild, &windowRect);
    char coord[10];
    _itoa_s(windowRect.left, coord, sizeof(coord), 10);
    WritePrivateProfileStringA(counterID, "x", coord, settingsPath);
    _itoa_s(windowRect.top, coord, sizeof(coord), 10);
    WritePrivateProfileStringA(counterID, "y", coord, settingsPath);
    _itoa_s(windowRect.right - windowRect.left, coord, sizeof(coord), 10);
    WritePrivateProfileStringA(counterID, "width", coord, settingsPath);
    _itoa_s(windowRect.bottom - windowRect.top, coord, sizeof(coord), 10);
    WritePrivateProfileStringA(counterID, "height", coord, settingsPath);
    return true;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC hdc;

    switch (message)
    {
    case WM_CREATE:
    {
        HANDLE timer;
        HANDLE timerQueue = CreateTimerQueue();
        if (!CreateTimerQueueTimer(&timer, timerQueue, (WAITORTIMERCALLBACK)TimerRoutine, NULL, 0, 3000, WT_EXECUTEDEFAULT)) {
            MessageBoxA(NULL, "Timer Queue Timer Failed", "Error!", MB_ICONEXCLAMATION | MB_OK);
        }
        return 0;
    }
    case WM_HOTKEY:
    {
        switch (wParam)
        {
        case viewHotkey:
            if (menuToggle) { // if the menu is open, do not toggle view
                break;
            }
            if (viewToggle) {
                SetLayeredWindowAttributes(hwnd, RGB(1, 1, 1), 0, LWA_ALPHA | LWA_COLORKEY);
                viewToggle = false;
            }
            else {
                SetLayeredWindowAttributes(hwnd, RGB(1, 1, 1), 255, LWA_ALPHA | LWA_COLORKEY);
                viewToggle = true;
            }
            break;
        case menuHotkey:
        {
            RECT client;
            GetClientRect(hwnd, &client);
            if (!viewToggle) { // if view is off, turn it on and then draw the menu
                SetLayeredWindowAttributes(hwnd, RGB(1, 1, 1), 255, LWA_ALPHA | LWA_COLORKEY);
                viewToggle = true;
                menuToggle = true;
                SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) & ~WS_EX_TRANSPARENT);
                InvalidateRect(hwnd, &client, true);
                break;
            }
            if (menuToggle) { // closing the menu
                menuToggle = false;
                SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_TRANSPARENT);
                InvalidateRect(hwnd, &client, true);
                EnumChildWindows(hwnd, UpdateSettings, NULL); // update settings on the new locations of the counters
                DestroyWindow(dialogHwnd);
            }
            else {
                menuToggle = true;
                SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) & ~WS_EX_TRANSPARENT);
                InvalidateRect(hwnd, &client, true);
            }
            break;
        }
        case quitHotkey:
            PostMessage(hwnd, WM_DESTROY, 0, 0);
            break;
        case dialogHotkey:
        {
            if (menuToggle) {
                if (!IsWindow(dialogHwnd)) {
                    dialogHwnd = CreateDialog(hInst, MAKEINTRESOURCE(IDD_ADDDIALOG), hwnd, AddDlgProc);
                    ShowWindow(dialogHwnd, SW_SHOW);
                }
            }
            break;
        }
        }
    }
    case WM_PAINT:
        hdc = BeginPaint(hwnd, &ps);
        if (menuToggle) {
            PrintMenu(hdc, hwnd);
        }
        EndPaint(hwnd, &ps);
        break;
    case closeDialogs:
        DestroyWindow(dialogHwnd);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
        break;
    }

    return 0;
}

void createNewCounter(LPCSTR counterType) {
    // create a new counterState
    CounterInfo* counterState = new CounterInfo;
    if (counterState == NULL) {
        return;
    }
    // counterID will be the section name of the new counter in settings
    // add the string and int version of counterID into counterState
    char counterID[10];
    _itoa_s(uniqueCounterID, counterID, sizeof(counterID), 10);
    strcpy_s(counterState->counterIDStr, 10, counterID);
    counterState->counterID = uniqueCounterID;
    // add counter type into counterState and settings
    WritePrivateProfileStringA(counterID, "type", counterType, settingsPath);
    strcpy_s(counterState->counterType, 30, counterType);
    // add label into counterState and settings
    strcpy_s(counterState->label, 15, "  ");
    WritePrivateProfileStringA(counterID, "label", "", settingsPath);
    // add font to counterState and settings
    strcpy_s(counterState->font, 20, "Inconsolata");
    WritePrivateProfileStringA(counterID, "font", "Inconsolata", settingsPath);
    // add font color to counterState and settings
    counterState->r = 0;
    counterState->g = 0;
    counterState->b = 0;
    WritePrivateProfileStringA(counterID, "r", "0", settingsPath);
    WritePrivateProfileStringA(counterID, "g", "0", settingsPath);
    WritePrivateProfileStringA(counterID, "b", "0", settingsPath);
    // add font size to counterState and settings
    counterState->fontSize = 30;
    WritePrivateProfileStringA(counterID, "fontSize", "30", settingsPath);
    // add background transparency to counterState and settings
    counterState->noBkg = false;
    WritePrivateProfileStringA(counterID, "noBkg", "0", settingsPath);
    // add underline to counterState and settings
    counterState->underline = 1;
    WritePrivateProfileStringA(counterID, "underline", "1", settingsPath);
    // add x, y, height, and width to settings
    WritePrivateProfileStringA(counterID, "x", "300", settingsPath);
    WritePrivateProfileStringA(counterID, "y", "200", settingsPath);

    HWND counterHwnd = CreateWindowEx(
        0,
        counterWinClass,
        NULL,
        WS_CHILD | WS_VISIBLE,
        300, 200, 100, 100,
        parent,       // Parent window    
        reinterpret_cast<HMENU>(uniqueCounterID),       // Menu
        hInst,      // Instance handle
        counterState        // Additional application data
    );
    if (!counterHwnd)
    {
        MessageBoxA(NULL, "New Counter Window Creation Failed", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return;
    }
    ShowWindow(counterHwnd, SW_SHOW);
    UpdateWindow(counterHwnd);
    uniqueCounterID++;

    // update [init] in settings
    numCounters++;
    char newNumCounters[10];
    _itoa_s(numCounters, newNumCounters, sizeof(newNumCounters), 10);
    WritePrivateProfileStringA("init", "NumCounters", newNumCounters, settingsPath);
}

BOOL CALLBACK AddDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
    {
        HWND viewCheckbox = GetDlgItem(hwndDlg, IDC_CHECKVIEW);
        if (GetPrivateProfileIntA("init", "view", 1, settingsPath) == 1) { // set checked position of view checkbox depending on settings
            SendMessage(viewCheckbox, BM_SETCHECK, BST_CHECKED, 0);
        }
        else {
            SendMessage(viewCheckbox, BM_SETCHECK, BST_UNCHECKED, 0);
        }
        HWND startupCheckbox = GetDlgItem(hwndDlg, IDC_CHECKSTARTUP);
        if (GetPrivateProfileIntA("init", "startup", 0, settingsPath) == 1) { // set checked position of startup checkbox depending on settings
            SendMessage(startupCheckbox, BM_SETCHECK, BST_CHECKED, 0);
        }
        else {
            SendMessage(startupCheckbox, BM_SETCHECK, BST_UNCHECKED, 0);
        }
        break;
    }
    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDABORT:
        {
            dialogHwnd = NULL;
            DestroyWindow(hwndDlg);
            return true;
        }
        case BTN_CPULOAD:
            createNewCounter("CPULoad");
            return true;
        case BTN_CPUFREQ:
            createNewCounter("CPUFreq");
            return true;
        case BTN_MEMLOAD:
            createNewCounter("MemLoad");
            return true;
        case BTN_MEMINUSE:
            createNewCounter("MemInUse");
            return true;
        case BTN_DISKLOAD:
            createNewCounter("DiskLoad");
            return true;
        case BTN_DISKSPEED:
            createNewCounter("DiskSpeed");
            return true;
        case BTN_DISKREAD:
            createNewCounter("DiskRead");
            return true;
        case BTN_DISKWRITE:
            createNewCounter("DiskWrite");
            return true;
        case IDOK:
        {
            dialogHwnd = NULL;
            DestroyWindow(hwndDlg);
            break;
        }
        }
        if (HIWORD(wParam) == BN_CLICKED) {
            HWND viewCheckbox = GetDlgItem(hwndDlg, IDC_CHECKVIEW);
            if ((HWND)lParam == viewCheckbox) { // if the view on-start is toggled
                if (GetPrivateProfileIntA("init", "view", 1, settingsPath) == 1) {
                    WritePrivateProfileStringA("init", "view", "0", settingsPath);
                }
                else {
                    WritePrivateProfileStringA("init", "view", "1", settingsPath);
                }
            }
            HWND startupCheckbox = GetDlgItem(hwndDlg, IDC_CHECKSTARTUP);
            if ((HWND)lParam == startupCheckbox) {
                if (GetPrivateProfileIntA("init", "startup", 0, settingsPath) == 1) { // if startup is on and user wants to switch it off
                    WritePrivateProfileStringA("init", "startup", "0", settingsPath);
                    HKEY hkey = NULL;
                    RegCreateKey(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", &hkey);
                    TCHAR appPath[] = L"";
                    RegSetValueEx(hkey, L"Resource Monitor", 0, REG_SZ, (BYTE*)appPath, (wcslen(appPath) + 1) * 2);
                }
                else { // user wants to open on startup
                    WritePrivateProfileStringA("init", "startup", "1", settingsPath);
                    HKEY hkey = NULL;
                    RegCreateKey(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", &hkey);
                    TCHAR appPath[500];
                    GetCurrentDirectory(500, appPath);
                    _tcscat_s(appPath, 500, L"\\Resource Monitor");
                    RegSetValueEx(hkey, L"Resource Monitor", 0, REG_SZ, (BYTE*)appPath, (wcslen(appPath) + 1) * 2);
                }
            }
        }
        break;
    }
    case WM_DESTROY:
    {
        dialogHwnd = NULL;
        DestroyWindow(hwndDlg);
        break;
    }
    default:
        return false;
    }
    return true;
}

constexpr unsigned int str2int(const char* str, int h = 0)
{
    return !str[h] ? 5381 : (str2int(str, h + 1) * 33) ^ str[h];
}

void setCurrentEditVals(HWND hwndDlg, TCHAR labels[][15], int numLabels) {
    // add the labels the user can select to the combo box
    TCHAR labelOption[15];
    memset(&labelOption, 0, sizeof(labelOption));
    HWND editCombo = GetDlgItem(hwndDlg, IDC_COMBOLABEL);
    for (int i = 0; i < numLabels; i++) {
        wcscpy_s(labelOption, sizeof(labelOption) / sizeof(TCHAR), (TCHAR*)labels[i]);
        SendMessage(editCombo, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)labelOption);
    }
    switch (str2int(editState->label)) // set the label in the combo box to match the label currently being used
    {
    case str2int("  "):
        SendMessage(editCombo, CB_SETCURSEL, (WPARAM)0, (LPARAM)0);
        break;
    case str2int("CPU  "):
        SendMessage(editCombo, CB_SETCURSEL, (WPARAM)1, (LPARAM)0);
        break;
    case str2int("CPU:  "):
        SendMessage(editCombo, CB_SETCURSEL, (WPARAM)2, (LPARAM)0);
        break;
    case str2int("Mem  "):
        SendMessage(editCombo, CB_SETCURSEL, (WPARAM)1, (LPARAM)0);
        break;
    case str2int("Mem:  "):
        SendMessage(editCombo, CB_SETCURSEL, (WPARAM)2, (LPARAM)0);
        break;
    case str2int("Memory  "):
        SendMessage(editCombo, CB_SETCURSEL, (WPARAM)3, (LPARAM)0);
        break;
    case str2int("Memory:  "):
        SendMessage(editCombo, CB_SETCURSEL, (WPARAM)4, (LPARAM)0);
        break;
    case str2int("Disk  "):
        SendMessage(editCombo, CB_SETCURSEL, (WPARAM)1, (LPARAM)0);
        break;
    case str2int("Disk:  "):
        SendMessage(editCombo, CB_SETCURSEL, (WPARAM)2, (LPARAM)0);
        break;
    case str2int("Read  "):
        SendMessage(editCombo, CB_SETCURSEL, (WPARAM)1, (LPARAM)0);
        break;
    case str2int("Read:  "):
        SendMessage(editCombo, CB_SETCURSEL, (WPARAM)2, (LPARAM)0);
        break;
    case str2int("Write  "):
        SendMessage(editCombo, CB_SETCURSEL, (WPARAM)1, (LPARAM)0);
        break;
    case str2int("Write:  "):
        SendMessage(editCombo, CB_SETCURSEL, (WPARAM)2, (LPARAM)0);
        break;
    }

}

BOOL CALLBACK EditDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
    {
        // add the appropriate label options to the combo box
        switch (str2int(editState->counterType)) // add the appropriate numeric value to numBuffer
        {
        case str2int("CPULoad"):
        {
            SetDlgItemText(hwndDlg, IDC_EDITTYPE, L"CPU Load Counter");
            TCHAR labels[3][15] = { TEXT("(no label)"), TEXT("CPU:"), TEXT("CPU") };
            setCurrentEditVals(hwndDlg, labels, 3);
            break;
        }
        case str2int("CPUFreq"):
        {
            SetDlgItemText(hwndDlg, IDC_EDITTYPE, L"CPU Frequency Counter");
            TCHAR labels[3][15] = { TEXT("(no label)"), TEXT("CPU:"), TEXT("CPU") };
            setCurrentEditVals(hwndDlg, labels, 3);
            break;
        }
        case str2int("MemLoad"):
        {
            SetDlgItemText(hwndDlg, IDC_EDITTYPE, L"Memory Load Counter");
            TCHAR labels[5][15] = { TEXT("(no label)"), TEXT("Memory:"), TEXT("Memory"), TEXT("Mem:"), TEXT("Mem") };
            setCurrentEditVals(hwndDlg, labels, 5);
            break;
        }
        case str2int("MemInUse"):
        {
            SetDlgItemText(hwndDlg, IDC_EDITTYPE, L"Memory In Use Counter");
            TCHAR labels[5][15] = { TEXT("(no label)"), TEXT("Memory:"), TEXT("Memory"), TEXT("Mem:"), TEXT("Mem") };
            setCurrentEditVals(hwndDlg, labels, 5);
            break;
        }
        case str2int("DiskLoad"):
        {
            SetDlgItemText(hwndDlg, IDC_EDITTYPE, L"Disk Load Counter");
            TCHAR labels[3][15] = { TEXT("(no label)"), TEXT("Disk:"), TEXT("Disk") };
            setCurrentEditVals(hwndDlg, labels, 3);
            break;
        }
        case str2int("DiskSpeed"):
        {
            SetDlgItemText(hwndDlg, IDC_EDITTYPE, L"Disk Speed Counter");
            TCHAR labels[3][15] = { TEXT("(no label)"), TEXT("Disk:"), TEXT("Disk") };
            setCurrentEditVals(hwndDlg, labels, 3);
            break;
        }
        case str2int("DiskRead"):
        {
            SetDlgItemText(hwndDlg, IDC_EDITTYPE, L"Disk Read Counter");
            TCHAR labels[3][15] = { TEXT("(no label)"), TEXT("Read:"), TEXT("Read") };
            setCurrentEditVals(hwndDlg, labels, 3);
            break;
        }
        case str2int("DiskWrite"):
        {
            SetDlgItemText(hwndDlg, IDC_EDITTYPE, L"Disk Write Counter");
            TCHAR labels[3][15] = { TEXT("(no label)"), TEXT("Write:"), TEXT("Write") };
            setCurrentEditVals(hwndDlg, labels, 3);
            break;
        }
        }

        // add the available fonts to the combo box
        TCHAR fonts[4][20] = { TEXT("Inconsolata"), TEXT("Handlee"), TEXT("TitilliumWeb"), TEXT("Tajawal") };
        TCHAR labelOption[20];
        memset(&labelOption, 0, sizeof(labelOption));
        HWND fontCombo = GetDlgItem(hwndDlg, IDC_COMBOFONT);
        for (int i = 0; i < 4; i++) {
            wcscpy_s(labelOption, sizeof(labelOption) / sizeof(TCHAR), (TCHAR*)fonts[i]);
            SendMessage(fontCombo, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)labelOption);
        }
        switch (str2int(editState->font)) // set font currently being used
        {
        case str2int("Handlee"):
            SendMessage(fontCombo, CB_SETCURSEL, (WPARAM)0, (LPARAM)0);
            break;
        case str2int("Inconsolata"):
            SendMessage(fontCombo, CB_SETCURSEL, (WPARAM)1, (LPARAM)0);
            break;
        case str2int("Tajawal"):
            SendMessage(fontCombo, CB_SETCURSEL, (WPARAM)2, (LPARAM)0);
            break;
        case str2int("TitilliumWeb"):
            SendMessage(fontCombo, CB_SETCURSEL, (WPARAM)3, (LPARAM)0);
            break;
        }

        // limit the font size the user can select to 3 digits
        HWND fontSizeEdit = GetDlgItem(hwndDlg, IDC_EDITFONTSIZE);
        SendMessage(fontSizeEdit, EM_SETLIMITTEXT, (WPARAM)3, (LPARAM)0);
        char fontSize[5];
        _itoa_s(editState->fontSize, fontSize, sizeof(fontSize), 10);
        SetWindowTextA(fontSizeEdit, fontSize);

        // add the background transparency options to the combo box
        TCHAR bkgOptions[2][20] = { TEXT("Transparent"), TEXT("Opaque") };
        TCHAR bkgOption[20];
        memset(&bkgOption, 0, sizeof(bkgOption));
        HWND bkgCombo = GetDlgItem(hwndDlg, IDC_COMBOBKG);
        for (int i = 0; i < 2; i++) {
            wcscpy_s(bkgOption, sizeof(bkgOption) / sizeof(TCHAR), (TCHAR*)bkgOptions[i]);
            SendMessage(bkgCombo, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)bkgOption);
        }
        if (editState->noBkg) {
            SendMessage(bkgCombo, CB_SETCURSEL, (WPARAM)1, (LPARAM)0);
        }
        else {
            SendMessage(bkgCombo, CB_SETCURSEL, (WPARAM)0, (LPARAM)0);
        }

        // add underline options to combo box
        TCHAR underlineOptions[2][20] = { TEXT("Underline"), TEXT("No Underline") };
        TCHAR underlineOption[20];
        memset(&underlineOption, 0, sizeof(underlineOption));
        HWND underlineCombo = GetDlgItem(hwndDlg, IDC_COMBOUNDERLINE);
        for (int i = 0; i < 2; i++) {
            wcscpy_s(underlineOption, sizeof(underlineOption) / sizeof(TCHAR), (TCHAR*)underlineOptions[i]);
            SendMessage(underlineCombo, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)underlineOption);
        }
        if (editState->underline == 0) {
            SendMessage(underlineCombo, CB_SETCURSEL, (WPARAM)0, (LPARAM)0);
        }
        else {
            SendMessage(underlineCombo, CB_SETCURSEL, (WPARAM)1, (LPARAM)0);
        }
        break;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_COMBOLABEL:
            if (HIWORD(wParam) == CBN_SELENDOK)
            {
                // write the newly selected label into settings
                int ItemIndex = SendMessage((HWND)lParam, (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
                TCHAR selectedLabel[15];
                (TCHAR)SendMessage((HWND)lParam, (UINT)CB_GETLBTEXT, (WPARAM)ItemIndex, (LPARAM)selectedLabel);
                char selectedLabelChar[15];
                wcstombs_s(NULL, selectedLabelChar, 15, selectedLabel, 14);
                if (ItemIndex == 0) { // if the selects no label, write "" to settings
                    WritePrivateProfileStringA(editState->counterIDStr, "label", "", settingsPath);
                }
                else {
                    WritePrivateProfileStringA(editState->counterIDStr, "label", selectedLabelChar, settingsPath);
                }
                // set the label to the selected label
                strcat_s(selectedLabelChar, 15, "  ");
                strcpy_s(editState->label, 15, selectedLabelChar);
            }
            return TRUE;
        case IDC_COMBOFONT:
            if (HIWORD(wParam) == CBN_SELENDOK)
            {
                int ItemIndex = SendMessage((HWND)lParam, (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
                TCHAR selectedFont[20];
                (TCHAR)SendMessage((HWND)lParam, (UINT)CB_GETLBTEXT, (WPARAM)ItemIndex, (LPARAM)selectedFont);
                char selectedFontChar[20];
                wcstombs_s(NULL, selectedFontChar, 20, selectedFont, 19);
                // set the font to the selected font
                strcpy_s(editState->font, 20, selectedFontChar);
                // write the new font to settings
                WritePrivateProfileStringA(editState->counterIDStr, "font", selectedFontChar, settingsPath);
            }
            return TRUE;
        case IDC_BTNCOLOR:
        {
            // open the color selector dialog
            COLORREF currColor = RGB(editState->r, editState->g, editState->b);
            static COLORREF customColors[16];
            CHOOSECOLOR cc;
            memset(&cc, 0, sizeof(cc));
            cc.lStructSize = sizeof(cc);
            cc.hwndOwner = hwndDlg;
            cc.lpCustColors = customColors;
            cc.rgbResult = currColor;
            cc.Flags = CC_FULLOPEN | CC_RGBINIT;
            ChooseColor(&cc);
            // set the current color to the selected color
            editState->r = GetRValue(cc.rgbResult);
            editState->g = GetGValue(cc.rgbResult);
            editState->b = GetBValue(cc.rgbResult);
            EnumChildWindows(parent, RedrawChildWins, NULL);
            // write the current color to settings
            char vOut[5];
            _itoa_s(editState->r, vOut, sizeof(vOut), 10);
            WritePrivateProfileStringA(editState->counterIDStr, "r", vOut, settingsPath);
            _itoa_s(editState->g, vOut, sizeof(vOut), 10);
            WritePrivateProfileStringA(editState->counterIDStr, "g", vOut, settingsPath);
            _itoa_s(editState->b, vOut, sizeof(vOut), 10);
            WritePrivateProfileStringA(editState->counterIDStr, "b", vOut, settingsPath);
            return TRUE;
        }
        case IDC_EDITFONTSIZE:
        {
            if (HIWORD(wParam) == EN_UPDATE)
            {
                char fontSize[5];
                GetDlgItemTextA(hwndDlg, IDC_EDITFONTSIZE, fontSize, 4);
                editState->fontSize = atoi(fontSize);
                WritePrivateProfileStringA(editState->counterIDStr, "fontSize", fontSize, settingsPath);
                EnumChildWindows(parent, RedrawChildWins, NULL);
            }
            return TRUE;
        }
        case IDC_COMBOBKG:
            if (HIWORD(wParam) == CBN_SELENDOK) 
            {
                int ItemIndex = SendMessage((HWND)lParam, (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
                if (ItemIndex == 1) {
                    editState->noBkg = true;
                    WritePrivateProfileStringA(editState->counterIDStr, "noBkg", "1", settingsPath);
                }
                else {
                    editState->noBkg = false;
                    WritePrivateProfileStringA(editState->counterIDStr, "noBkg", "0", settingsPath);
                }
                EnumChildWindows(parent, RedrawChildWins, NULL);
            }
            return TRUE;
        case IDC_COMBOUNDERLINE:
            if (HIWORD(wParam) == CBN_SELENDOK)
            {
                int ItemIndex = SendMessage((HWND)lParam, (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
                if (ItemIndex == 0) {
                    editState->underline = 0;
                    WritePrivateProfileStringA(editState->counterIDStr, "unerline", "0", settingsPath);
                }
                else {
                    editState->underline = 1;
                    WritePrivateProfileStringA(editState->counterIDStr, "unerline", "1", settingsPath);
                }
                EnumChildWindows(parent, RedrawChildWins, NULL);
                return TRUE;
            }
            return TRUE;
        case IDC_BUTTONDEL:
        {
            // erase the entry in settings
            int currNumCounters = GetPrivateProfileIntA("init", "NumCounters", 1, settingsPath);
            currNumCounters--;
            char vOut[10];
            _itoa_s(currNumCounters, vOut, sizeof(vOut), 10);
            WritePrivateProfileStringA("init", "NumCounters", vOut, settingsPath);
            WritePrivateProfileStringA(editState->counterIDStr, NULL, NULL, settingsPath);
            numCounters--;
            SendMessage(editCounter, delCounter, (WPARAM)0, (LPARAM)0);
            editCounter = NULL;
            dialogHwnd = NULL;
            DestroyWindow(hwndDlg);
            return TRUE;
        }
        case IDABORT:
        {
            editCounter = NULL;
            dialogHwnd = NULL;
            DestroyWindow(hwndDlg);
            return true;
        }
        }
    case WM_DESTROY:
    {
        dialogHwnd = NULL;
        DestroyWindow(hwndDlg);
        break;
    }
    default:
        return false;
    }
    return true;
}

LRESULT CALLBACK CounterProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    CounterInfo* counterInfo;
    if (message == WM_CREATE)
    {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        counterInfo = reinterpret_cast<CounterInfo*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)counterInfo);
    }
    else
    {
        LONG_PTR ptr = GetWindowLongPtr(hwnd, GWLP_USERDATA);
        counterInfo = reinterpret_cast<CounterInfo*>(ptr);
    }

    switch (message)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc;
        HFONT hFont;
        hdc = BeginPaint(hwnd, &ps);
        char numBuffer[30];
        switch (str2int(counterInfo->counterType)) // adds the appropriate numeric value to numBuffer
        {
        case str2int("CPULoad"):
        {
            sprintf_s(numBuffer, 30, "%2.0f%%", sysState.cpuPercent);
            break;
        }
        case str2int("CPUFreq"):
        {
            sprintf_s(numBuffer, 30, "%1.1f GHz", sysState.cpuFreq);
            break;
        }
        case str2int("MemLoad"):
        {
            sprintf_s(numBuffer, 30, "%u%%", sysState.memPercent);
            break;
        }
        case str2int("MemInUse"):
        {
            sprintf_s(numBuffer, 30, "%1.1f GB", sysState.memInUse);
            break;
        }
        case str2int("DiskLoad"):
        {
            sprintf_s(numBuffer, 30, "%1.2f%%", sysState.diskPercent);
            break;
        }
        case str2int("DiskSpeed"):
        {
            sprintf_s(numBuffer, 30, "%1.1f MB/s", sysState.diskSpeed);
            break;
        }
        case str2int("DiskRead"):
        {
            sprintf_s(numBuffer, 30, "%1.1f MB/s", sysState.diskRead);
            break;
        }
        case str2int("DiskWrite"):
        {
            sprintf_s(numBuffer, 30, "%1.1f MB/s", sysState.diskWrite);
            break;
        }
        }
        char printBuffer[40];
        strcpy_s(printBuffer, 40, counterInfo->label);
        strcat_s(printBuffer, 40, numBuffer);

        // set font, font color, and background transparency
        SetTextColor(hdc, RGB(counterInfo->r, counterInfo->g, counterInfo->b));
        hFont = CreateFontA(counterInfo->fontSize, 0, 0, 0, FW_DONTCARE, FALSE, counterInfo->underline, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
            CLIP_STROKE_PRECIS, PROOF_QUALITY, VARIABLE_PITCH, counterInfo->font);
        SelectObject(hdc, hFont);
        if (counterInfo->noBkg) {
            SetBkMode(hdc, TRANSPARENT);
        }
        else {
            SetBkMode(hdc, OPAQUE);
        }

        // change window size to match textbox size
        SIZE textSize;
        GetTextExtentPointA(hdc, printBuffer, strlen(printBuffer), &textSize);
        RECT currWin;
        GetWindowRect(hwnd, &currWin);
        MoveWindow(hwnd, currWin.left, currWin.top, textSize.cx, textSize.cy, FALSE);

        SetBkColor(hdc, RGB(0, 0, 0));
        TextOutA(hdc, 0, 0, printBuffer, strlen(printBuffer));
        EndPaint(hwnd, &ps);
        break;
    }
    case WM_RBUTTONDOWN:
    {
        SendMessage(GetParent(hwnd), closeDialogs, NULL, NULL); // close the add counters dialog
        dialogHwnd = NULL;
        if (!IsWindow(dialogHwnd)) {
            editState = counterInfo;
            editCounter = hwnd;
            dialogHwnd = CreateDialog(hInst, MAKEINTRESOURCE(IDD_EDITDIALOG), hwnd, EditDlgProc);
            ShowWindow(dialogHwnd, SW_SHOW);
        }
        break;
    }
    case WM_LBUTTONDOWN:
        ReleaseCapture();
        SendMessage(hwnd, WM_SYSCOMMAND, 0xf012, 0);
        break;
    case delCounter:
        DestroyWindow(hwnd);
        break;
    case WM_DESTROY:
        editCounter = NULL;
        return 0;
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
        break;
    }

    return 0;
}