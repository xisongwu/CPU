#include <cstdio>
#include <cstring>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <commdlg.h>
#include <dbt.h>
#include <gdiplus.h>
#include <comdef.h>
#include <wbemidl.h>
#include <malloc.h>
#elif defined(__linux__)
#include <gtk/gtk.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <unistd.h>
#include <sys/sysinfo.h>
#elif defined(__APPLE__)
#include <gtk/gtk.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <unistd.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/processor_info.h>
#include <mach/mach_host.h>
#endif

enum MetricType {
    METRIC_CPU = 0,
    METRIC_MEMORY,
    METRIC_GPU,
    METRIC_DISK_ALL,
    METRIC_SWAP,
    METRIC_CPU_TEMP,
    METRIC_GPU_TEMP,
    METRIC_DISK_DYNAMIC,
    METRIC_COUNT = METRIC_DISK_DYNAMIC + 26
};

struct DiskInfo {
    char letter;
    char name[32];
    char shortName[8];
};

static int g_diskCount = 0;
static DiskInfo g_disks[26];

int GetRealMetricCount() {
    return METRIC_DISK_DYNAMIC + g_diskCount;
}

MetricType MakeDiskMetric(int diskIndex) {
    return (MetricType)(METRIC_DISK_DYNAMIC + diskIndex);
}

bool IsDiskMetric(MetricType type) {
    return type >= METRIC_DISK_DYNAMIC && type < METRIC_DISK_DYNAMIC + g_diskCount;
}

int GetDiskIndex(MetricType type) {
    if (IsDiskMetric(type)) return type - METRIC_DISK_DYNAMIC;
    return -1;
}

enum ThemeType {
    THEME_DARK = 0,
    THEME_LIGHT,
    THEME_BLUE,
    THEME_PURPLE,
    THEME_COUNT
};

struct ThemeColors {
    double bgR, bgG, bgB;
    double bgHoverR, bgHoverG, bgHoverB;
    double borderR, borderG, borderB;
    double borderHoverR, borderHoverG, borderHoverB;
    double labelR, labelG, labelB;
    double trackR, trackG, trackB;
    double trackBorderR, trackBorderG, trackBorderB;
};

const ThemeColors THEMES[] = {
    {28/255.0,28/255.0,36/255.0, 40/255.0,40/255.0,48/255.0, 55/255.0,55/255.0,70/255.0, 80/255.0,80/255.0,100/255.0, 160/255.0,160/255.0,180/255.0, 50/255.0,50/255.0,60/255.0, 60/255.0,60/255.0,60/255.0},
    {245/255.0,245/255.0,250/255.0, 235/255.0,235/255.0,240/255.0, 200/255.0,200/255.0,210/255.0, 170/255.0,170/255.0,190/255.0, 80/255.0,80/255.0,100/255.0, 220/255.0,220/255.0,230/255.0, 190/255.0,190/255.0,200/255.0},
    {18/255.0,30/255.0,50/255.0, 25/255.0,40/255.0,65/255.0, 40/255.0,70/255.0,110/255.0, 60/255.0,100/255.0,150/255.0, 130/255.0,170/255.0,220/255.0, 30/255.0,50/255.0,80/255.0, 40/255.0,60/255.0,90/255.0},
    {35/255.0,20/255.0,45/255.0, 48/255.0,30/255.0,60/255.0, 70/255.0,45/255.0,90/255.0, 100/255.0,65/255.0,130/255.0, 180/255.0,140/255.0,210/255.0, 55/255.0,35/255.0,70/255.0, 65/255.0,45/255.0,80/255.0},
};

const char* BASE_METRIC_NAMES[] = {
    "CPU 使用率",
    "内存使用率",
    "GPU 使用率",
    "所有磁盘",
    "交换区使用率",
    "CPU 温度",
    "GPU 温度"
};

const char* BASE_METRIC_SHORT[] = {
    "CPU",
    "MEM",
    "GPU",
    "DISK",
    "SWAP",
    "CPU Temp",
    "GPU Temp"
};

const char* GetMetricName(MetricType type) {
    if (type < METRIC_DISK_DYNAMIC) return BASE_METRIC_NAMES[type];
    int idx = GetDiskIndex(type);
    if (idx >= 0 && idx < g_diskCount) return g_disks[idx].name;
    return "???";
}

const char* GetMetricShort(MetricType type) {
    if (type < METRIC_DISK_DYNAMIC) return BASE_METRIC_SHORT[type];
    int idx = GetDiskIndex(type);
    if (idx >= 0 && idx < g_diskCount) return g_disks[idx].shortName;
    return "?";
}

const char* THEME_NAMES[] = {
    "深色",
    "浅色",
    "蓝色",
    "紫色"
};

struct Settings {
    MetricType slots[4];
    int numSlots;
    ThemeType theme;
    char bgPath[512];
};

struct MonitorData {
    double cpuUsage;
    double memoryUsage;
    double gpuUsage;
    double diskUsage;
    double swapUsage;
};

static Settings g_settings;
static MonitorData g_monitorData = {0};
static double g_diskValues[26] = {0};

#ifdef _WIN32
static char g_cpuName[128] = "";
static char g_gpuName[128] = "";
static double g_cpuTemp = -1;  // -1 = not available
static double g_gpuTemp = -1;
static double g_cpuHistory[120] = {0};
static int g_cpuHistoryIdx = 0;
static int g_cpuHistoryLen = 0;
#endif

void InitSettings() {
    g_settings.numSlots = 2;
    g_settings.theme = THEME_DARK;
    g_settings.slots[0] = METRIC_CPU;
    g_settings.slots[1] = METRIC_MEMORY;
    g_settings.slots[2] = METRIC_GPU;
    g_settings.slots[3] = METRIC_DISK_ALL;
    g_settings.bgPath[0] = '\0';
}

#ifdef _WIN32
void DetectDisks() {
    g_diskCount = 0;
    wchar_t drives[256];
    GetLogicalDriveStringsW(256, drives);
    wchar_t* p = drives;
    while (*p) {
        if (GetDriveTypeW(p) == DRIVE_FIXED || GetDriveTypeW(p) == DRIVE_REMOVABLE || GetDriveTypeW(p) == DRIVE_REMOTE) {
            char letter = (char)towupper(p[0]);
            g_disks[g_diskCount].letter = letter;
            sprintf(g_disks[g_diskCount].name, "磁盘 %c:", letter);
            sprintf(g_disks[g_diskCount].shortName, "%c:", letter);
            g_diskCount++;
        }
        p += wcslen(p) + 1;
    }
}
#else
void DetectDisks() {
    g_diskCount = 0;
}
#endif

double GetMetricValue(const MonitorData& data, MetricType type) {
    switch (type) {
    case METRIC_CPU: return data.cpuUsage;
    case METRIC_MEMORY: return data.memoryUsage;
    case METRIC_GPU: return data.gpuUsage;
    case METRIC_DISK_ALL: return data.diskUsage;
    case METRIC_SWAP: return data.swapUsage;
#ifdef _WIN32
    case METRIC_CPU_TEMP: return (g_cpuTemp >= 0) ? g_cpuTemp : 0;
    case METRIC_GPU_TEMP: return (g_gpuTemp >= 0) ? g_gpuTemp : 0;
#endif
    default:
        if (IsDiskMetric(type)) {
            int idx = GetDiskIndex(type);
            if (idx >= 0 && idx < g_diskCount) return g_diskValues[idx];
        }
        return 0;
    }
}

void GetUsageColor(double value, ThemeType theme, double &r, double &g, double &b) {
    if (theme == THEME_LIGHT) {
        if (value < 60) { r=46/255.0; g=125/255.0; b=50/255.0; }
        else if (value < 85) { r=230/255.0; g=160/255.0; b=0; }
        else { r=211/255.0; g=47/255.0; b=47/255.0; }
    } else {
        if (value < 60) { r=76/255.0; g=175/255.0; b=80/255.0; }
        else if (value < 85) { r=255/255.0; g=193/255.0; b=7/255.0; }
        else { r=244/255.0; g=67/255.0; b=54/255.0; }
    }
}

#ifdef _WIN32
void GetUsageColorForMetric(double value, MetricType metric, ThemeType theme, double &r, double &g, double &b) {
    if (metric == METRIC_CPU_TEMP || metric == METRIC_GPU_TEMP) {
        // Temperature: 0-40 green, 40-70 yellow, 70-100+ red
        if (theme == THEME_LIGHT) {
            if (value < 40) { r=46/255.0; g=125/255.0; b=50/255.0; }
            else if (value < 70) { r=230/255.0; g=160/255.0; b=0; }
            else { r=211/255.0; g=47/255.0; b=47/255.0; }
        } else {
            if (value < 40) { r=76/255.0; g=175/255.0; b=80/255.0; }
            else if (value < 70) { r=255/255.0; g=193/255.0; b=7/255.0; }
            else { r=244/255.0; g=67/255.0; b=54/255.0; }
        }
    } else {
        GetUsageColor(value, theme, r, g, b);
    }
}
#endif

// ============================================================
// Platform: Windows
// ============================================================
#ifdef _WIN32

static HINSTANCE g_hInstance = NULL;
static HBITMAP g_bgBitmap = NULL;
static HFONT g_uiFont = NULL;

#define TIMER_ID 1
#define ID_SHOW 1001
#define ID_HIDE 1002
#define ID_CLOSE 1003
#define ID_SETTINGS 1004
#define ID_APPLY 1005
#define ID_COMBO1 2001
#define ID_COMBO2 2002
#define ID_COMBO3 2003
#define ID_COMBO4 2004
#define ID_THEME_COMBO 2010
#define ID_SLOTS_COMBO 2011
#define ID_BG_BUTTON 2012
#define ID_BG_CLEAR 2013
#define WM_TRAYMESSAGE (WM_USER + 100)

static HFONT GetSystemUIFont(int sizeDiff = 0, bool bold = false) {
    LOGFONTW lf = {0};
    lf.lfHeight = -13 + sizeDiff;
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    wcscpy(lf.lfFaceName, L"Microsoft YaHei UI");
    return CreateFontIndirectW(&lf);
}

