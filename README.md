# FloatMonitor - 系统监控悬浮窗

轻量级系统监控悬浮窗，实时显示 CPU、内存、GPU、磁盘、温度等指标，支持曲线图、多主题、背景图片。

## 功能特性

- **实时监控**：CPU 使用率、内存占用、GPU 使用率、磁盘活动率、交换区使用率
- **温度监控**：CPU/GPU 温度（PDH / WMI / NVML / ADL 多层回退检测）
- **型号显示**：CPU/GPU 型号名称
- **历史曲线图**：CPU 使用率 2 分钟历史折线图，渐变填充 + 发光线条
- **动态磁盘检测**：自动检测系统盘符，支持热插拔
- **多主题**：深色 / 浅色 / 蓝色 / 紫色 4 种主题
- **自定义栏目**：1~4 个显示栏目，可选任意指标
- **背景图片**：支持 JPG/PNG 背景图片
- **系统托盘**：最小化到托盘，右键菜单
- **启动动画**：Gemini 风格旋转十字动画
- **跨平台**：Windows (Win32) / Linux (GTK3) / macOS (GTK3)

## 截图

![FloatMonitor](https://via.placeholder.com/220x300?text=FloatMonitor+Screenshot)

## 编译

### Windows (MinGW)

```bash
g++ -o float_monitor.exe float_monitor.cpp -lgdi32 -lshell32 -lpdh -ladvapi32 -lcomdlg32 -lmsimg32 -lgdiplus -lole32 -loleaut32 -lwbemuuid -mwindows -std=c++17 -O2
```

### Linux

```bash
g++ -o float_monitor float_monitor.cpp `pkg-config --cflags --libs gtk+-3.0` -std=c++17 -O2
```

### macOS

```bash
g++ -o float_monitor float_monitor.cpp `pkg-config --cflags --libs gtk+-3.0` -std=c++17 -O2
```

## 使用方法

1. 运行程序，悬浮窗出现在屏幕右上角
2. 右键悬浮窗打开菜单：
   - **设置**：选择主题、栏目数量、显示指标、背景图片
   - **隐藏**：最小化到系统托盘
   - **完全退出**：关闭程序

## 可选指标

| 指标 | 说明 |
|------|------|
| CPU | CPU 使用率 |
| MEM | 内存使用率 |
| GPU | GPU 使用率（支持集显/独显） |
| DISK | 所有磁盘活动率 |
| SWAP | 交换区使用率 |
| CPU Temp | CPU 温度 |
| GPU Temp | GPU 温度 |
| 磁盘 X: | 单个磁盘活动率（自动检测盘符） |

## 温度检测说明

温度检测采用多层回退策略：

1. **CPU 温度**：PDH Thermal Zone → WMI MSAcpi_ThermalZoneTemperature → WMI Win32_TemperatureProbe
2. **GPU 温度**：NVIDIA (nvml.dll) → AMD (atiadlxx.dll)

> 部分系统可能需要管理员权限才能读取温度传感器。

## 依赖

### Windows
- Windows 10/11
- 无额外运行时依赖

### Linux / macOS
- GTK3
- Pango
- Cairo

## 许可证

MIT License
