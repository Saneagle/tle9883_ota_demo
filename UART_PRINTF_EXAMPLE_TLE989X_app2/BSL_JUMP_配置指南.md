# User BSL 双引导跳转实验 - 配置指南

## 概述
本指南说明如何配置和测试 TLE989X 芯片的双引导（Dual Boot）功能，实现工程1和工程2之间的跳转。

## 系统架构

```
+----------------------------------+
|   NVM0 Flash (UBSL Segment)     |
|                                  |
|  0x11000000  +---------------+   |
|              |  Boot Config  |   |
|  0x11000100  +---------------+   |
|              |   Vector Table|   |
|              |   (APP1)      |   |
|              |               |   |
|              |   工程1代码    |   |
|              |   (16KB)      |   |
|  0x11004000  +---------------+   |
|              |   Vector Table|   |
|              |   (APP2)      |   |
|              |               |   |
|              |   工程2代码    |   |
|              |   (16KB)      |   |
|  0x11008000  +---------------+   |
|              |    未使用      |   |
+----------------------------------+
```

## 配置步骤

### 1. 配置工程1（当前工程）

#### 1.1 修改链接脚本 (tle9883_93.sct)
确保 UBSL 大小足够容纳两个应用程序：

```c
// 在 tle9883_93.sct 中修改
#define __NVM0_SIZE      0x8000   // 32KB，可容纳两个16KB的应用
```

#### 1.2 确认 main.c 中的版本号
```c
#define APP_VERSION     1    // 工程1保持为1
```

#### 1.3 添加源文件到工程
在 Keil/IAR 工程中添加：
- `app/app_jump.c`
- `app/app_jump.h`

#### 1.4 编译工程1
- 编译并生成 hex/bin 文件
- 记录工程1的大小（应小于16KB）

### 2. 创建和配置工程2

#### 2.1 复制工程
1. 复制整个工程文件夹到新位置（如 UART_PRINTF_EXAMPLE_TLE989X_APP2）
2. 重命名工程文件

#### 2.2 修改链接脚本起始地址
在工程2的 `tle9883_93.sct` 中修改：

```c
// 将 UBSL 起始地址偏移16KB
#define __NVM0_BASE      0x11004000  // 原来是 0x11000000
#define __NVM0_SIZE      0x4000      // 16KB
```

**重要提示：** 由于 scatter 文件的限制，可能需要使用不同的方法：

**方法A：使用预链接工具**
创建一个后处理脚本，将编译好的 APP2 合并到 APP1 的 hex 文件中的正确位置。

**方法B：使用 bootloader 方式（推荐）**
1. APP1 使用完整的 UBSL 段（从 0x11000000 开始）
2. 通过编程器或 bootloader 将 APP2 烧录到 0x11004000 位置

#### 2.3 修改 main.c 版本号
```c
#define APP_VERSION     2    // 工程2设为2
```

#### 2.4 编译工程2

### 3. 烧录和测试

#### 方法1：使用调试器分别烧录（推荐用于初次测试）

1. **仅烧录工程1进行单独测试：**
   ```
   - 烧录工程1到芯片
   - 观察串口输出，确认工程1运行
   - 此时跳转会失败（因为APP2还未烧录）
   ```

2. **准备工程2的镜像：**
   - 编译工程2得到 bin 文件
   - 使用 hex 编辑器或编程器将 APP2 烧录到 0x11004000

3. **完整测试：**
   - 复位芯片
   - 应该看到 APP1 启动后跳转到 APP2
   - APP2 启动后跳转回 APP1
   - 循环往复

#### 方法2：合并 HEX 文件

使用工具（如 srec_cat）合并两个 hex 文件：

```bash
srec_cat app1.hex -Intel app2.hex -Intel -offset 0x4000 -o combined.hex -Intel
```

然后烧录 combined.hex

## 预期输出

### APP1 运行时串口输出：
```
========================================
  Application 1 - BSL Jump Test
========================================
This is Application 1 running...
APP1 will jump to APP2 in 5000 ms

APP1 running... 1 sec
APP1 running... 2 sec
APP1 running... 3 sec
APP1 running... 4 sec
APP1 running... 5 sec

Time to jump!
Preparing to jump to APP2...
APP2 Address Offset: 0x00004000
```

### APP2 运行时串口输出：
```
========================================
  Application 2 - BSL Jump Test
========================================
This is Application 2 running...
APP2 will jump to APP1 in 5000 ms

APP2 running... 1 sec
APP2 running... 2 sec
APP2 running... 3 sec
APP2 running... 4 sec
APP2 running... 5 sec

Time to jump!
Preparing to jump to APP1...
APP1 Address Offset: 0x00000000
```

## 故障排除

### 1. 跳转失败
**症状：** 看到 "Jump failed!" 信息

**可能原因：**
- APP2 未正确烧录到 0x11004000 位置
- APP2 的中断向量表不正确
- 地址偏移计算错误

**解决方案：**
- 使用调试器检查 0x11004000 处的内容
- 确认 user_secure_dualboot 的偏移参数正确
- 检查链接脚本配置

### 2. 系统重启而非跳转
**症状：** 每次都从 APP1 开始（而非 APP2）

**可能原因：**
- user_secure_dualboot 内部执行了复位
- 需要在 NVM 中配置启动选项

**解决方案：**
- 查看 TLE989X 用户手册关于双引导配置的章节
- 可能需要配置 Boot Configuration 字段

### 3. 编译错误
**症状：** 找不到 app_jump.h 或 user_secure_dualboot 未定义

**解决方案：**
- 确认 app_jump.c 已添加到工程
- 确认包含路径正确
- 确认 bootrom.h 已正确包含

## 进阶优化

### 1. 添加按键触发跳转
可以修改代码，通过 GPIO 按键触发跳转而非自动触发：

```c
// 添加 GPIO 检测
if (GPIO_ReadButton())  // 假设有按键检测函数
{
    Jump_To_App2();
}
```

### 2. 添加 NVM 参数保存
在跳转前保存参数到 NVM，跳转后读取：

```c
// 跳转前
NVM_WriteParameter(param);
Jump_To_App2();

// APP2 启动后
param = NVM_ReadParameter();
```

### 3. 实现真正的 Bootloader
- APP1 作为 Bootloader，通过 UART 接收固件
- 将新固件写入 APP2 位置
- 跳转到 APP2 执行
- APP2 可以跳回 APP1 进行固件更新

## 相关 API 参考

### user_secure_dualboot()
```c
int32_t user_secure_dualboot(uint32_t image_offset);
```
- **功能：** 跳转到 UBSL 段中的另一个应用程序
- **参数：** image_offset - 相对于 UBSL 起始地址（0x11000000）的偏移量
- **返回：** 错误代码，成功则不返回
- **注意：** 该函数会重新初始化系统并跳转到新的中断向量表

## 参考文档
- TLE989X 用户手册 - Bootloader 章节
- TLE989X RTE 文档 - BOOTROM API
- 应用笔记：Implementing a User Bootloader on TLE989X

## 联系支持
如遇到问题，请查阅 Infineon 官方技术支持文档或论坛。
