# ESP White Knob

ESP White Knob 是一套基于 ESP32-S3 的智能旋钮工程，包含圆形 LCD 界面、磁编码器角度采集、电机力反馈、WS2812 灯效、压力/按键输入、Wi-Fi 相关功能以及 BLE HID Surface Dial 发送逻辑。

项目使用 ESP-IDF 构建，当前配置目标芯片为 `esp32s3`，Flash 配置为 `16MB`。

## 功能概览

- 圆形 LCD UI：包含菜单、时钟、Surface Dial、百分比、番茄钟、灯光、Wi-Fi、时间设置等界面。
- 智能旋钮控制：通过 AS5047P 磁编码器读取角度，并结合电机控制实现旋钮手感/震动反馈。
- BLE HID Surface Dial：进入 Dial 界面后初始化蓝牙 HID，可发送按下、释放和旋转指令。
- WS2812 灯效：支持 16 颗 WS2812 LED 装饰灯。
- Wi-Fi 与网页资源：包含 AP/Wi-Fi 管理模块和 `html` SPIFFS 分区。
- 时间功能：包含本地计时和 SNTP 相关模块。

## 硬件配置

主要硬件接口定义在 `main/config/pin_config.h`。

| 模块 | 引脚/配置 |
| --- | --- |
| LCD SPI Host | `SPI2_HOST` |
| LCD CLK | `GPIO11` |
| LCD MOSI | `GPIO12` |
| LCD DC | `GPIO10` |
| LCD CS | `GPIO9` |
| LCD RST | `GPIO13` |
| LCD Backlight | `GPIO14` |
| AS5047P SPI Host | `SPI3_HOST` |
| AS5047P CLK | `GPIO39` |
| AS5047P MOSI | `GPIO48` |
| AS5047P MISO | `GPIO45` |
| AS5047P SS | `GPIO40` |
| WS2812 DIN | `GPIO1` |
| WS2812 LED 数量 | `16` |
| TMC6300 IN1 | `GPIO17` |
| TMC6300 IN2 | `GPIO16` |
| TMC6300 IN3 | `GPIO15` |
| HX711 SCK | `GPIO47` |
| HX711 DO | `GPIO21` |
| 电源按键 | `GPIO5` |
| 电池 ADC | `GPIO4` |

## 软件环境

推荐环境：

- ESP-IDF `5.3.3`，以 `dependencies.lock` 中记录的版本为准。
- 目标芯片：`esp32s3`
- 分区表：自定义 `my_partitions.csv`

依赖组件：

- `espressif/esp_tinyusb`
- `espressif/tinyusb`
- 本仓库内置 `lvgl`、`esp_rlottie`、`bsp` 等组件源码。

## 目录结构

```text
.
├── CMakeLists.txt
├── dependencies.lock
├── my_partitions.csv
├── sdkconfig
├── components/
│   ├── bsp/
│   ├── esp_rlottie/
│   ├── espressif__tinyusb/
│   └── lvgl/
├── managed_components/
│   └── espressif__esp_tinyusb/
└── main/
    ├── White_Knob.c
    ├── app_core/
    ├── config/
    └── drivers/
```

关键模块：

- `main/White_Knob.c`：应用入口，初始化 NVS、时间、UI 管理器，并创建 UI、智能旋钮、LED、Dial 任务。
- `main/app_core/smart_knob/`：智能旋钮任务与电机手感逻辑。
- `main/app_core/dial/`：Surface Dial 指令队列与 BLE HID 发送任务。
- `main/drivers/encoder/`：AS5047P 磁编码器驱动。
- `main/drivers/motor/`：电机、SVPWM、PID 控制相关代码。
- `main/drivers/lcd/`：LCD、LVGL 移植、界面管理与生成的 UI 文件。
- `main/drivers/ble_hid/`：BLE HID 设备实现。
- `main/drivers/wifi/`：Wi-Fi/AP/Web Server 相关代码。
- `main/drivers/ws2812/`：WS2812 灯带控制。

## 构建与烧录

先进入 ESP-IDF 环境，再执行：

```powershell
idf.py set-target esp32s3
idf.py build
```

烧录和串口监视：

```powershell
idf.py -p COMx flash monitor
```

其中 `COMx` 替换为实际串口号。

如需清理构建产物：

```powershell
idf.py fullclean
```

## 分区说明

分区表位于 `my_partitions.csv`：

| 分区 | 类型 | 大小 | 用途 |
| --- | --- | --- | --- |
| `nvs` | data/nvs | `0x6000` | NVS 参数存储 |
| `phy_init` | data/phy | `0x1000` | PHY 初始化数据 |
| `factory` | app/factory | `4M` | 主应用固件 |
| `others` | data/spiffs | `0x20000` | SPIFFS 数据 |
| `html` | data/spiffs | `32k` | Web 页面资源 |

##项目产品渲染图
<img width="527" height="518" alt="0de61b77d64c09d7295239678390eb40" src="https://github.com/user-attachments/assets/f5c00887-268a-46f8-97d5-278bfe462aaa" />

