# UART 固件升级使用指南

## 概述

本项目实现了通过UART接口升级TLE989X芯片固件的完整方案。支持在APP1运行时通过UART接收新的APP2固件并自动跳转。

## 架构说明

### 系统组成

```
┌─────────────────────────────────────────┐
│         TLE989X Flash Memory            │
│                                         │
│  0x11000000  ┌──────────────────┐      │
│              │  Boot Config     │      │
│  0x11000100  ├──────────────────┤      │
│              │  APP1 (16KB)     │ ◄─── 当前运行
│              │  - 接收升级固件   │      │
│              │  - 写入Flash     │      │
│  0x11004000  ├──────────────────┤      │
│              │  APP2 (16KB)     │ ◄─── 升级目标
│              │  - 新固件写入此处 │      │
│  0x11008000  └──────────────────┘      │
│                                         │
└─────────────────────────────────────────┘
```

### 升级流程

```
[PC端]                    [UART]                  [TLE989X/APP1]
   │                        │                          │
   │  1. START_UPGRADE      │                          │
   ├───────────────────────>│                          │
   │      (固件大小)         │   启动升级会话           │
   │                        ├─────────────────────────>│
   │                        │                          │ 擦除Flash
   │                        │         ACK_OK           │
   │  <─────────────────────┼──────────────────────────┤
   │                        │                          │
   │  2. DATA_PACKET (0)    │                          │
   ├───────────────────────>│                          │
   │      (128 bytes)       │   写入Flash缓冲区         │
   │                        ├─────────────────────────>│
   │                        │         ACK_OK           │ 写入Flash
   │  <─────────────────────┼──────────────────────────┤
   │                        │                          │
   │  3. DATA_PACKET (1)    │                          │
   ├───────────────────────>│                          │
   │         ...            │         ...              │
   │                        │                          │
   │  N. END_UPGRADE        │                          │
   ├───────────────────────>│                          │
   │      (CRC16)           │   验证CRC                │
   │                        ├─────────────────────────>│
   │                        │         ACK_OK           │ CRC通过
   │  <─────────────────────┼──────────────────────────┤
   │                        │                          │
   │                        │                          │ 跳转到APP2
   │                        │                          └──────>
```

## 协议定义

### 帧格式

所有通信都使用以下帧格式：

```
┌────────┬────────┬────────┬────────┬────────────┬────────┐
│ Header │  CMD   │ Length │  SEQ   │    Data    │  CRC16 │
│ (1B)   │ (1B)   │ (2B)   │ (1B)   │  (0-128B)  │  (2B)  │
└────────┴────────┴────────┴────────┴────────────┴────────┘
```

- **Header**: 固定为 `0x5A`
- **CMD**: 命令码（见下表）
- **Length**: 数据部分长度（大端格式）
- **SEQ**: 序号（用于数据包，从0开始递增）
- **Data**: 数据部分（可选）
- **CRC16**: CRC16校验（MODBUS标准，覆盖Header到Data）

### 命令定义

| 命令码 | 名称              | 数据格式           | 说明                     |
|--------|-------------------|--------------------|--------------------------|
| 0x01   | START_UPGRADE     | 4字节固件大小      | 开始升级会话             |
| 0x02   | DATA_PACKET       | 固件数据(≤128B)    | 发送固件数据包           |
| 0x03   | END_UPGRADE       | 2字节CRC16         | 结束升级会话并验证       |
| 0x04   | QUERY_STATUS      | 无                 | 查询升级状态             |
| 0x05   | ABORT_UPGRADE     | 无                 | 中止升级                 |

### 应答码

| 应答码 | 名称       | 说明                |
|--------|------------|---------------------|
| 0xAA   | ACK_OK     | 操作成功            |
| 0x55   | ACK_ERROR  | 操作失败            |
| 0x33   | ACK_BUSY   | 设备忙碌            |
| 0x66   | ACK_CRC    | CRC校验错误         |

### CRC16 计算

使用MODBUS CRC16算法：

```python
def crc16(data):
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc
```

## 使用步骤

### 1. 准备固件文件

确保你有APP2的固件二进制文件（.bin格式），大小不超过16KB。

如果你有.hex文件，可以使用以下工具转换：

```bash
# 使用 objcopy (ARM toolchain)
arm-none-eabi-objcopy -I ihex -O binary app2.hex app2.bin

# 或使用 srec_cat
srec_cat app2.hex -Intel -o app2.bin -Binary
```

### 2. 安装Python依赖

```bash
pip install pyserial
```

### 3. 连接硬件

1. 将TLE989X开发板通过USB连接到PC
2. 确认UART引脚配置：
   - UART TX (P1.1) - 调试输出
   - UART RX (P1.2) - 接收升级数据
3. 记录串口号（Windows: COM3, Linux: /dev/ttyUSB0）

### 4. 烧录APP1

使用Keil/IAR或调试器烧录当前工程（包含升级功能的APP1）到芯片。

### 5. 执行升级

运行Python升级工具：

```bash
# Windows
python uart_upgrade_tool.py -p COM3 -f app2.bin

# Linux
python uart_upgrade_tool.py -p /dev/ttyUSB0 -f app2.bin

# 指定波特率
python uart_upgrade_tool.py -p COM3 -f app2.bin -b 115200
```

