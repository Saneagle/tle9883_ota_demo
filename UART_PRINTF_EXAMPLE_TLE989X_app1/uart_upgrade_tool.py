#!/usr/bin/env python3
"""
UART固件升级工具
用于通过UART协议向TLE989X芯片发送固件升级

使用方法:
    python uart_upgrade_tool.py -p COM3 -f firmware.bin
    
协议格式:
    帧头(1) + 命令(1) + 长度(2) + 序号(1) + 数据(N) + CRC16(2)
"""

import serial
import struct
import time
import argparse
import os
import sys

# 命令定义
CMD_START_UPGRADE = 0x01
CMD_DATA_PACKET = 0x02
CMD_END_UPGRADE = 0x03
CMD_QUERY_STATUS = 0x04
CMD_ABORT_UPGRADE = 0x05
CMD_ERASE_FLASH = 0x06

# 应答定义
ACK_OK = 0xAA
ACK_ERROR = 0x55
ACK_BUSY = 0x33
ACK_CRC_ERROR = 0x66

# 帧头
FRAME_HEADER = 0x5A

# 数据包大小
MAX_PACKET_SIZE = 128


class UARTUpgrader:
    """UART固件升级器类"""
    
    def __init__(self, port, baudrate=115200, timeout=2, debug=False, retry_count=3, packet_delay=0.01):
        """
        初始化升级器
        
        Args:
            port: 串口号（如 'COM3' 或 '/dev/ttyUSB0'）
            baudrate: 波特率
            timeout: 超时时间（秒）
            debug: 是否显示调试信息
            retry_count: 失败重试次数
            packet_delay: 数据包之间的延时（秒）
        """
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.debug = debug
        self.retry_count = retry_count
        self.packet_delay = packet_delay
        self.ser = None
        
    def open(self):
        """打开串口"""
        try:
            self.ser = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=self.timeout
            )
            print(f"串口 {self.port} 已打开 (波特率: {self.baudrate})")
            
            # 等待设备启动并清空缓冲区
            print("等待设备初始化...")
            time.sleep(1.0)  # 给设备更多启动时间
            
            # 读取并显示设备启动时的调试信息
            if self.ser.in_waiting > 0:
                startup_data = self.ser.read(self.ser.in_waiting)
                print(f"[设备启动信息] 收到 {len(startup_data)} 字节")
                if self.debug:
                    try:
                        text = startup_data.decode('ascii', errors='ignore')
                        if text.strip():
                            print(f"  内容: {text}")
                    except:
                        print(f"  HEX: {startup_data.hex()}")
            
            # 清空输入输出缓冲区，确保干净的起始状态
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()
            print("串口缓冲区已清空，准备通信\n")
            
            return True
        except Exception as e:
            print(f"打开串口失败: {e}")
            return False
    
    def close(self):
        """关闭串口"""
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("串口已关闭")
    
    def crc16(self, data):
        """
        计算MODBUS CRC16校验
        
        Args:
            data: 字节数据
            
        Returns:
            CRC16值
        """
        crc = 0xFFFF
        
        for byte in data:
            crc ^= byte
            for _ in range(8):
                if crc & 0x0001:
                    crc = (crc >> 1) ^ 0xA001
                else:
                    crc >>= 1
        
        return crc
    
    def build_frame(self, cmd, data=b'', sequence=0):
        """
        构建协议帧
        
        Args:
            cmd: 命令码
            data: 数据部分
            sequence: 序号
            
        Returns:
            完整的帧数据
        """
        # 帧头 + 命令 + 长度(2字节大端) + 序号
        frame = struct.pack('>BBHB', FRAME_HEADER, cmd, len(data), sequence)
        frame += data
        
        # 计算CRC
        crc = self.crc16(frame)
        frame += struct.pack('>H', crc)
        
        return frame
    
    def send_frame(self, frame):
        """发送帧"""
        if self.ser and self.ser.is_open:
            if self.debug:
                print(f"\n[DEBUG] 发送: {frame.hex()}")
            # 清空输入缓冲区，避免旧数据干扰
            self.ser.reset_input_buffer()
            self.ser.write(frame)
            self.ser.flush()
    
    def wait_ack(self, expected_ack=ACK_OK):
        """
        等待应答
        
        Args:
            expected_ack: 期望的应答码
            
        Returns:
            True表示收到正确应答，False表示错误
        """
        try:
            # 读取应答帧：0x5A + ACK + CRC16 = 4字节
            response = self.ser.read(4)
            
            if self.debug:
                print(f"\n[DEBUG] 接收: {response.hex()} (长度: {len(response)})")
            
            if len(response) < 4:
                print(f"应答超时 (收到 {len(response)} 字节)")
                if len(response) > 0:
                    print(f"  收到数据: {response.hex()}")
                return False
            
            header = response[0]
            ack = response[1]
            crc_recv = struct.unpack('>H', response[2:4])[0]
            
            # 验证CRC
            crc_calc = self.crc16(response[0:2])
            
            if crc_calc != crc_recv:
                print(f"应答CRC错误: 收到=0x{crc_recv:04X}, 计算=0x{crc_calc:04X}, 原始数据={response.hex()}")
                return False
            
            if header != FRAME_HEADER:
                print(f"应答帧头错误: 0x{header:02X}")
                return False
            
            if ack == ACK_OK:
                return True
            elif ack == ACK_ERROR:
                print("设备返回错误应答")
                return False
            elif ack == ACK_CRC_ERROR:
                print("设备返回CRC错误")
                return False
            elif ack == ACK_BUSY:
                print("设备忙碌")
                return False
            else:
                print(f"未知应答码: 0x{ack:02X}")
                return False
                
        except Exception as e:
            print(f"等待应答异常: {e}")
            return False
    
    def uart_send_back(self, wait_time=2.0, read_timeout=0.5):
        """
        读取并打印UART接收到的所有数据（调试用）
        
        Args:
            wait_time: 等待设备发送数据的总时间（秒）
            read_timeout: 两次读取之间的间隔时间（秒）
        
        Returns:
            读取到的字节数据
        """
        if not self.ser or not self.ser.is_open:
            return b''
        
        print(f"\n[等待UART数据] 最多等待 {wait_time} 秒...")
        all_data = b''
        start_time = time.time()
        
        # 循环读取，直到超时或一段时间内没有新数据
        while time.time() - start_time < wait_time:
            available = self.ser.in_waiting
            
            if available > 0:
                # 有数据，读取
                chunk = self.ser.read(available)
                all_data += chunk
                print(f"  [读取] {len(chunk)} 字节")
                
                # 重置超时计时（因为收到了新数据）
                start_time = time.time()
                wait_time = read_timeout  # 后续只等待较短时间
            else:
                # 没有数据，短暂等待
                time.sleep(0.1)
        
        # 打印所有接收到的数据
        if len(all_data) > 0:
            print(f"\n[UART接收] 总共 {len(all_data)} 字节:")
            print(f"  HEX: {all_data.hex()}")
            try:
                # 尝试解码为ASCII
                text = all_data.decode('ascii', errors='ignore')
                if text.strip():
                    print(f"  ASCII:\n{text}")
            except:
                pass
        else:
            print("[UART接收] 无数据")
        
        return all_data

    
    def start_upgrade(self, firmware_size, retry=0):
        """
        开始升级会话
        
        Args:
            firmware_size: 固件大小
            retry: 当前重试次数
            
        Returns:
            成功返回True
        """
        if retry == 0:
            print(f"\n开始升级会话 (固件大小: {firmware_size} 字节)...")
        else:
            print(f"  [重试 {retry}/{self.retry_count}] 开始升级会话...")
        
        # 构建开始命令帧（4字节大端格式的固件大小）
        data = struct.pack('>I', firmware_size)
        frame = self.build_frame(CMD_START_UPGRADE, data)
        
        self.send_frame(frame)
        
        if self.wait_ack():
            print("升级会话已启动")
            return True
        else:
            # 如果失败且还有重试次数
            if retry < self.retry_count:
                time.sleep(0.2)  # 重试前等待
                return self.start_upgrade(firmware_size, retry + 1)
            
            print("启动升级会话失败")
            return False
    
    def send_data_packet(self, sequence, data, retry=0):
        """
        发送数据包（带重试机制）
        
        Args:
            sequence: 序号
            data: 数据
            retry: 当前重试次数
            
        Returns:
            成功返回True
        """
        frame = self.build_frame(CMD_DATA_PACKET, data, sequence)
        self.send_frame(frame)
        
        if self.wait_ack():
            return True
        
        # 如果失败且还有重试次数
        if retry < self.retry_count:
            print(f"  [重试 {retry + 1}/{self.retry_count}]", end='')
            time.sleep(0.1)  # 重试前等待
            return self.send_data_packet(sequence, data, retry + 1)
        
        return False
    
    def end_upgrade(self, firmware_crc):
        """
        结束升级会话
        
        Args:
            firmware_crc: 固件CRC16
            
        Returns:
            成功返回True
        """
        print(f"\n结束升级会话 (CRC: 0x{firmware_crc:04X})...")
        
        # 构建结束命令帧（2字节大端格式的CRC）
        data = struct.pack('>H', firmware_crc)
        frame = self.build_frame(CMD_END_UPGRADE, data)
        
        self.send_frame(frame)
        
        if self.wait_ack():
            print("升级完成！")
            return True
        else:
            print("升级失败")
            return False
    
    def upgrade_firmware(self, firmware_path):
        """
        执行完整的固件升级流程
        
        Args:
            firmware_path: 固件文件路径
            
        Returns:
            成功返回True
        """
        # 读取固件文件
        try:
            with open(firmware_path, 'rb') as f:
                firmware_data = f.read()
        except Exception as e:
            print(f"读取固件文件失败: {e}")
            return False
        
        firmware_size = len(firmware_data)
        print(f"固件文件: {firmware_path}")
        print(f"固件大小: {firmware_size} 字节")
        
        # 检查大小限制
        MAX_SIZE = 16 * 1024  # 16KB
        if firmware_size > MAX_SIZE:
            print(f"错误: 固件大小超过限制 ({MAX_SIZE} 字节)")
            return False
        
        # 计算固件CRC
        firmware_crc = self.crc16(firmware_data)
        print(f"固件CRC16: 0x{firmware_crc:04X}")
        
        # 开始升级会话
        if not self.start_upgrade(firmware_size):
            return False
        
        # 发送数据包
        sequence = 0
        offset = 0
        total_packets = (firmware_size + MAX_PACKET_SIZE - 1) // MAX_PACKET_SIZE
        
        print(f"\n开始传输数据 (共 {total_packets} 个数据包)...")
        
        while offset < firmware_size:
            # 获取当前数据包
            packet_data = firmware_data[offset:offset + MAX_PACKET_SIZE]
            packet_size = len(packet_data)
            
            # 发送数据包
            print(f"发送数据包 {sequence + 1}/{total_packets} "
                  f"({offset + packet_size}/{firmware_size} 字节, "
                  f"{((offset + packet_size) * 100) // firmware_size}%)", end='')
            
            if not self.send_data_packet(sequence & 0xFF, packet_data):
                print(" - 失败!")
                return False
            
            print(" - 成功")
            
            sequence += 1
            offset += packet_size
            
            # 数据包之间的延时
            # 每10个包增加额外延时，给设备更多处理时间
            if sequence % 10 == 0:
                time.sleep(self.packet_delay * 5)  # 第10、20、30...包后延时更长
            else:
                time.sleep(self.packet_delay)
        
        # 结束升级
        if not self.end_upgrade(firmware_crc):
            return False
        
        print("\n固件升级完成！设备将在3秒后跳转到新应用程序。")
        return True


