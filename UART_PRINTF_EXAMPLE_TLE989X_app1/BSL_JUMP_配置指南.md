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
   - 编译工程2得到 HEX 文件
   
   **重要：** 如果 APP2 的 scatter 文件已正确配置（`__NVM0_BASE = 0x11004000`），
   生成的 HEX 文件会自动包含 0x11004000 地址信息，Keil 可以直接烧录！

3. **在 Keil 中烧录 APP2：**
   
   **方法A：使用 Keil 直接烧录（推荐）**
   
   前提条件：APP2 的 scatter 文件必须配置为 `__NVM0_BASE = 0x11004000`
   
   步骤：
   1. 打开 APP2 工程
   2. 编译生成 HEX 文件（会包含 0x11004000 地址）
   3. 连接调试器（J-Link/ULink）
   4. 点击 "Download" 按钮（或 Flash -> Download）
   5. Keil 会自动将代码烧录到 HEX 文件中指定的地址（0x11004000）
   
   **验证烧录地址：**
   - 打开生成的 HEX 文件（文本编辑器）
   - 查看地址行（第二列），应该看到 "1004" 开头的地址
   - 例如：`:10400000...` 表示地址 0x11004000
   
   **方法B：使用 J-Link Commander 手动烧录**
   
   如果 HEX 文件地址不对，可以使用 J-Link Commander：
   ```
   J-Link> connect
   J-Link> loadfile app2.hex 0x11004000
   J-Link> verifybin app2.bin 0x11004000
   J-Link> exit
   ```
   
   **方法C：先擦除再分别烧录**
   
   1. 完全擦除 Flash
   2. 烧录 APP1（地址 0x11000000）
   3. 烧录 APP2（地址 0x11004000）

4. **完整测试：**
   - 复位芯片
   - 应该看到 APP1 启动后跳转到 APP2
   - APP2 启动后跳转回 APP1
   - 循环往复

#### 方法2：合并 HEX 文件（适用于量产）

**注意：** 只有当 APP2 的 HEX 文件仍从 0x11000000 开始时，才需要使用 offset 参数。

**情况1：APP2 已配置为 0x11004000（推荐）**
直接合并即可：
```bash
srec_cat app1.hex -Intel app2.hex -Intel -o combined.hex -Intel
```

**情况2：APP2 仍从 0x11000000 开始（需要偏移）**
使用 offset 参数：
```bash
srec_cat app1.hex -Intel app2.hex -Intel -offset 0x4000 -o combined.hex -Intel
```

然后在 Keil 中烧录 combined.hex

#### 方法3：使用 Keil 的 Multi-Project Workspace（适用于开发）

1. 创建一个 Workspace 包含 APP1 和 APP2 工程
2. 分别编译两个工程
3. 使用 Keil 的批处理下载功能
4. 在 Flash Download 设置中添加两个 HEX 文件

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
**症状：** 看到 "Jump failed!" 信息或错误代码 0

**可能原因：**
- APP2 未正确烧录到 0x11004000 位置
- APP2 的中断向量表不正确
- APP2 的 scatter 文件配置错误
- 地址偏移计算错误

**解决方案：**

**步骤1：验证 APP2 是否已烧录**
在 Keil 调试器中：
1. 启动调试会话
2. 打开 Memory Window（View -> Memory Windows -> Memory 1）
3. 输入地址 `0x11004100`（APP2 向量表位置）
4. 检查内容：
   - 如果全是 `FF FF FF FF`，说明 APP2 未烧录
   - 如果看到有效数据（如 `00 7C 00 18`），说明 APP2 已烧录

**步骤2：验证 APP2 的 HEX 文件地址**
1. 用文本编辑器打开 APP2 的 HEX 文件
2. 查看第一行数据行的地址字段
3. 应该看到类似 `:02000004110` 的扩展地址记录（表示 0x1100xxxx）
4. 然后是 `:10400000...` 或 `:10401000...` 表示 0x11004xxx

**步骤3：检查 scatter 文件**
确认 APP2 的 `tle9883_93.sct` 中：
```c
#define __NVM0_BASE      0x11004000  // 必须是这个地址！
```

**步骤4：重新编译和烧录**
如果地址不对，修改后重新编译 APP2 并烧录

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