COLORREF GetUsageColorWin(double value, ThemeType theme) {
    double r, g, b;
    GetUsageColor(value, theme, r, g, b);
    return RGB((int)(r*255), (int)(g*255), (int)(b*255));
}

COLORREF GetUsageColorWinForMetric(double value, MetricType metric, ThemeType theme) {
    double r, g, b;
    GetUsageColorForMetric(value, metric, theme, r, g, b);
    return RGB((int)(r*255), (int)(g*255), (int)(b*255));
}

// Forward declaration
void DrawRoundedRect(HDC hdc, RECT rect, int radius, COLORREF fillColor, COLORREF borderColor, int borderWidth);

void QueryCPUName() {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(g_cpuName);
        RegQueryValueExA(hKey, "ProcessorNameString", NULL, NULL, (LPBYTE)g_cpuName, &size);
        RegCloseKey(hKey);
    }
    // 去除前后空格
    char* p = g_cpuName;
    while (*p == ' ') p++;
    if (p != g_cpuName) memmove(g_cpuName, p, strlen(p) + 1);
    int len = strlen(g_cpuName);
    while (len > 0 && g_cpuName[len-1] == ' ') g_cpuName[--len] = '\0';
}

void QueryGPUName() {
    DISPLAY_DEVICEA dd;
    dd.cb = sizeof(DISPLAY_DEVICEA);
    g_gpuName[0] = '\0';
    for (DWORD i = 0; EnumDisplayDevicesA(NULL, i, &dd, 0); i++) {
        if (dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) {
            // 优先选择独显（NVIDIA/AMD），否则选集显
            if (g_gpuName[0] == '\0') {
                strncpy(g_gpuName, dd.DeviceString, sizeof(g_gpuName) - 1);
            }
            if (strstr(dd.DeviceString, "NVIDIA") || strstr(dd.DeviceString, "AMD") ||
                strstr(dd.DeviceString, "Radeon") || strstr(dd.DeviceString, "GeForce")) {
                strncpy(g_gpuName, dd.DeviceString, sizeof(g_gpuName) - 1);
                break;
            }
        }
    }
}

static void* __cdecl ADL_Main_Memory_AllocCb(int size) {
    return malloc(size);
}