### 6. 观察升级过程

升级工具会显示以下信息：

```
串口 COM3 已打开 (波特率: 115200)
固件文件: app2.bin
固件大小: 4096 字节
固件CRC16: 0x1234

开始升级会话 (固件大小: 4096 字节)...
升级会话已启动

开始传输数据 (共 32 个数据包)...
发送数据包 1/32 (128/4096 字节, 3%) - 成功
发送数据包 2/32 (256/4096 字节, 6%) - 成功
...
发送数据包 32/32 (4096/4096 字节, 100%) - 成功

结束升级会话 (CRC: 0x1234)...
升级完成！

固件升级完成！设备将在3秒后跳转到新应用程序。
```

### 7. 验证结果

升级完成后，设备会自动跳转到APP2。你可以通过串口监视器观察APP2的启动信息。

## 故障排查

### 常见错误

#### 1. "串口打开失败"

**原因**: 串口被占用或不存在

**解决方法**:
- 检查设备管理器确认串口号
- 关闭其他占用串口的程序（如串口监视器）
- 检查USB连接

#### 2. "应答超时"

**原因**: 设备未响应或波特率不匹配

**解决方法**:
- 确认设备正常运行（观察串口输出）
- 检查波特率设置是否一致（默认115200）
- 检查UART接线

#### 3. "CRC校验错误"

**原因**: 数据传输错误

**解决方法**:
- 检查串口连接质量
- 降低波特率重试
- 检查USB线缆质量

#### 4. "Flash写入失败"

**原因**: Flash保护或地址错误

**解决方法**:
- 确认APP2地址配置正确（0x11004000）
- 检查Flash保护设置
- 查看错误日志获取详细信息

### 调试技巧

#### 使能详细日志

在 `uart_upgrade.c` 中，你可以添加更多调试输出：

```c
#define DEBUG_UPGRADE  1  // 添加到文件顶部

#if DEBUG_UPGRADE
    printf("DEBUG: Write offset=0x%08X, len=%u\r\n", offset, length);
#endif
```

#### 监控UART通信

使用串口监视器同时监听TX和RX：
- TX (P1.1): 设备输出日志
- RX (P1.2): 升级数据输入

#### 手动测试帧

你可以手动发送测试帧来验证协议：

```python
# 简单的查询状态命令
frame = bytes([0x5A, 0x04, 0x00, 0x00, 0x00])
crc = crc16(frame)
frame += struct.pack('>H', crc)
ser.write(frame)
```

## 扩展功能

### 1. 添加身份验证

在 `CMD_START_UPGRADE` 命令中添加密码验证：

```c
if (password != EXPECTED_PASSWORD) {
    UART_Upgrade_SendAck(ACK_ERROR);
    return -1;
}
```

### 2. 支持增量升级

只传输变化的部分，而不是整个固件。

### 3. 支持A/B分区

实现双份固件，升级失败可回滚。

### 4. 添加签名验证

使用加密签名验证固件完整性和来源。

## 项目文件说明

```
app/
├── main.c              # 主程序（集成升级功能）
├── app_jump.c/h        # 应用跳转功能
├── uart_upgrade.c/h    # UART升级协议实现
└── write.c             # UART写入函数

uart_upgrade_tool.py    # PC端升级工具
README_UART_UPGRADE.md  # 本文档
```

## 技术细节

### Flash编程

- **页大小**: 128字节
- **扇区大小**: 通常2KB或4KB（取决于芯片型号）
- **写入前必须擦除**: Flash只能从1写为0，需要先擦除（全FF）
- **写保护**: 使用 `user_nvm_ucode_temp_protect_clear/set` 控制

### 内存布局

```
APP1: 0x11000000 - 0x11003FFF (16KB)
APP2: 0x12002000 - 0x11005FFF (16KB)
```

### 跳转机制

跳转使用Infineon BootROM API：
- 重置NVIC中断
- 切换向量表（VTOR）
- 重载栈指针（MSP）
- 跳转到新的Reset_Handler

## 参考资源

- [TLE989X数据手册](https://www.infineon.com/tle989x)
- [BootROM API参考](RTE/Device/TLE9893_2QKW62S/bootrom.h)
- [MODBUS CRC16](https://www.modbustools.com/modbus.html)

## 常见问题 (FAQ)

**Q: 升级失败会导致设备变砖吗？**

A: 不会。APP1始终保持不变，即使APP2升级失败，设备也可以继续运行APP1。

**Q: 可以通过其他接口（如SPI、CAN）升级吗？**

A: 可以。只需修改底层传输部分，协议层保持不变。

**Q: 升级速度有多快？**

A: 以115200波特率计算，升级16KB固件约需10-15秒。

**Q: 是否支持断点续传？**

A: 当前版本不支持。升级中断后需要重新开始。可以通过添加序号检查实现此功能。

## 版本历史

- **v1.0** (2024): 初始版本，支持基本UART升级功能
  - START/DATA/END命令
  - CRC16校验
  - Flash编程
  - 自动跳转

## 许可证

Copyright (c) 2020-2024, Infineon Technologies AG  
All rights reserved.

---

**祝你升级顺利！** 🚀

如有问题，请查看故障排查章节或联系技术支持。