def main():
    """主函数"""
    parser = argparse.ArgumentParser(
        description='TLE989X UART固件升级工具',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  %(prog)s -p COM3 -f firmware.bin
  %(prog)s -p /dev/ttyUSB0 -f app2.bin -b 115200
        """
    )
    
    parser.add_argument('-p', '--port', required=True,
                        help='串口号 (如 COM3 或 /dev/ttyUSB0)')
    parser.add_argument('-f', '--firmware', required=True,
                        help='固件文件路径 (.bin 或 .hex)')
    parser.add_argument('-b', '--baudrate', type=int, default=115200,
                        help='波特率 (默认: 115200)')
    parser.add_argument('-t', '--timeout', type=float, default=2.0,
                        help='超时时间（秒，默认: 2.0）')
    parser.add_argument('-d', '--debug', action='store_true',
                        help='显示调试信息')
    parser.add_argument('-r', '--retry', type=int, default=3,
                        help='失败重试次数（默认: 3）')
    parser.add_argument('--delay', type=float, default=0.05,
                        help='数据包延时（秒，默认: 0.05）')
    
    args = parser.parse_args()
    
    # 检查固件文件是否存在
    if not os.path.exists(args.firmware):
        print(f"错误: 固件文件不存在: {args.firmware}")
        return 1
    
    # 创建升级器
    upgrader = UARTUpgrader(args.port, args.baudrate, args.timeout, 
                           args.debug, args.retry, args.delay)
    
    # 打开串口
    if not upgrader.open():
        return 1
    
    try:
        # 执行升级
        success = upgrader.upgrade_firmware(args.firmware)
        
        if success:
            print("\n升级成功！")
            return 0
        else:
            print("\n升级失败！")
            return 1
            
    except KeyboardInterrupt:
        print("\n\n用户中断")
        return 1
        
    finally:
        # 读取并打印升级完成后设备的输出信息
        upgrader.uart_send_back(wait_time=2.0, read_timeout=0.5)
        # 关闭串口
        upgrader.close()


if __name__ == '__main__':
    sys.exit(main())