void QueryTemperatures() {
    // Reset temps each query cycle - sensors may come and go
    g_cpuTemp = -1;
    g_gpuTemp = -1;

    // CPU Temperature - try PDH Thermal Zone counter first (no admin needed)
    static HQUERY tempPdhQuery = NULL;
    static HCOUNTER cpuTempCounter = NULL;
    static bool tempPdhInited = false;

    if (!tempPdhInited) {
        tempPdhInited = true;
        if (PdhOpenQuery(NULL, 0, &tempPdhQuery) == ERROR_SUCCESS) {
            if (PdhAddCounterA(tempPdhQuery, "\\Thermal Zone Information(*)\\Temperature", 0, &cpuTempCounter) != ERROR_SUCCESS)
                cpuTempCounter = NULL;
        }
    }

    if (cpuTempCounter) {
        PdhCollectQueryData(tempPdhQuery);
        DWORD bufSize = 0;
        DWORD itemCount = 0;
        PDH_STATUS pdhStatus = PdhGetFormattedCounterArray(cpuTempCounter, PDH_FMT_DOUBLE, &bufSize, &itemCount, NULL);
        if (pdhStatus == PDH_MORE_DATA || pdhStatus == ERROR_SUCCESS) {
            PPDH_FMT_COUNTERVALUE_ITEM items = (PPDH_FMT_COUNTERVALUE_ITEM)malloc(bufSize);
            if (items) {
                pdhStatus = PdhGetFormattedCounterArray(cpuTempCounter, PDH_FMT_DOUBLE, &bufSize, &itemCount, items);
                if (pdhStatus == ERROR_SUCCESS) {
                    for (DWORD i = 0; i < itemCount; i++) {
                        double val = items[i].FmtValue.doubleValue;
                        double temp = val - 273.15;
                        if (temp > 0 && temp < 150) {
                            if (g_cpuTemp < 0 || temp > g_cpuTemp)
                                g_cpuTemp = temp;
                        }
                    }
                }
                free(items);
            }
        }
    }

    // CPU Temperature fallback - try WMI MSAcpi_ThermalZoneTemperature
    if (g_cpuTemp < 0) {
        static bool comInitForWmi = false;
        if (!comInitForWmi) {
            CoInitialize(NULL);
            comInitForWmi = true;
        }
        IWbemLocator* pLoc = NULL;
        HRESULT hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);
        if (SUCCEEDED(hr)) {
            IWbemServices* pSvc = NULL;
            hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\WMI"), NULL, NULL, 0, 0, NULL, 0, &pSvc);
            if (SUCCEEDED(hr)) {
                CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
                IEnumWbemClassObject* pEnumerator = NULL;
                hr = pSvc->ExecQuery(_bstr_t("WQL"), _bstr_t("SELECT * FROM MSAcpi_ThermalZoneTemperature"),
                    WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);
                if (SUCCEEDED(hr) && pEnumerator) {
                    IWbemClassObject* pclsObj = NULL;
                    ULONG uReturn = 0;
                    while (pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn) == S_OK && uReturn > 0) {
                        VARIANT vtProp;
                        VariantInit(&vtProp);
                        hr = pclsObj->Get(L"CurrentTemperature", 0, &vtProp, 0, 0);
                        if (SUCCEEDED(hr) && vtProp.vt == VT_I4) {
                            double temp = vtProp.lVal / 10.0 - 273.15;
                            if (temp > 0 && temp < 150) {
                                if (g_cpuTemp < 0 || temp > g_cpuTemp)
                                    g_cpuTemp = temp;
                            }
                        }
                        VariantClear(&vtProp);
                        pclsObj->Release();
                    }
                    pEnumerator->Release();
                }
                pSvc->Release();
            }
            pLoc->Release();
        }
    }

    // CPU Temperature fallback 2 - try WMI Win32_TemperatureProbe
    if (g_cpuTemp < 0) {
        IWbemLocator* pLoc = NULL;
        HRESULT hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);
        if (SUCCEEDED(hr)) {
            IWbemServices* pSvc = NULL;
            hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, 0, NULL, 0, &pSvc);
            if (SUCCEEDED(hr)) {
                CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
                IEnumWbemClassObject* pEnumerator = NULL;
                hr = pSvc->ExecQuery(_bstr_t("WQL"), _bstr_t("SELECT * FROM Win32_TemperatureProbe"),
                    WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);
                if (SUCCEEDED(hr) && pEnumerator) {
                    IWbemClassObject* pclsObj = NULL;
                    ULONG uReturn = 0;
                    while (pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn) == S_OK && uReturn > 0) {
                        VARIANT vtProp;
                        VariantInit(&vtProp);
                        hr = pclsObj->Get(L"CurrentReading", 0, &vtProp, 0, 0);
                        if (SUCCEEDED(hr) && (vtProp.vt == VT_I4 || vtProp.vt == VT_UI4)) {
                            double temp = vtProp.lVal / 10.0; // tenths of Kelvin
                            temp = temp - 273.15;
                            if (temp > 0 && temp < 150) {
                                if (g_cpuTemp < 0 || temp > g_cpuTemp)
                                    g_cpuTemp = temp;
                            }
                        }
                        VariantClear(&vtProp);
                        pclsObj->Release();
                    }
                    pEnumerator->Release();
                }
                pSvc->Release();
            }
            pLoc->Release();
        }
    }

    // GPU Temperature - try NVML (NVIDIA)
    static HMODULE hNvml = NULL;
    static bool nvmlTried = false;
    if (!nvmlTried) {
        hNvml = LoadLibraryA("nvml.dll");
        nvmlTried = true;
    }
    if (hNvml) {
        typedef int (*nvmlInit_t)(void);
        typedef int (*nvmlDeviceGetCount_t)(unsigned int*);
        typedef int (*nvmlDeviceGetHandleByIndex_t)(unsigned int, void**);
        typedef int (*nvmlDeviceGetTemperature_t)(void*, int, unsigned int*);

        static nvmlInit_t pNvmlInit = (nvmlInit_t)GetProcAddress(hNvml, "nvmlInit_v2");
        static nvmlDeviceGetCount_t pNvmlDeviceGetCount = (nvmlDeviceGetCount_t)GetProcAddress(hNvml, "nvmlDeviceGetCount_v2");
        static nvmlDeviceGetHandleByIndex_t pNvmlDeviceGetHandleByIndex = (nvmlDeviceGetHandleByIndex_t)GetProcAddress(hNvml, "nvmlDeviceGetHandleByIndex_v2");
        static nvmlDeviceGetTemperature_t pNvmlDeviceGetTemperature = (nvmlDeviceGetTemperature_t)GetProcAddress(hNvml, "nvmlDeviceGetTemperature");

        if (pNvmlInit && pNvmlDeviceGetCount && pNvmlDeviceGetHandleByIndex && pNvmlDeviceGetTemperature) {
            static bool nvmlInited = false;
            if (!nvmlInited) {
                if (pNvmlInit() == 0) nvmlInited = true;
            }
            if (nvmlInited) {
                unsigned int count = 0;
                if (pNvmlDeviceGetCount(&count) == 0 && count > 0) {
                    void* handle = NULL;
                    if (pNvmlDeviceGetHandleByIndex(0, &handle) == 0) {
                        unsigned int temp = 0;
                        if (pNvmlDeviceGetTemperature(handle, 0, &temp) == 0) {
                            g_gpuTemp = (double)temp;
                        }
                    }
                }
            }
        }
    }

    // GPU Temperature - try ADL (AMD)
    static HMODULE hAdl = NULL;
    static bool adlTried = false;
    if (!adlTried) {
        hAdl = LoadLibraryA("atiadlxx.dll");
        if (!hAdl) hAdl = LoadLibraryA("atiadlxy.dll");
        adlTried = true;
    }
    if (hAdl) {
        typedef int (*ADL_Main_Control_Create_t)(void* (*)(int), int);
        typedef int (*ADL_Adapter_NumberOfAdapters_Get_t)(int*);
        typedef int (*ADL_Adapter_AdapterInfo_Get_t)(void*, int);
        typedef int (*ADL_Overdrive5_Temperature_Get_t)(int, int, void*);
        typedef int (*ADL2_Overdrive6_Temperature_Get_t)(int, int, void*);

        static ADL_Main_Control_Create_t pADL_Main_Control_Create = (ADL_Main_Control_Create_t)GetProcAddress(hAdl, "ADL_Main_Control_Create");
        static ADL_Adapter_NumberOfAdapters_Get_t pADL_Adapter_NumberOfAdapters_Get = (ADL_Adapter_NumberOfAdapters_Get_t)GetProcAddress(hAdl, "ADL_Adapter_NumberOfAdapters_Get");
        static ADL_Adapter_AdapterInfo_Get_t pADL_Adapter_AdapterInfo_Get = (ADL_Adapter_AdapterInfo_Get_t)GetProcAddress(hAdl, "ADL_Adapter_AdapterInfo_Get");
        static ADL_Overdrive5_Temperature_Get_t pADL_Overdrive5_Temperature_Get = (ADL_Overdrive5_Temperature_Get_t)GetProcAddress(hAdl, "ADL_Overdrive5_Temperature_Get");

        if (pADL_Main_Control_Create && pADL_Adapter_NumberOfAdapters_Get && pADL_Adapter_AdapterInfo_Get) {
            static bool adlInited = false;
            if (!adlInited) {
                if (pADL_Main_Control_Create(ADL_Main_Memory_AllocCb, 1) == 0)
                    adlInited = true;
            }
            if (adlInited) {
                int numAdapters = 0;
                if (pADL_Adapter_NumberOfAdapters_Get(&numAdapters) == 0 && numAdapters > 0) {
                    int infoSize = numAdapters * 1024;
                    void* adapterInfo = malloc(infoSize);
                    if (adapterInfo) {
                        memset(adapterInfo, 0, infoSize);
                        if (pADL_Adapter_AdapterInfo_Get(adapterInfo, infoSize) == 0) {
                            for (int i = 0; i < numAdapters; i++) {
                                int adapterIndex = *((int*)((char*)adapterInfo + i * 1024 + 4));
                                int adlTemp[2] = { 8, 0 };
                                if (pADL_Overdrive5_Temperature_Get) {
                                    if (pADL_Overdrive5_Temperature_Get(adapterIndex, 0, adlTemp) == 0) {
                                        double temp = adlTemp[1] / 1000.0;
                                        if (temp > 0 && temp < 150) {
                                            if (g_gpuTemp < 0 || temp > g_gpuTemp)
                                                g_gpuTemp = temp;
                                        }
                                    }
                                }
                                if (g_gpuTemp < 0) {
                                    static ADL2_Overdrive6_Temperature_Get_t pADL2_OD6_Temp = NULL;
                                    if (!pADL2_OD6_Temp) {
                                        pADL2_OD6_Temp = (ADL2_Overdrive6_Temperature_Get_t)GetProcAddress(hAdl, "ADL2_Overdrive6_Temperature_Get");
                                    }
                                    if (pADL2_OD6_Temp) {
                                        int od6Temp[2] = { 8, 0 };
                                        if (pADL2_OD6_Temp(adapterIndex, 0, od6Temp) == 0) {
                                            double temp = od6Temp[1] / 1000.0;
                                            if (temp > 0 && temp < 150) {
                                                if (g_gpuTemp < 0 || temp > g_gpuTemp)
                                                    g_gpuTemp = temp;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        free(adapterInfo);
                    }
                }
            }
        }
    }
}

void DrawCurveGraph(HDC hdc, int x, int y, int width, int height, ThemeType theme) {
    const ThemeColors& tc = THEMES[theme];
    if (g_cpuHistoryLen < 2) return;

    // 获取当前CPU值对应的颜色
    double curValue = g_cpuHistory[(g_cpuHistoryIdx - 1 + 120) % 120];
    double cr, cg, cb;
    GetUsageColor(curValue, theme, cr, cg, cb);
    COLORREF lineColor = RGB((int)(cr*255), (int)(cg*255), (int)(cb*255));
    COLORREF glowColor = RGB((int)(cr*255*0.5), (int)(cg*255*0.5), (int)(cb*255*0.5));

    // 计算所有点坐标
    int startIdx = (g_cpuHistoryIdx - g_cpuHistoryLen + 120) % 120;
    int* ptsX = (int*)malloc(g_cpuHistoryLen * sizeof(int));
    int* ptsY = (int*)malloc(g_cpuHistoryLen * sizeof(int));
    for (int i = 0; i < g_cpuHistoryLen; i++) {
        int idx = (startIdx + i) % 120;
        ptsX[i] = x + (int)((double)i / (119.0) * width);
        ptsY[i] = y + height - (int)(height * g_cpuHistory[idx] / 100.0);
        if (ptsY[i] < y) ptsY[i] = y;
        if (ptsY[i] > y + height) ptsY[i] = y + height;
    }

    // 绘制渐变填充区域
    HDC fillDC = CreateCompatibleDC(hdc);
    HBITMAP fillBmp = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldFillBmp = (HBITMAP)SelectObject(fillDC, fillBmp);

    // 清空为透明黑色
    RECT clearRect = {0, 0, width, height};
    FillRect(fillDC, &clearRect, (HBRUSH)GetStockObject(BLACK_BRUSH));

    // 创建渐变填充 - 从线条颜色渐变到透明
    TRIVERTEX vert[2];
    GRADIENT_RECT gRect;
    vert[0].x = 0; vert[0].y = 0;
    vert[0].Red = (WORD)(cr * 65535 * 0.25);
    vert[0].Green = (WORD)(cg * 65535 * 0.25);
    vert[0].Blue = (WORD)(cb * 65535 * 0.25);
    vert[0].Alpha = 0;
    vert[1].x = width; vert[1].y = height;
    vert[1].Red = 0; vert[1].Green = 0; vert[1].Blue = 0; vert[1].Alpha = 0;
    gRect.UpperLeft = 0; gRect.LowerRight = 1;

    // 绘制填充多边形（从曲线到底部）
    POINT* fillPts = (POINT*)malloc((g_cpuHistoryLen + 2) * sizeof(POINT));
    for (int i = 0; i < g_cpuHistoryLen; i++) {
        fillPts[i].x = ptsX[i] - x;
        fillPts[i].y = ptsY[i] - y;
    }
    fillPts[g_cpuHistoryLen].x = ptsX[g_cpuHistoryLen - 1] - x;
    fillPts[g_cpuHistoryLen].y = height;
    fillPts[g_cpuHistoryLen + 1].x = ptsX[0] - x;
    fillPts[g_cpuHistoryLen + 1].y = height;

    HRGN fillRgn = CreatePolygonRgn(fillPts, g_cpuHistoryLen + 2, ALTERNATE);
    SelectClipRgn(fillDC, fillRgn);
    GradientFill(fillDC, vert, 2, &gRect, 1, GRADIENT_FILL_RECT_V);
    SelectClipRgn(fillDC, NULL);
    DeleteObject(fillRgn);

    // AlphaBlend 填充区域到主DC
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, 80, 0};
    AlphaBlend(hdc, x, y, width, height, fillDC, 0, 0, width, height, bf);

    SelectObject(fillDC, oldFillBmp);
    DeleteObject(fillBmp);
    DeleteDC(fillDC);
    free(fillPts);

    // 绘制发光效果（外层粗线）
    for (int glow = 3; glow >= 1; glow--) {
        HPEN glowPen = CreatePen(PS_SOLID, glow * 2 + 1, glowColor);
        HPEN oldGlowPen = (HPEN)SelectObject(hdc, glowPen);
        bool first = true;
        for (int i = 0; i < g_cpuHistoryLen; i++) {
            if (first) { MoveToEx(hdc, ptsX[i], ptsY[i], NULL); first = false; }
            else LineTo(hdc, ptsX[i], ptsY[i]);
        }
        SelectObject(hdc, oldGlowPen);
        DeleteObject(glowPen);
    }

    // 绘制主曲线线条（抗锯齿效果通过多层细线模拟）
    for (int w = 2; w >= 1; w--) {
        COLORREF c = (w == 2) ? RGB((int)(cr*255*0.7+80), (int)(cg*255*0.7+80), (int)(cb*255*0.7+80)) : lineColor;
        HPEN linePen = CreatePen(PS_SOLID, w, c);
        HPEN oldLinePen = (HPEN)SelectObject(hdc, linePen);
        bool first = true;
        for (int i = 0; i < g_cpuHistoryLen; i++) {
            if (first) { MoveToEx(hdc, ptsX[i], ptsY[i], NULL); first = false; }
            else LineTo(hdc, ptsX[i], ptsY[i]);
        }
        SelectObject(hdc, oldLinePen);
        DeleteObject(linePen);
    }

    // 绘制当前值高亮小点（仅2px，不遮挡折线）
    int lastX = ptsX[g_cpuHistoryLen - 1];
    int lastY = ptsY[g_cpuHistoryLen - 1];
    HBRUSH dotBrush = CreateSolidBrush(lineColor);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, dotBrush);
    Ellipse(hdc, lastX - 1, lastY - 1, lastX + 1, lastY + 1);
    SelectObject(hdc, oldBrush);
    DeleteObject(dotBrush);

    // 绘制Y轴刻度标签（0%, 50%, 100%）
    HFONT smallFont = GetSystemUIFont(-4, false);
    HFONT oldFont = (HFONT)SelectObject(hdc, smallFont);
    SetTextColor(hdc, RGB((int)(tc.labelR*255*0.5), (int)(tc.labelG*255*0.5), (int)(tc.labelB*255*0.5)));
    SetBkMode(hdc, TRANSPARENT);
    const char* labels[] = {"100", "50", "0"};
    int labelY[] = {y, y + height / 2, y + height - 8};
    for (int i = 0; i < 3; i++) {
        RECT r = {x - 22, labelY[i] - 6, x - 2, labelY[i] + 6};
        DrawTextA(hdc, labels[i], -1, &r, DT_RIGHT | DT_VCENTER);
    }
    SelectObject(hdc, oldFont);
    DeleteObject(smallFont);

    free(ptsX);
    free(ptsY);
}

void DrawRoundedRect(HDC hdc, RECT rect, int radius, COLORREF fillColor, COLORREF borderColor, int borderWidth) {
    HBRUSH fillBrush = CreateSolidBrush(fillColor);
    HPEN borderPen = CreatePen(PS_SOLID, borderWidth, borderColor);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, fillBrush);
    HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
    BeginPath(hdc);
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    EndPath(hdc);
    FillPath(hdc);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(fillBrush);
    DeleteObject(borderPen);
}

void DrawProgressBar(HDC hdc, int x, int y, int width, int height, double value, COLORREF trackColor, COLORREF trackBorderColor, ThemeType theme) {
    RECT trackRect = {x, y, x + width, y + height};
    DrawRoundedRect(hdc, trackRect, height / 2, trackColor, trackBorderColor, 1);
    int barWidth = (int)(width * value / 100.0);
    if (barWidth > 0) {
        COLORREF barColor = GetUsageColorWin(value, theme);
        RECT barRect = {x, y, x + barWidth, y + height};
        DrawRoundedRect(hdc, barRect, height / 2, barColor, barColor, 0);
    }
}

void UpdateWindowSize(HWND hwnd) {
    int windowWidth = 220;
    int rowHeight = 48;
    int topMargin = 12;
    int bottomMargin = 10;
    int graphHeight = 55;
    int graphGap = 5;
    int windowHeight = topMargin + g_settings.numSlots * rowHeight + graphGap + graphHeight + bottomMargin;
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    SetWindowPos(hwnd, NULL, screenWidth - windowWidth - 15, 15, windowWidth, windowHeight, SWP_NOZORDER);
}

static ULONG_PTR g_gdiplusToken = 0;

void InitGdiplus() {
    Gdiplus::GdiplusStartupInput si;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &si, NULL);
}

void ShutdownGdiplus() {
    if (g_gdiplusToken) {
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
}

void LoadBgBitmap() {
    if (g_bgBitmap) { DeleteObject(g_bgBitmap); g_bgBitmap = NULL; }
    wchar_t wpath[512];
    MultiByteToWideChar(CP_UTF8, 0, g_settings.bgPath, -1, wpath, 512);
    if (wpath[0] != L'\0') {
        Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromFile(wpath);
        if (bitmap && bitmap->GetLastStatus() == Gdiplus::Ok) {
            bitmap->GetHBITMAP(Gdiplus::Color(0, 0, 0), &g_bgBitmap);
        }
        delete bitmap;
    }
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
    if (hIcon) DestroyIcon(hIcon);
}

void DestroyTrayIcon(HWND hwnd) {
    NOTIFYICONDATAW nid = {0};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hwnd;
    nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

void LoadSettings(Settings& settings) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\FloatMonitor", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(DWORD);
        DWORD val;
        if (RegQueryValueExW(hKey, L"NumSlots", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS && val >= 1 && val <= 4)
            settings.numSlots = val;
        if (RegQueryValueExW(hKey, L"Theme", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS && val < THEME_COUNT)
            settings.theme = (ThemeType)val;
        for (int i = 0; i < 4; i++) {
            wchar_t name[20];
            swprintf(name, 20, L"Slot%d", i);
            if (RegQueryValueExW(hKey, name, NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS && val < GetRealMetricCount())
                settings.slots[i] = (MetricType)val;
        }
        wchar_t wpath[512] = {0};
        DWORD pathSize = sizeof(wpath);
        if (RegQueryValueExW(hKey, L"BgPath", NULL, NULL, (LPBYTE)wpath, &pathSize) == ERROR_SUCCESS)
            WideCharToMultiByte(CP_UTF8, 0, wpath, -1, settings.bgPath, 512, NULL, NULL);
        RegCloseKey(hKey);
    }
}

void SaveSettings(const Settings& settings) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\FloatMonitor", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD val;
        val = settings.numSlots; RegSetValueExW(hKey, L"NumSlots", 0, REG_DWORD, (LPBYTE)&val, sizeof(DWORD));
        val = settings.theme; RegSetValueExW(hKey, L"Theme", 0, REG_DWORD, (LPBYTE)&val, sizeof(DWORD));
        for (int i = 0; i < 4; i++) {
            wchar_t name[20];
            swprintf(name, 20, L"Slot%d", i);
            val = settings.slots[i]; RegSetValueExW(hKey, name, 0, REG_DWORD, (LPBYTE)&val, sizeof(DWORD));
        }
        wchar_t wpath[512] = {0};
        MultiByteToWideChar(CP_UTF8, 0, settings.bgPath, -1, wpath, 512);
        RegSetValueExW(hKey, L"BgPath", 0, REG_SZ, (LPBYTE)wpath, (wcslen(wpath) + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
    }
}

LRESULT CALLBACK SettingsProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static char currentBgPath[512];

    switch (msg) {
    case WM_CREATE: {
        strcpy(currentBgPath, g_settings.bgPath);
        static HFONT titleFont = CreateFontW(-16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
        static HFONT btnFont = CreateFontW(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");

        int y = 15;
        HWND hTitle = CreateWindowW(L"STATIC", L"外观设置", WS_VISIBLE | WS_CHILD, 20, y, 200, 25, hwnd, NULL, g_hInstance, NULL);
        SendMessage(hTitle, WM_SETFONT, (WPARAM)titleFont, TRUE);
        y += 35;

        HWND hThemeLabel = CreateWindowW(L"STATIC", L"主题样式", WS_VISIBLE | WS_CHILD, 20, y, 80, 22, hwnd, NULL, g_hInstance, NULL);
        SendMessage(hThemeLabel, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
        HWND hThemeCombo = CreateWindowW(L"COMBOBOX", L"", WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 100, y - 2, 170, 150, hwnd, (HMENU)ID_THEME_COMBO, g_hInstance, NULL);
        SendMessage(hThemeCombo, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
        for (int i = 0; i < THEME_COUNT; i++) {
            wchar_t wname[64];
            MultiByteToWideChar(CP_UTF8, 0, THEME_NAMES[i], -1, wname, 64);
            SendMessageW(hThemeCombo, CB_ADDSTRING, 0, (LPARAM)wname);
        }
        SendMessage(hThemeCombo, CB_SETCURSEL, g_settings.theme, 0);
        y += 32;

        HWND hSlotsLabel = CreateWindowW(L"STATIC", L"显示栏目数", WS_VISIBLE | WS_CHILD, 20, y, 80, 22, hwnd, NULL, g_hInstance, NULL);
        SendMessage(hSlotsLabel, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
        HWND hSlotsCombo = CreateWindowW(L"COMBOBOX", L"", WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 100, y - 2, 170, 100, hwnd, (HMENU)ID_SLOTS_COMBO, g_hInstance, NULL);
        SendMessage(hSlotsCombo, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
        for (int i = 1; i <= 4; i++) {
            wchar_t buf[10];
            swprintf(buf, 10, L"%d 个", i);
            SendMessageW(hSlotsCombo, CB_ADDSTRING, 0, (LPARAM)buf);
        }
        SendMessage(hSlotsCombo, CB_SETCURSEL, g_settings.numSlots - 1, 0);
        y += 32;

        HWND hBgLabel = CreateWindowW(L"STATIC", L"背景图片", WS_VISIBLE | WS_CHILD, 20, y, 80, 22, hwnd, NULL, g_hInstance, NULL);
        SendMessage(hBgLabel, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
        HWND hBgBtn = CreateWindowW(L"BUTTON", L"浏览...", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 100, y - 2, 85, 26, hwnd, (HMENU)ID_BG_BUTTON, g_hInstance, NULL);
        SendMessage(hBgBtn, WM_SETFONT, (WPARAM)btnFont, TRUE);
        HWND hBgClear = CreateWindowW(L"BUTTON", L"清除", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 190, y - 2, 80, 26, hwnd, (HMENU)ID_BG_CLEAR, g_hInstance, NULL);
        SendMessage(hBgClear, WM_SETFONT, (WPARAM)btnFont, TRUE);
        y += 40;

        HWND hSep = CreateWindowW(L"STATIC", L"", WS_VISIBLE | WS_CHILD | SS_ETCHEDHORZ, 20, y, 270, 2, hwnd, NULL, g_hInstance, NULL);
        y += 15;

        HWND hDisplayTitle = CreateWindowW(L"STATIC", L"栏目配置", WS_VISIBLE | WS_CHILD, 20, y, 200, 25, hwnd, NULL, g_hInstance, NULL);
        SendMessage(hDisplayTitle, WM_SETFONT, (WPARAM)titleFont, TRUE);
        y += 32;

        int comboIds[] = {ID_COMBO1, ID_COMBO2, ID_COMBO3, ID_COMBO4};
        for (int i = 0; i < 4; i++) {
            wchar_t labelText[20];
            swprintf(labelText, 20, L"栏目 %d", i + 1);
            HWND hLabel = CreateWindowW(L"STATIC", labelText, WS_VISIBLE | WS_CHILD, 20, y, 60, 22, hwnd, NULL, g_hInstance, NULL);
            SendMessage(hLabel, WM_SETFONT, (WPARAM)g_uiFont, TRUE);

            HWND hCombo = CreateWindowW(L"COMBOBOX", L"", WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 85, y - 2, 185, 200, hwnd, (HMENU)comboIds[i], g_hInstance, NULL);
            SendMessage(hCombo, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
            int metricCount = GetRealMetricCount();
            for (int j = 0; j < metricCount; j++) {
                wchar_t wname[64];
                MultiByteToWideChar(CP_UTF8, 0, GetMetricName((MetricType)j), -1, wname, 64);
                SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)wname);
            }
            if (g_settings.slots[i] < metricCount)
                SendMessage(hCombo, CB_SETCURSEL, g_settings.slots[i], 0);
            y += 32;
        }

        y += 15;
        HWND hApply = CreateWindowW(L"BUTTON", L"应用设置", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 100, y, 100, 32, hwnd, (HMENU)ID_APPLY, g_hInstance, NULL);
        SendMessage(hApply, WM_SETFONT, (WPARAM)btnFont, TRUE);
        break;
    }
    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case ID_BG_BUTTON: {
            OPENFILENAMEW ofn = {0};
            wchar_t fileBuf[MAX_PATH] = {0};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = L"图片文件\0*.bmp;*.jpg;*.png\0所有文件\0*.*\0";
            ofn.lpstrFile = fileBuf;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            if (GetOpenFileNameW(&ofn))
                WideCharToMultiByte(CP_UTF8, 0, fileBuf, -1, currentBgPath, 512, NULL, NULL);
            break;
        }
        case ID_BG_CLEAR: currentBgPath[0] = '\0'; break;
        case ID_APPLY: {
            int numSlots = (int)SendMessage(GetDlgItem(hwnd, ID_SLOTS_COMBO), CB_GETCURSEL, 0, 0) + 1;
            int theme = (int)SendMessage(GetDlgItem(hwnd, ID_THEME_COMBO), CB_GETCURSEL, 0, 0);
            int comboIds[] = {ID_COMBO1, ID_COMBO2, ID_COMBO3, ID_COMBO4};
            int selections[4];
            for (int i = 0; i < 4; i++)
                selections[i] = (int)SendMessage(GetDlgItem(hwnd, comboIds[i]), CB_GETCURSEL, 0, 0);
            bool duplicate = false;
            for (int i = 0; i < numSlots && !duplicate; i++)
                for (int j = i + 1; j < numSlots; j++)
                    if (selections[i] == selections[j]) { duplicate = true; break; }
            if (duplicate) {
                MessageBoxW(hwnd, L"栏目不能选择相同的指标", L"提示", MB_OK | MB_ICONWARNING);
                return 0;
            }
            g_settings.numSlots = numSlots;
            g_settings.theme = (ThemeType)theme;
            for (int i = 0; i < 4; i++) g_settings.slots[i] = (MetricType)selections[i];
            strcpy(g_settings.bgPath, currentBgPath);
            SaveSettings(g_settings);
            LoadBgBitmap();
            HWND hMain = FindWindowW(L"FloatMonitor", NULL);
            if (hMain) { UpdateWindowSize(hMain); InvalidateRect(hMain, NULL, TRUE); }
            DestroyWindow(hwnd);
            break;
        }
        }
        break;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)wParam;
        SetTextColor(hdcStatic, RGB(60, 60, 60));
        SetBkMode(hdcStatic, TRANSPARENT);
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }
    case WM_CTLCOLORBTN: {
        HDC hdcBtn = (HDC)wParam;
        SetTextColor(hdcBtn, RGB(40, 60, 100));
        SetBkMode(hdcBtn, TRANSPARENT);
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }
    case WM_CLOSE: DestroyWindow(hwnd); break;
    default: return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// PDH counters management functions - extracted to eliminate code duplication
static HQUERY pdhQuery = NULL;
static HCOUNTER gpuCounter = NULL;
static HCOUNTER diskCounters[26] = {NULL};
static HCOUNTER diskAllCounter = NULL;
static bool pdhReady = false;

void CleanupPdhCounters() {
    if (gpuCounter) {
        PdhRemoveCounter(gpuCounter);
        gpuCounter = NULL;
    }
    if (diskAllCounter) {
        PdhRemoveCounter(diskAllCounter);
        diskAllCounter = NULL;
    }
    for (int i = 0; i < 26; i++) {
        if (diskCounters[i]) {
            PdhRemoveCounter(diskCounters[i]);
            diskCounters[i] = NULL;
        }
    }
    if (pdhQuery) {
        PdhCloseQuery(pdhQuery);
        pdhQuery = NULL;
    }
    pdhReady = false;
}

bool InitializePdhCounters() {
    CleanupPdhCounters();  // Ensure old resources are cleaned up first
    
    PDH_STATUS status = PdhOpenQuery(NULL, 0, &pdhQuery);
    if (status != ERROR_SUCCESS) {
        pdhReady = false;
        return false;
    }
    
    // Add GPU counter
    if (PdhAddCounterA(pdhQuery, "\\GPU Engine(*)\\Utilization Percentage", 0, &gpuCounter) != ERROR_SUCCESS) {
        gpuCounter = NULL;
    }
    
    // Add total disk counter
    if (PdhAddCounterA(pdhQuery, "\\PhysicalDisk(_Total)\\% Disk Time", 0, &diskAllCounter) != ERROR_SUCCESS) {
        diskAllCounter = NULL;
    }
    
    // Add individual disk counters
    for (int i = 0; i < g_diskCount; i++) {
        char counterPath[128];
        sprintf(counterPath, "\\LogicalDisk(%c:)\\%% Disk Time", g_disks[i].letter);
        if (PdhAddCounterA(pdhQuery, counterPath, 0, &diskCounters[i]) != ERROR_SUCCESS) {
            diskCounters[i] = NULL;
        }
    }
    
    // Collect initial data
    PdhCollectQueryData(pdhQuery);
    pdhReady = true;
    return true;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static FILETIME prevIdleTime = {0};
    static FILETIME prevKernelTime = {0};
    static FILETIME prevUserTime = {0};
    static ULONGLONG totalPhys = 0;
    static bool isHovering = false;

    switch (msg) {
    case WM_CREATE: {
        MEMORYSTATUSEX memStatus;
        memStatus.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memStatus)) totalPhys = memStatus.ullTotalPhys;
        GetSystemTimes(&prevIdleTime, &prevKernelTime, &prevUserTime);
        InitializePdhCounters();
        QueryCPUName();
        QueryGPUName();
        LoadBgBitmap();
        SetTimer(hwnd, TIMER_ID, 1000, NULL);
        break;
    }
    case WM_MOUSEMOVE: if (!isHovering) { isHovering = true; InvalidateRect(hwnd, NULL, TRUE); } break;
    case WM_MOUSELEAVE: isHovering = false; InvalidateRect(hwnd, NULL, TRUE); break;
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
        if (totalDiff > 0) g_monitorData.cpuUsage = (100.0 - (idleDiff * 100.0 / totalDiff));
        prevIdleTime = idleTime; prevKernelTime = kernelTime; prevUserTime = userTime;

        // 记录CPU历史
        g_cpuHistory[g_cpuHistoryIdx] = g_monitorData.cpuUsage;
        g_cpuHistoryIdx = (g_cpuHistoryIdx + 1) % 120;
        if (g_cpuHistoryLen < 120) g_cpuHistoryLen++;

        MEMORYSTATUSEX memStatus;
        memStatus.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memStatus) && totalPhys > 0) {
            g_monitorData.memoryUsage = (100.0 - (memStatus.ullAvailPhys * 100.0 / totalPhys));
            if (memStatus.ullTotalPageFile > 0)
                g_monitorData.swapUsage = (100.0 - (memStatus.ullAvailPageFile * 100.0 / memStatus.ullTotalPageFile));
        }
        if (pdhReady) {
            PdhCollectQueryData(pdhQuery);
            if (gpuCounter) {
                DWORD bufSize = 0;
                DWORD itemCount = 0;
                PDH_STATUS pdhStatus = PdhGetFormattedCounterArray(gpuCounter, PDH_FMT_DOUBLE, &bufSize, &itemCount, NULL);
                if (pdhStatus == PDH_MORE_DATA || pdhStatus == ERROR_SUCCESS) {
                    PPDH_FMT_COUNTERVALUE_ITEM items = (PPDH_FMT_COUNTERVALUE_ITEM)malloc(bufSize);
                    if (items) {
                        pdhStatus = PdhGetFormattedCounterArray(gpuCounter, PDH_FMT_DOUBLE, &bufSize, &itemCount, items);
                        if (pdhStatus == ERROR_SUCCESS) {
                            double maxGpu = 0;
                            for (DWORD i = 0; i < itemCount; i++) {
                                double val = items[i].FmtValue.doubleValue;
                                if (val > maxGpu && val <= 100) maxGpu = val;
                            }
                            g_monitorData.gpuUsage = maxGpu;
                        }
                        free(items);
                    }
                }
            }
            if (diskAllCounter) {
                PDH_FMT_COUNTERVALUE cv;
                if (PdhGetFormattedCounterValue(diskAllCounter, PDH_FMT_DOUBLE, NULL, &cv) == ERROR_SUCCESS) {
                    g_monitorData.diskUsage = cv.doubleValue;
                    if (g_monitorData.diskUsage < 0) g_monitorData.diskUsage = 0;
                    if (g_monitorData.diskUsage > 100) g_monitorData.diskUsage = 100;
                }
            }
            for (int i = 0; i < g_diskCount; i++) {
                if (diskCounters[i]) {
                    PDH_FMT_COUNTERVALUE cv;
                    if (PdhGetFormattedCounterValue(diskCounters[i], PDH_FMT_DOUBLE, NULL, &cv) == ERROR_SUCCESS) {
                        g_diskValues[i] = cv.doubleValue;
                        if (g_diskValues[i] < 0) g_diskValues[i] = 0;
                        if (g_diskValues[i] > 100) g_diskValues[i] = 100;
                    }
                }
            }
        }
        // 温度查询（每5秒一次，避免频繁WMI调用）
        static int tempTimer = 0;
        tempTimer++;
        if (tempTimer >= 5) {
            tempTimer = 0;
            QueryTemperatures();
        }
        InvalidateRect(hwnd, NULL, TRUE);
        break;
    }
    case WM_SETCURSOR: SetCursor(LoadCursor(NULL, IDC_ARROW)); return TRUE;
    case WM_DEVICECHANGE: {
        if (wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE) {
            DEV_BROADCAST_HDR* dbh = (DEV_BROADCAST_HDR*)lParam;
            if (dbh && dbh->dbch_devicetype == DBT_DEVTYP_VOLUME) {
                int oldCount = g_diskCount;
                char oldLetters[26];
                for (int i = 0; i < oldCount; i++) oldLetters[i] = g_disks[i].letter;

                CleanupPdhCounters();
                DetectDisks();
                InitializePdhCounters();
            }
        }
        break;
    }
    case WM_CONTEXTMENU: {
        POINT pt = {LOWORD(lParam), HIWORD(lParam)};
        ClientToScreen(hwnd, &pt);
        HMENU hMenu = CreatePopupMenu();
        AppendMenuW(hMenu, MF_STRING, ID_SETTINGS, L"设置");
        AppendMenuW(hMenu, MF_STRING, ID_HIDE, L"隐藏");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hMenu, MF_STRING, ID_CLOSE, L"退出");
        TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
        DestroyMenu(hMenu);
        break;
    }
    case WM_TRAYMESSAGE: {
        if (lParam == WM_RBUTTONUP) {
            POINT pt; GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, ID_SHOW, L"显示");
            AppendMenuW(hMenu, MF_STRING, ID_SETTINGS, L"设置");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, ID_CLOSE, L"退出");
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        } else if (lParam == WM_LBUTTONUP) ShowWindow(hwnd, SW_SHOW);
        break;
    }
    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case ID_SHOW: ShowWindow(hwnd, SW_SHOW); break;
        case ID_HIDE: ShowWindow(hwnd, SW_HIDE); break;
        case ID_SETTINGS: {
            HWND hSettings = CreateWindowExW(WS_EX_TOOLWINDOW, L"SettingsWindow", L"监控设置",
                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 340, 480,
                hwnd, NULL, g_hInstance, NULL);
            if (hSettings) {
                ShowWindow(hSettings, SW_SHOW);
                UpdateWindow(hSettings);
            }
            break;
        }
        case ID_CLOSE: DestroyWindow(hwnd); break;
        }
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect; GetClientRect(hwnd, &rect);
        const ThemeColors& tc = THEMES[g_settings.theme];
        COLORREF bgColor = isHovering ? RGB(tc.bgHoverR*255, tc.bgHoverG*255, tc.bgHoverB*255) : RGB(tc.bgR*255, tc.bgG*255, tc.bgB*255);
        COLORREF borderColor = isHovering ? RGB(tc.borderHoverR*255, tc.borderHoverG*255, tc.borderHoverB*255) : RGB(tc.borderR*255, tc.borderG*255, tc.borderB*255);

        if (g_bgBitmap) {
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, g_bgBitmap);
            BITMAP bm;
            GetObject(g_bgBitmap, sizeof(bm), &bm);
            SetStretchBltMode(hdc, HALFTONE);
            StretchBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
            SelectObject(memDC, oldBmp);
            DeleteDC(memDC);

            HDC blendDC = CreateCompatibleDC(hdc);
            HBITMAP blendBmp = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
            HBITMAP oldBlend = (HBITMAP)SelectObject(blendDC, blendBmp);
            DrawRoundedRect(blendDC, rect, 12, bgColor, borderColor, 1);
            BLENDFUNCTION bf = {AC_SRC_OVER, 0, 160, 0};
            AlphaBlend(hdc, 0, 0, rect.right, rect.bottom, blendDC, 0, 0, rect.right, rect.bottom, bf);
            SelectObject(blendDC, oldBlend);
            DeleteObject(blendBmp);
            DeleteDC(blendDC);
        } else {
            DrawRoundedRect(hdc, rect, 12, bgColor, borderColor, 1);
        }
        HFONT labelFont = GetSystemUIFont(0, false);
        HFONT valueFont = GetSystemUIFont(1, true);
        for (int i = 0; i < g_settings.numSlots; i++) {
            MetricType metric = g_settings.slots[i];
            double value = GetMetricValue(g_monitorData, metric);
            COLORREF valueColor = GetUsageColorWinForMetric(value, metric, g_settings.theme);
            int yPos = 12 + i * 48;

            // 标签
            SelectObject(hdc, labelFont);
            SetTextColor(hdc, RGB(tc.labelR*255, tc.labelG*255, tc.labelB*255));
            SetBkMode(hdc, TRANSPARENT);
            RECT labelRect = {15, yPos, 75, yPos + 16};
            DrawTextA(hdc, GetMetricShort(metric), -1, &labelRect, DT_LEFT | DT_VCENTER);

            // 数值
            SelectObject(hdc, valueFont);
            SetTextColor(hdc, valueColor);
            RECT valueRect = {rect.right - 60, yPos, rect.right - 15, yPos + 16};
            if (metric == METRIC_CPU_TEMP || metric == METRIC_GPU_TEMP) {
                double temp = (metric == METRIC_CPU_TEMP) ? g_cpuTemp : g_gpuTemp;
                wchar_t wValueBuf[32];
                if (temp >= 0) swprintf(wValueBuf, 32, L"%.0f\u00B0C", temp);
                else wcscpy(wValueBuf, L"N/A");
                DrawTextW(hdc, wValueBuf, -1, &valueRect, DT_RIGHT | DT_VCENTER);
            } else {
                char valueBuf[32];
                sprintf(valueBuf, "%.1f%%", value);
                DrawTextA(hdc, valueBuf, -1, &valueRect, DT_RIGHT | DT_VCENTER);
            }

            // 型号名和温度（仅 CPU/GPU 栏目）
            if (metric == METRIC_CPU || metric == METRIC_GPU) {
                HFONT smallFont = GetSystemUIFont(-3, false);
                SelectObject(hdc, smallFont);
                SetTextColor(hdc, RGB((int)(tc.labelR*255*0.7), (int)(tc.labelG*255*0.7), (int)(tc.labelB*255*0.7)));
                wchar_t wInfoBuf[128] = L"";
                const char* name = (metric == METRIC_CPU) ? g_cpuName : g_gpuName;
                double temp = (metric == METRIC_CPU) ? g_cpuTemp : g_gpuTemp;
                if (name[0]) {
                    wchar_t wName[64] = L"";
                    MultiByteToWideChar(CP_UTF8, 0, name, -1, wName, 64);
                    wcsncpy(wInfoBuf, wName, 60);
                    wInfoBuf[60] = L'\0';
                }
                if (temp >= 0) {
                    wchar_t wTempBuf[16];
                    swprintf(wTempBuf, 16, L" %.0f\u00B0C", temp);
                    wcscat(wInfoBuf, wTempBuf);
                }
                if (wInfoBuf[0]) {
                    RECT infoRect = {15, yPos + 16, rect.right - 15, yPos + 28};
                    DrawTextW(hdc, wInfoBuf, -1, &infoRect, DT_LEFT | DT_VCENTER);
                }
                DeleteObject(smallFont);
            }

            // 进度条（温度栏目也显示进度条，范围0-100°C）
            double barValue = (metric == METRIC_CPU_TEMP || metric == METRIC_GPU_TEMP) ? (value > 100 ? 100 : value) : value;
            DrawProgressBar(hdc, 15, yPos + 32, rect.right - 30, 6,
                barValue, RGB(tc.trackR*255, tc.trackG*255, tc.trackB*255),
                RGB(tc.trackBorderR*255, tc.trackBorderG*255, tc.trackBorderB*255), g_settings.theme);
        }

        // 绘制CPU曲线图
        int graphY = 12 + g_settings.numSlots * 48 + 5;
        DrawCurveGraph(hdc, 15, graphY, rect.right - 30, 50, g_settings.theme);

        DeleteObject(labelFont); DeleteObject(valueFont);
        EndPaint(hwnd, &ps);
        break;
    }
    case WM_DESTROY:
        KillTimer(hwnd, TIMER_ID);
        if (gpuCounter) PdhRemoveCounter(gpuCounter);
        if (diskAllCounter) PdhRemoveCounter(diskAllCounter);
        for (int i = 0; i < g_diskCount; i++) {
            if (diskCounters[i]) PdhRemoveCounter(diskCounters[i]);
        }
        if (pdhQuery) PdhCloseQuery(pdhQuery);
        if (g_bgBitmap) { DeleteObject(g_bgBitmap); g_bgBitmap = NULL; }
        if (g_uiFont) { DeleteObject(g_uiFont); g_uiFont = NULL; }
        ShutdownGdiplus();
        DestroyTrayIcon(hwnd);
        PostQuitMessage(0);
        break;
    default: return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

