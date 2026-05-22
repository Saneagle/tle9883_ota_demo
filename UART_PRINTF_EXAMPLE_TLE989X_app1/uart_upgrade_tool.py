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
    
    def __init__(self, port, baudrate=115200, timeout=2):
        """
        初始化升级器
        
        Args:
            port: 串口号（如 'COM3' 或 '/dev/ttyUSB0'）
            baudrate: 波特率
            timeout: 超时时间（秒）
        """
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
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
            time.sleep(0.5)  # 等待串口稳定
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
            
            if len(response) < 4:
                print(f"应答超时 (收到 {len(response)} 字节)")
                return False
            
            header = response[0]
            ack = response[1]
            crc_recv = struct.unpack('>H', response[2:4])[0]
            
            # 验证CRC
            crc_calc = self.crc16(response[0:2])
            
            if crc_calc != crc_recv:
                print(f"应答CRC错误")
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
    
    def start_upgrade(self, firmware_size):
        """
        开始升级会话
        
        Args:
            firmware_size: 固件大小
            
        Returns:
            成功返回True
        """
        print(f"\n开始升级会话 (固件大小: {firmware_size} 字节)...")
        
        # 构建开始命令帧（4字节大端格式的固件大小）
        data = struct.pack('>I', firmware_size)
        frame = self.build_frame(CMD_START_UPGRADE, data)
        
        self.send_frame(frame)
        
        if self.wait_ack():
            print("升级会话已启动")
            return True
        else:
            print("启动升级会话失败")
            return False
    
    def send_data_packet(self, sequence, data):
        """
        发送数据包
        
        Args:
            sequence: 序号
            data: 数据
            
        Returns:
            成功返回True
        """
        frame = self.build_frame(CMD_DATA_PACKET, data, sequence)
        self.send_frame(frame)
        
        return self.wait_ack()
    
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
            
            # 短暂延时，避免过快
            time.sleep(0.01)
        
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
    
    args = parser.parse_args()
    
    # 检查固件文件是否存在
    if not os.path.exists(args.firmware):
        print(f"错误: 固件文件不存在: {args.firmware}")
        return 1
    
    # 创建升级器
    upgrader = UARTUpgrader(args.port, args.baudrate, args.timeout)
    
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
        # 关闭串口
        upgrader.close()


if __name__ == '__main__':
    sys.exit(main())
