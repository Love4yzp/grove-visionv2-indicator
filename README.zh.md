# Grove Vision V2 AI 摄像头 + SenseCAP Indicator 集成方案

## 项目概述

本 Demo 演示如何将 **Grove Vision V2 AI 摄像头传感器** 作为 **SenseCAP Indicator** 的采集器，实现在 SenseCAP Indicator 上直接显示图像和检测数据，无需连接电脑。

> **重要说明**：由于 SenseCraft AI 版权冲突问题，默认姿态识别模型已被移除，无法通过官网直接烧录。但可以从 [GitHub 仓库](https://github.com/HimaxWiseEyePlus/Seeed_Grove_Vision_AI_Module_V2/tree/main) 获取相关模型文件。比如[姿态识别模型 - yolov8n_pose_256_vela_3_9_0x3BB000](https://github.com/HimaxWiseEyePlus/Seeed_Grove_Vision_AI_Module_V2/blob/main/model_zoo/tflm_yolov8_pose/yolov8n_pose_256_vela_3_9_0x3BB000.tflite)；通过[SenseCraft-Web-Toolkit](https://seeed-studio.github.io/SenseCraft-Web-Toolkit/#/tool/tool)将模型烧录到`0x400000`地址。

### 支持的模型显示

- [x] 人脸检测
- [x] 姿态识别
- [ ] 物体检测
- [ ] 其他 AI 视觉任务

## 技术架构

### 系统组成

- **SenseCAP Indicator**：主控设备（ESP32S3 + RP2040）
- **Grove Vision V2**：AI 视觉传感器
- **通信方式**：UART 串口通信

### 工作原理

为提高处理效率，采用分层架构：

- **RP2040**：预设模板程序，处理基础通信和数据预处理
- **ESP32S3**：负责界面显示和高级数据处理
- **Vision V2**：执行 AI 视觉检测，通过 AT 指令交互

### 备忘录

- 官方的 SSCMA Arduino 库中的串口没有为 RP2040 设定为 16*1024 的 SIZE 设置。

## 硬件连接方案

### 接线说明

**SenseCAP Indicator 端**：

- Grove 接口插入右侧 Grove 插槽
- 白线 → GPIO20（TX）
- 黄线 → GPIO21（RX）

**Grove Vision V2 端**：

- 白线 → RX（接收）
- 黄线 → TX（发送）
- 红线 → 3.3V（电源）
- 黑线 → GND（接地）

![连接示意图](https://files.seeedstudio.com/wiki/SenseCAP/SenseCAP_Indicator/new-grove.png)

## 软件烧录

### RP2040 固件烧录

1. **环境准备**
   - 在 Arduino IDE 中添加 RP2040 开发板支持
   - 参考：https://github.com/earlephilhower/arduino-pico

2. **固件烧录**
   - 下载 RP2040 专用固件
   - 通过 Arduino IDE 烧录到 RP2040

### ESP32S3 固件烧录

1. **环境要求**
   - 使用 ESP-IDF v5.1 版本（SenseCAP Indicator SDK 仅在此版本测试通过）

2. **烧录步骤**
   - 配置 ESP-IDF v5.1 开发环境
   - 编译并烧录 ESP32S3 固件

### Grove Vision V2 模型烧录

1. **获取模型文件**
   - 从 GitHub 仓库下载所需的 AI 模型
   - 选择适合的姿态识别或其他检测模型

2. **烧录方法**
   - 使用专用烧录工具
   - 按照仓库说明进行模型部署



## 使用说明

### 启动流程

1. 确保硬件连接正确
2. 上电启动系统
3. SenseCAP Indicator 自动识别 Vision V2
4. 开始图像采集和显示

### 操作界面

- 实时图像显示

## 注意事项

1. **电源要求**：确保 3.3V 供电稳定
2. **接线检查**：TX/RX 交叉连接，避免短路
3. **固件版本**：使用指定版本的开发环境
4. **模型兼容性**：确认 AI 模型与硬件兼容

## 故障排除

### 常见问题

- 通信异常：检查接线和波特率设置
- 图像不显示：确认 Vision V2 固件正常
- 检测精度低：尝试更换或重新训练模型