#define SPLASH_TIMER 2
#define SPLASH_DURATION 1800

static double g_splashAngle = 0;
static double g_splashAlpha = 0;
static DWORD g_splashStart = 0;

LRESULT CALLBACK SplashProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_splashStart = GetTickCount();
        g_splashAngle = 0;
        g_splashAlpha = 0;
        SetTimer(hwnd, SPLASH_TIMER, 16, NULL);
        break;
    }
    case WM_TIMER: {
        DWORD elapsed = GetTickCount() - g_splashStart;
        if (elapsed < 300) {
            g_splashAlpha = (double)elapsed / 300.0;
        } else if (elapsed > SPLASH_DURATION - 400) {
            g_splashAlpha = (double)(SPLASH_DURATION - elapsed) / 400.0;
            if (g_splashAlpha < 0) g_splashAlpha = 0;
        } else {
            g_splashAlpha = 1.0;
        }
        g_splashAngle += 4.0;
        if (g_splashAngle >= 360.0) g_splashAngle -= 360.0;
        InvalidateRect(hwnd, NULL, FALSE);
        if (elapsed >= SPLASH_DURATION) {
            KillTimer(hwnd, SPLASH_TIMER);
            PostMessage(hwnd, WM_CLOSE, 0, 0);
        }
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);
        int cx = rect.right / 2;
        int cy = rect.bottom / 2;

        SetBkMode(hdc, TRANSPARENT);

        HBRUSH bgBrush = CreateSolidBrush(RGB(20, 20, 28));
        FillRect(hdc, &rect, bgBrush);
        DeleteObject(bgBrush);

        int armLen = 22;
        int armWidth = 6;
        double angleRad = g_splashAngle * 3.14159265 / 180.0;
        double alpha = g_splashAlpha;

        COLORREF crossColor = RGB(
            (int)(100 * alpha + 20 * (1 - alpha)),
            (int)(180 * alpha + 20 * (1 - alpha)),
            (int)(255 * alpha + 28 * (1 - alpha))
        );

        HBRUSH crossBrush = CreateSolidBrush(crossColor);
        HPEN crossPen = CreatePen(PS_SOLID, 0, crossColor);
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, crossBrush);
        HPEN oldPen = (HPEN)SelectObject(hdc, crossPen);

        for (int arm = 0; arm < 4; arm++) {
            double armAngle = angleRad + arm * 3.14159265 / 2.0;
            double perpAngle = armAngle + 3.14159265 / 2.0;

            double tipX = cx + armLen * cos(armAngle);
            double tipY = cy + armLen * sin(armAngle);
            double leftX = cx + (armWidth / 2.0) * cos(perpAngle);
            double leftY = cy + (armWidth / 2.0) * sin(perpAngle);
            double rightX = cx - (armWidth / 2.0) * cos(perpAngle);
            double rightY = cy - (armWidth / 2.0) * sin(perpAngle);

            POINT pts[4];
            pts[0].x = (LONG)leftX; pts[0].y = (LONG)leftY;
            pts[1].x = (LONG)(tipX + (armWidth / 2.0) * cos(perpAngle) * 0.3);
            pts[1].y = (LONG)(tipY + (armWidth / 2.0) * sin(perpAngle) * 0.3);
            pts[2].x = (LONG)(tipX - (armWidth / 2.0) * cos(perpAngle) * 0.3);
            pts[2].y = (LONG)(tipY - (armWidth / 2.0) * sin(perpAngle) * 0.3);
            pts[3].x = (LONG)rightX; pts[3].y = (LONG)rightY;

            Polygon(hdc, pts, 4);
        }

        int centerR = (int)(armWidth * 0.8);
        Ellipse(hdc, cx - centerR, cy - centerR, cx + centerR, cy + centerR);

        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(crossBrush);
        DeleteObject(crossPen);

        if (alpha > 0.3) {
            HFONT textFont = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
            SelectObject(hdc, textFont);
            COLORREF textColor = RGB(
                (int)(160 * alpha),
                (int)(180 * alpha),
                (int)(220 * alpha)
            );
            SetTextColor(hdc, textColor);
            RECT textRect = {0, cy + 40, rect.right, cy + 65};
            DrawTextW(hdc, L"System Monitor", -1, &textRect, DT_CENTER | DT_VCENTER);
            DeleteObject(textFont);
        }

        EndPaint(hwnd, &ps);
        break;
    }
    case WM_ERASEBKGND: return TRUE;
    default: return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInstance = hInstance;
    InitSettings();
    DetectDisks();
    InitGdiplus();
    g_uiFont = GetSystemUIFont(0, false);

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"FloatMonitor";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    WNDCLASSW swc = {0};
    swc.lpfnWndProc = SettingsProc;
    swc.hInstance = hInstance;
    swc.lpszClassName = L"SettingsWindow";
    swc.hCursor = LoadCursor(NULL, IDC_ARROW);
    swc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&swc);

    WNDCLASSW splashwc = {0};
    splashwc.lpfnWndProc = SplashProc;
    splashwc.hInstance = hInstance;
    splashwc.lpszClassName = L"SplashWindow";
    splashwc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&splashwc);

    LoadSettings(g_settings);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    HWND hSplash = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        L"SplashWindow", L"", WS_POPUP,
        screenWidth / 2 - 80, screenHeight / 2 - 80, 160, 160,
        NULL, NULL, hInstance, NULL);
    SetLayeredWindowAttributes(hSplash, RGB(20, 20, 28), 0, LWA_COLORKEY);
    SetLayeredWindowAttributes(hSplash, 0, 240, LWA_ALPHA);
    ShowWindow(hSplash, SW_SHOW);
    UpdateWindow(hSplash);

    MSG msg;
    while (IsWindow(hSplash)) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    if (IsWindow(hSplash)) DestroyWindow(hSplash);

    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        L"FloatMonitor", L"", WS_POPUP | WS_SYSMENU,
        screenWidth - 235, 15, 220, 200, NULL, NULL, hInstance, NULL);
    SetLayeredWindowAttributes(hwnd, 0, 235, LWA_ALPHA);
    UpdateWindowSize(hwnd);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    CreateTrayIcon(hwnd);

    while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return 0;
}

