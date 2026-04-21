# LYF — 智能骑行码表（硬件 + 固件）

本仓库仅收录桌面 `LYF` 工程下的 **`PCB/`** 与 **`Program/`** 两部分（同级的其他个人目录已通过 `.gitignore` 排除）。

在线仓库：<https://github.com/huaixushiliu16/LYF-smart-cycling-computer>

内容包含 **PCB 资料/实物照片** 与 **ESP32-S3 固件工程**，面向一款带触摸屏的骑行码表类设备：BLE 心率/CSCS、速度/踏频、IMU、RGB 状态灯、蜂鸣器等，界面基于 **LVGL**。

## 目录说明

| 路径 | 内容 |
|------|------|
| `PCB/` | 电路板相关图片与资料（如实物照、布线截图等，便于归档与分享） |
| `Program/mabiao/` | **ESP-IDF** 工程（CMake 项目名 `SSS`，目标芯片 **ESP32-S3**） |

固件入口与整体说明见 `Program/mabiao/main/main.c`（智能骑行码表主程序，含 BLE 调试与核心模式等）。

## 固件环境与编译

- **ESP-IDF**：建议与本机锁定的依赖一致，见 `Program/mabiao/dependencies.lock`（当前记录为 **IDF 5.3.1**）。
- **组件**：使用乐鑫组件管理器；克隆后会在 `managed_components/` 中自动拉取依赖（该目录已被忽略，勿手动提交）。
- **配置**：工程内 `.gitignore` 忽略了本地生成的 `sdkconfig`；首次克隆后请执行 `idf.py menuconfig` 按需配置，或依赖 `sdkconfig.defaults` 的默认项。

在仓库根目录下：

```bash
cd Program/mabiao
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

将 `PORT` 替换为实际串口（Windows 上常为 `COMx`）。

## 不上传到 Git 的内容说明

为减小体积、避免泄露本机路径，以下内容通过 `.gitignore` 排除（与 ESP-IDF 常见实践一致）：

- 编译产物目录 `build/`、中间目标与 map 等
- `managed_components/`（由 `idf.py` / 组件管理器重新生成）
- 本地 `sdkconfig`、`sdkconfig.old`
- `.vscode`、`.cursor`、`.clangd` 等编辑器/语言服务器缓存
- `DOC/` 下以 `cursor_` 开头的临时导出文档
- `components/LovyanGFX/` 下的 `examples/`、`examples_for_PC/`、`examples_for_picosdk/`、`.github/`（上游库自带演示与 CI，不参与本设备固件编译）

若你本地曾单独在 `mabiao` 里初始化过 Git，已改为在 **仓库根目录** 统一维护一个远程仓库，便于同时包含 `PCB` 与 `Program`。

## 许可证

若需对外发布，请在根目录补充 `LICENSE` 并在此更新说明。
