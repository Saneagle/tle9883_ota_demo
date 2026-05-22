# 快速开始 - UART固件升级测试

## 5分钟快速测试

### 步骤1: 编译工程

1. 打开 Keil/IAR 工程
2. 确保以下文件已添加到工程：
   - `app/uart_upgrade.c`
   - `app/uart_upgrade.h`
   - `app/main.c` (已修改)
3. 编译工程，确保没有错误

### 步骤2: 烧录APP1

使用调试器将编译好的APP1烧录到芯片：
- APP1起始地址: `0x11000000`

### 步骤3: 连接串口

1. 确认串口号（设备管理器）
2. 波特率：115200
3. 数据位：8，停止位：1，无校验

### 步骤4: 准备测试固件

为了快速测试，你可以：

#### 选项A：使用当前固件作为测试

```bash
# 复制当前编译的bin文件作为测试固件
copy Objects\*.bin test_firmware.bin
```

#### 选项B：创建简单测试固件

修改 `main.c` 中的 `APP_VERSION` 为 2，重新编译生成APP2固件。

### 步骤5: 运行升级工具

```bash
# 安装依赖（首次运行）
pip install pyserial

# 运行升级
python uart_upgrade_tool.py -p COM3 -f test_firmware.bin
```

替换 `COM3` 为你的实际串口号。

### 步骤6: 观察结果

你应该看到类似以下输出：

```
串口 COM3 已打开 (波特率: 115200)
固件文件: test_firmware.bin
固件大小: 8192 字节
固件CRC16: 0xABCD

开始升级会话...
升级会话已启动

开始传输数据 (共 64 个数据包)...
发送数据包 1/64 (128/8192 字节, 1%) - 成功
发送数据包 2/64 (256/8192 字节, 3%) - 成功
...
发送数据包 64/64 (8192/8192 字节, 100%) - 成功

结束升级会话...
升级完成！

固件升级完成！设备将在3秒后跳转到新应用程序。
```

## 常见问题快速解决

### 问题1: 串口打开失败

```bash
# 检查串口是否正确
# Windows PowerShell:
Get-WmiObject Win32_SerialPort | Select-Object Name, DeviceID

# 或者使用模式匹配
[System.IO.Ports.SerialPort]::getportnames()
```

### 问题2: 无法找到 pyserial

```bash
# 确保使用正确的pip
python -m pip install pyserial

# 或者使用国内镜像加速
pip install pyserial -i https://pypi.tuna.tsinghua.edu.cn/simple
```

### 问题3: 升级卡住不动

1. 按 `Ctrl+C` 中断
2. 重启设备
3. 检查串口连接
4. 降低波特率重试：

```bash
python uart_upgrade_tool.py -p COM3 -f test_firmware.bin -b 9600
```

## 验证升级成功

使用串口监视器（如PuTTY、Tera Term）连接到设备：

1. 升级前应该看到：
   ```
   ========================================
     Application 1 - UART Firmware Upgrade
   ========================================
   This is Application 1 running...
   ```

2. 升级成功后（如果APP2版本不同）应该看到：
   ```
   ========================================
     Application 2 - UART Firmware Upgrade
   ========================================
   This is Application 2 running...
   ```

## 下一步

测试成功后，你可以：

1. 修改APP2代码实现不同功能
2. 添加版本校验逻辑
3. 实现A/B双分区切换
4. 添加固件签名验证
5. 集成到你的产品中

详细文档请查看 [README_UART_UPGRADE.md](README_UART_UPGRADE.md)

---

## 完整示例：端到端测试

### 1. 准备两个不同版本

**APP1 (main.c)**:
```c
#define APP_VERSION     1
```

**APP2 (main.c)**:
```c
#define APP_VERSION     2
```

### 2. 编译并烧录APP1

编译APP1，烧录到 `0x11000000`

### 3. 编译APP2生成bin文件

编译APP2，找到生成的 `.bin` 文件（通常在 `Objects/` 目录）

### 4. 使用APP1升级到APP2

```bash
python uart_upgrade_tool.py -p COM3 -f Objects/APP2.bin
```

### 5. 观察版本切换

升级完成后，设备自动从APP1跳转到APP2，串口输出会显示版本变化。

---

**准备好了吗？开始测试吧！** 🎯