// ============================================================
// Platform: Linux / macOS (GTK3)
// ============================================================
#elif defined(__linux__) || defined(__APPLE__)

#include <pango/pangofont.h>

static GtkWindow* g_mainWindow = NULL;
static GtkStatusIcon* g_trayIcon = NULL;

std::string GetConfigPath() {
    const char* home = getenv("HOME");
    if (!home) home = "/tmp";
    return std::string(home) + "/.config/float_monitor.conf";
}

void LoadSettings(Settings& settings) {
    std::string path = GetConfigPath();
    std::ifstream file(path);
    if (!file.is_open()) return;
    std::string line;
    while (std::getline(file, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        if (key == "NumSlots") settings.numSlots = std::clamp(atoi(val.c_str()), 1, 4);
        else if (key == "Theme") settings.theme = (ThemeType)std::clamp(atoi(val.c_str()), 0, (int)THEME_COUNT - 1);
        else if (key == "BgPath") strncpy(settings.bgPath, val.c_str(), 511);
        else if (key.substr(0, 4) == "Slot") {
            int idx = atoi(key.substr(4).c_str());
            if (idx >= 0 && idx < 4) settings.slots[idx] = (MetricType)std::clamp(atoi(val.c_str()), 0, (int)GetRealMetricCount() - 1);
        }
    }
}

void SaveSettings(const Settings& settings) {
    std::string path = GetConfigPath();
    std::ofstream file(path);
    if (!file.is_open()) return;
    file << "NumSlots=" << settings.numSlots << "\n";
    file << "Theme=" << settings.theme << "\n";
    file << "BgPath=" << settings.bgPath << "\n";
    for (int i = 0; i < 4; i++)
        file << "Slot" << i << "=" << settings.slots[i] << "\n";
}

#ifdef __linux__
void UpdateMetrics() {
    static unsigned long long prevIdle = 0, prevTotal = 0;
    std::ifstream statFile("/proc/stat");
    if (statFile.is_open()) {
        std::string line;
        if (std::getline(statFile, line)) {
            unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
            sscanf(line.c_str() + 5, "%llu %llu %llu %llu %llu %llu %llu %llu",
                   &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
            unsigned long long total = user + nice + system + idle + iowait + irq + softirq + steal;
            unsigned long long idleAll = idle + iowait;
            if (total - prevTotal > 0)
                g_monitorData.cpuUsage = 100.0 * (1.0 - (double)(idleAll - prevIdle) / (double)(total - prevTotal));
            prevIdle = idleAll;
            prevTotal = total;
        }
    }

    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        g_monitorData.memoryUsage = 100.0 * (1.0 - (double)si.freeram / si.totalram);
        if (si.totalswap > 0)
            g_monitorData.swapUsage = 100.0 * (1.0 - (double)si.freeswap / si.totalswap);
    }

    g_monitorData.gpuUsage = 0;
    g_monitorData.diskUsage = 0;
}
#elif defined(__APPLE__)
void UpdateMetrics() {
    static unsigned long long prevIdle = 0, prevTotal = 0;
    host_cpu_load_info_data_t cpuLoad;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t)&cpuLoad, &count) == KERN_SUCCESS) {
        unsigned long long total = 0;
        for (int i = 0; i < CPU_STATE_MAX; i++) total += cpuLoad.cpu_ticks[i];
        unsigned long long idle = cpuLoad.cpu_ticks[CPU_STATE_IDLE];
        if (total - prevTotal > 0)
            g_monitorData.cpuUsage = 100.0 * (1.0 - (double)(idle - prevIdle) / (double)(total - prevTotal));
        prevIdle = idle;
        prevTotal = total;
    }

    int mib[2] = {CTL_HW, HW_MEMSIZE};
    int64_t totalMem;
    size_t len = sizeof(totalMem);
    if (sysctl(mib, 2, &totalMem, &len, NULL, 0) == 0) {
        vm_statistics64_data_t vmStats;
        mach_msg_type_number_t vmCount = HOST_VM_INFO64_COUNT;
        if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vmStats, &vmCount) == KERN_SUCCESS) {
            int64_t used = (int64_t)(vmStats.active_count + vmStats.wire_count) * vm_kernel_page_size;
            g_monitorData.memoryUsage = 100.0 * used / totalMem;
        }
    }

    g_monitorData.swapUsage = 0;
    g_monitorData.gpuUsage = 0;
    g_monitorData.diskUsage = 0;
}
#endif

static gboolean on_draw(GtkWidget* widget, cairo_t* cr, gpointer data) {
    const ThemeColors& tc = THEMES[g_settings.theme];
    int width = gtk_widget_get_allocated_width(widget);
    int height = gtk_widget_get_allocated_height(widget);

    cairo_set_source_rgba(cr, tc.bgR, tc.bgG, tc.bgB, 0.92);
    cairo_move_to(cr, 12, 0);
    cairo_line_to(cr, width - 12, 0);
    cairo_curve_to(cr, width, 0, width, 0, width, 12);
    cairo_line_to(cr, width, height - 12);
    cairo_curve_to(cr, width, height, width, height, width - 12, height);
    cairo_line_to(cr, 12, height);
    cairo_curve_to(cr, 0, height, 0, height, 0, height - 12);
    cairo_line_to(cr, 0, 12);
    cairo_curve_to(cr, 0, 0, 0, 0, 12, 0);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, tc.borderR, tc.borderG, tc.borderB);
    cairo_set_line_width(cr, 1);
    cairo_move_to(cr, 12.5, 0.5);
    cairo_line_to(cr, width - 12.5, 0.5);
    cairo_curve_to(cr, width - 0.5, 0.5, width - 0.5, 0.5, width - 0.5, 12.5);
    cairo_line_to(cr, width - 0.5, height - 12.5);
    cairo_curve_to(cr, width - 0.5, height - 0.5, width - 0.5, height - 0.5, width - 12.5, height - 0.5);
    cairo_line_to(cr, 12.5, height - 0.5);
    cairo_curve_to(cr, 0.5, height - 0.5, 0.5, height - 0.5, 0.5, height - 12.5);
    cairo_line_to(cr, 0.5, 12.5);
    cairo_curve_to(cr, 0.5, 0.5, 0.5, 0.5, 12.5, 0.5);
    cairo_stroke(cr);

    for (int i = 0; i < g_settings.numSlots; i++) {
        MetricType metric = g_settings.slots[i];
        double value = GetMetricValue(g_monitorData, metric);
        int yPos = 12 + i * 44;

        PangoLayout* labelLayout = pango_cairo_create_layout(cr);
        PangoFontDescription* labelDesc = pango_font_description_from_string("Microsoft YaHei UI 11");
        pango_layout_set_font_description(labelLayout, labelDesc);
        pango_layout_set_text(labelLayout, GetMetricShort(metric), -1);
        cairo_set_source_rgb(cr, tc.labelR, tc.labelG, tc.labelB);
        cairo_move_to(cr, 15, yPos);
        pango_cairo_show_layout(cr, labelLayout);
        pango_font_description_free(labelDesc);
        g_object_unref(labelLayout);

        double vr, vg, vb;
        GetUsageColor(value, g_settings.theme, vr, vg, vb);
        char valueBuf[20];
        sprintf(valueBuf, "%.1f%%", value);
        PangoLayout* valueLayout = pango_cairo_create_layout(cr);
        PangoFontDescription* valueDesc = pango_font_description_from_string("Microsoft YaHei UI Bold 13");
        pango_layout_set_font_description(valueLayout, valueDesc);
        pango_layout_set_text(valueLayout, valueBuf, -1);
        pango_layout_set_width(valueLayout, (width - 30) * PANGO_SCALE);
        pango_layout_set_alignment(valueLayout, PANGO_ALIGN_RIGHT);
        cairo_set_source_rgb(cr, vr, vg, vb);
        cairo_move_to(cr, 15, yPos);
        pango_cairo_show_layout(cr, valueLayout);
        pango_font_description_free(valueDesc);
        g_object_unref(valueLayout);

        int barY = yPos + 22;
        int barWidth = width - 30;
        int barHeight = 6;
        cairo_set_source_rgb(cr, tc.trackR, tc.trackG, tc.trackB);
        cairo_move_to(cr, 15 + barHeight/2, barY);
        cairo_line_to(cr, 15 + barWidth - barHeight/2, barY);
        cairo_line_to(cr, 15 + barWidth - barHeight/2, barY + barHeight);
        cairo_line_to(cr, 15 + barHeight/2, barY + barHeight);
        cairo_close_path(cr);
        cairo_fill(cr);

        int fillWidth = (int)(barWidth * value / 100.0);
        if (fillWidth > 0) {
            cairo_set_source_rgb(cr, vr, vg, vb);
            cairo_move_to(cr, 15 + barHeight/2, barY);
            cairo_line_to(cr, 15 + fillWidth - barHeight/2, barY);
            cairo_line_to(cr, 15 + fillWidth - barHeight/2, barY + barHeight);
            cairo_line_to(cr, 15 + barHeight/2, barY + barHeight);
            cairo_close_path(cr);
            cairo_fill(cr);
        }
    }
    return FALSE;
}

static gboolean on_timer(gpointer data) {
    UpdateMetrics();
    GtkWidget* drawing = GTK_WIDGET(data);
    gtk_widget_queue_draw(drawing);
    return G_SOURCE_CONTINUE;
}

static void on_tray_click(GtkStatusIcon* icon, gpointer data) {
    gtk_widget_show_all(GTK_WIDGET(g_mainWindow));
}

static void on_tray_menu(GtkStatusIcon* icon, guint button, guint activate_time, gpointer data) {
    GtkWidget* menu = gtk_menu_new();
    GtkWidget* showItem = gtk_menu_item_new_with_label("显示");
    GtkWidget* settingsItem = gtk_menu_item_new_with_label("设置");
    GtkWidget* quitItem = gtk_menu_item_new_with_label("退出");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), showItem);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), settingsItem);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), quitItem);

    g_signal_connect(showItem, "activate", G_CALLBACK(+[](GtkMenuItem* item, gpointer data) {
        gtk_widget_show_all(GTK_WIDGET(g_mainWindow));
    }), NULL);

    g_signal_connect(settingsItem, "activate", G_CALLBACK(+[](GtkMenuItem* item, gpointer data) {
        GtkWidget* dialog = gtk_dialog_new_with_buttons("监控设置", g_mainWindow,
            GTK_DIALOG_MODAL, "应用", GTK_RESPONSE_ACCEPT, "取消", GTK_RESPONSE_CANCEL, NULL);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 400);
        GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
        GtkWidget* grid = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
        gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
        gtk_container_set_border_width(GTK_CONTAINER(grid), 15);

        GtkWidget* themeLabel = gtk_label_new("主题样式");
        GtkWidget* themeCombo = gtk_combo_box_text_new();
        for (int i = 0; i < THEME_COUNT; i++) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(themeCombo), THEME_NAMES[i]);
        gtk_combo_box_set_active(GTK_COMBO_BOX(themeCombo), g_settings.theme);
        gtk_grid_attach(GTK_GRID(grid), themeLabel, 0, 0, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), themeCombo, 1, 0, 1, 1);

        GtkWidget* slotsLabel = gtk_label_new("显示栏目数");
        GtkWidget* slotsCombo = gtk_combo_box_text_new();
        for (int i = 1; i <= 4; i++) {
            char buf[10]; sprintf(buf, "%d 个", i);
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(slotsCombo), buf);
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(slotsCombo), g_settings.numSlots - 1);
        gtk_grid_attach(GTK_GRID(grid), slotsLabel, 0, 1, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), slotsCombo, 1, 1, 1, 1);

        GtkWidget* slotCombos[4];
        for (int i = 0; i < 4; i++) {
            char labelText[20]; sprintf(labelText, "栏目 %d", i + 1);
            GtkWidget* label = gtk_label_new(labelText);
            slotCombos[i] = gtk_combo_box_text_new();
            for (int j = 0; j < GetRealMetricCount(); j++) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(slotCombos[i]), GetMetricName((MetricType)j));
            gtk_combo_box_set_active(GTK_COMBO_BOX(slotCombos[i]), g_settings.slots[i]);
            gtk_grid_attach(GTK_GRID(grid), label, 0, 3 + i, 1, 1);
            gtk_grid_attach(GTK_GRID(grid), slotCombos[i], 1, 3 + i, 1, 1);
        }

        gtk_container_add(GTK_CONTAINER(content), grid);
        gtk_widget_show_all(dialog);

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
            int numSlots = gtk_combo_box_get_active(GTK_COMBO_BOX(slotsCombo)) + 1;
            int theme = gtk_combo_box_get_active(GTK_COMBO_BOX(themeCombo));
            int selections[4];
            for (int i = 0; i < 4; i++)
                selections[i] = gtk_combo_box_get_active(GTK_COMBO_BOX(slotCombos[i]));
            bool dup = false;
            for (int i = 0; i < numSlots && !dup; i++)
                for (int j = i + 1; j < numSlots; j++)
                    if (selections[i] == selections[j]) { dup = true; break; }
            if (dup) {
                GtkWidget* msg = gtk_message_dialog_new(GTK_WINDOW(dialog), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, "栏目不能选择相同的指标");
                gtk_dialog_run(GTK_DIALOG(msg));
                gtk_widget_destroy(msg);
            } else {
                g_settings.numSlots = numSlots;
                g_settings.theme = (ThemeType)theme;
                for (int i = 0; i < 4; i++) g_settings.slots[i] = (MetricType)selections[i];
                SaveSettings(g_settings);
                int newHeight = 12 + g_settings.numSlots * 44 + 10;
                gtk_window_resize(g_mainWindow, 200, newHeight);
            }
        }
        gtk_widget_destroy(dialog);
    }), NULL);

    g_signal_connect(quitItem, "activate", G_CALLBACK(+[](GtkMenuItem* item, gpointer data) {
        gtk_main_quit();
    }), NULL);

    gtk_widget_show_all(menu);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, button, activate_time);
}

static gboolean on_button_press(GtkWidget* widget, GdkEventButton* event, gpointer data) {
    if (event->button == 3) {
        GtkWidget* menu = gtk_menu_new();
        GtkWidget* settingsItem = gtk_menu_item_new_with_label("设置");
        GtkWidget* hideItem = gtk_menu_item_new_with_label("隐藏");
        GtkWidget* quitItem = gtk_menu_item_new_with_label("退出");
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), settingsItem);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), hideItem);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), quitItem);

        g_signal_connect(settingsItem, "activate", G_CALLBACK(+[](GtkMenuItem* item, gpointer data) {
            g_signal_emit_by_name(g_trayIcon, "popup-menu", 0, gtk_get_current_event_time());
        }), NULL);
        g_signal_connect(hideItem, "activate", G_CALLBACK(+[](GtkMenuItem* item, gpointer data) {
            gtk_widget_hide(GTK_WIDGET(g_mainWindow));
        }), NULL);
        g_signal_connect(quitItem, "activate", G_CALLBACK(+[](GtkMenuItem* item, gpointer data) {
            gtk_main_quit();
        }), NULL);

        gtk_widget_show_all(menu);
        gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);
    }
    return FALSE;
}

int main(int argc, char* argv[]) {
    gtk_init(&argc, &argv);
    InitSettings();
    LoadSettings(g_settings);

    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_mainWindow = GTK_WINDOW(window);
    gtk_window_set_title(GTK_WINDOW(window), "System Monitor");
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_window_set_keep_above(GTK_WINDOW(window), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_UTILITY);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(window), TRUE);

    int newHeight = 12 + g_settings.numSlots * 44 + 10;
    gtk_window_set_default_size(GTK_WINDOW(window), 200, newHeight);
    gtk_window_resize(GTK_WINDOW(window), 200, newHeight);

    GdkScreen* screen = gdk_screen_get_default();
    int screenWidth = gdk_screen_get_width(screen);
    gtk_window_move(GTK_WINDOW(window), screenWidth - 215, 15);

    gtk_widget_set_app_paintable(window, TRUE);
    g_signal_connect(window, "draw", G_CALLBACK(on_draw), NULL);
    g_signal_connect(window, "button-press-event", G_CALLBACK(on_button_press), NULL);
    gtk_widget_add_events(window, GDK_BUTTON_PRESS_MASK);

    g_trayIcon = gtk_status_icon_new_from_stock(GTK_STOCK_EXECUTE);
    gtk_status_icon_set_tooltip_text(g_trayIcon, "System Monitor");
    g_signal_connect(g_trayIcon, "activate", G_CALLBACK(on_tray_click), NULL);
    g_signal_connect(g_trayIcon, "popup-menu", G_CALLBACK(on_tray_menu), NULL);

    g_timeout_add_seconds(1, on_timer, window);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_widget_show_all(window);
    gtk_main();
    return 0;
}

#endif
