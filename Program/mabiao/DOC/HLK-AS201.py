'''
@ author: baker
@ tools: pycharm
@ content: 海凌科AS201陀螺仪十轴数据串口输出显示
@ date: 2025.6.24
@ python软件版本：3.11.4
'''

import serial
import serial.tools.list_ports
import logging
import time
from typing import Optional

class SensorData:
    def __init__(self):
        # 初始化所有传感器数据字段
        self.ax = 0.0
        self.ay = 0.0
        self.az = 0.0
        self.gx = 0.0
        self.gy = 0.0
        self.gz = 0.0
        self.roll = 0.0
        self.pitch = 0.0
        self.yaw = 0.0
        self.mx = 0.0
        self.my = 0.0
        self.mz = 0.0
        self.q0 = 0.0
        self.q1 = 0.0
        self.q2 = 0.0
        self.q3 = 0.0
        self.temperature = 0.0
        self.pressure = 0.0
        self.height = 0.0

def combine_bytes(low_byte, high_byte, signed=False):
    """将两个字节组合成一个16位整数，支持有符号数"""
    value = (high_byte << 8) | low_byte
    if signed:
        # 如果最高位为1，说明是负数（补码表示）
        if value & 0x8000:  # 检查第16位（从0开始）
            value = value - 0x10000  # 转换为负数
    return value

def combine_four_bytes(byte1, byte2, byte3, byte4, signed=False):
    """将四个字节组合成一个32位整数，支持有符号数"""
    value = (byte4 << 24) | (byte3 << 16) | (byte2 << 8) | byte1
    if signed:
        if value & 0x80000000:  # 检查第32位
            value = value - 0x100000000  # 转换为负数
    return value

def parse_frame(buffer):
    """解析数据帧并返回解析结果和数据对象"""
    data = SensorData()
    
    # 定义帧头和帧尾
    HEAD_HIGH = 0xFA
    HEAD_LOW = 0xFB
    TAIL_HIGH = 0xFC
    TAIL_LOW = 0xFD
    
    buffer_index = len(buffer)
    
    # 检查帧头
    if buffer[0] != HEAD_HIGH or buffer[1] != HEAD_LOW:
        return False, data
    
    # 获取长度字段
    frame_len = buffer[2]
    
    # 检查帧长度是否合理
    if frame_len < 3 or frame_len + 4 > buffer_index:
        return False, data
    
    # 检查帧尾
    if buffer[buffer_index-2] != TAIL_HIGH or buffer[buffer_index-1] != TAIL_LOW:
        return False, data
    
    # 计算校验和
    checksum = 0
    for i in range(3, buffer_index-3):
        checksum += buffer[i]
    checksum &= 0xFF  # 确保校验和是8位
    
    # 验证校验和
    if checksum != buffer[buffer_index - 3]:
        return False, data
    
    # 解析命令
    cmd = buffer[3]
    
    # 解析数据部分
    data_index = 5
    
    # 根据命令解析不同类型的数据
    if cmd == 0:
        # 加速度计数据
        data.ax = combine_bytes(buffer[data_index], buffer[data_index+1], signed=True) * 0.00478515625
        data_index += 2
        data.ay = combine_bytes(buffer[data_index], buffer[data_index+1], signed=True) * 0.00478515625
        data_index += 2
        data.az = combine_bytes(buffer[data_index], buffer[data_index+1], signed=True) * 0.00478515625
        data_index += 2
        
        # 陀螺仪数据
        data.gx = combine_bytes(buffer[data_index], buffer[data_index+1], signed=True) * 0.0625
        data_index += 2
        data.gy = combine_bytes(buffer[data_index], buffer[data_index+1], signed=True) * 0.0625
        data_index += 2
        data.gz = combine_bytes(buffer[data_index], buffer[data_index+1], signed=True) * 0.0625
        data_index += 2
        
        # 角度数据
        data.roll = combine_bytes(buffer[data_index], buffer[data_index+1], signed=True) * 0.0054931640625
        data_index += 2
        data.pitch = combine_bytes(buffer[data_index], buffer[data_index+1], signed=True) * 0.0054931640625
        data_index += 2
        data.yaw = combine_bytes(buffer[data_index], buffer[data_index+1], signed=True) * 0.0054931640625
        data_index += 2
        
        # 磁力计数据
        data.mx = combine_bytes(buffer[data_index], buffer[data_index+1], signed=True) * 0.006103515625
        data_index += 2
        data.my = combine_bytes(buffer[data_index], buffer[data_index+1], signed=True) * 0.006103515625
        data_index += 2
        data.mz = combine_bytes(buffer[data_index], buffer[data_index+1], signed=True) * 0.006103515625
        data_index += 2
        
        # 四元数数据
        data.q0 = combine_bytes(buffer[data_index], buffer[data_index+1], signed=True) * 0.000030517578125
        data_index += 2
        data.q1 = combine_bytes(buffer[data_index], buffer[data_index+1], signed=True) * 0.000030517578125
        data_index += 2
        data.q2 = combine_bytes(buffer[data_index], buffer[data_index+1], signed=True) * 0.000030517578125
        data_index += 2
        data.q3 = combine_bytes(buffer[data_index], buffer[data_index+1], signed=True) * 0.000030517578125
        data_index += 2
        
        # 温度数据
        data.temperature = combine_bytes(buffer[data_index], buffer[data_index+1], signed=True) * 0.01
        data_index += 2
        
        # 解析32位气压值
        pressure_bits = combine_four_bytes(
            buffer[data_index], buffer[data_index+1], 
            buffer[data_index+2], buffer[data_index+3]
        )
        data.pressure = pressure_bits * 0.0002384185791
        data_index += 4
        
        # 解析32位高度值
        height_bits = combine_four_bytes(
            buffer[data_index], buffer[data_index+1], 
            buffer[data_index+2], buffer[data_index+3]
        )
        data.height = height_bits * 0.0010728836
        
        return True, data  # 解析成功
    
    return False, data  # 未知命令

# 配置日志
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler("serial_data.log"),
        logging.StreamHandler()
    ]
)

def list_available_ports() -> list:
    """列出所有可用的串口"""
    ports = serial.tools.list_ports.comports()
    return [port.device for port in ports]

def initialize_serial_port(port: str, baudrate: int = 9600, timeout: float = 1) -> Optional[serial.Serial]:
    """初始化并返回串口对象"""
    try:
        ser = serial.Serial(port, baudrate, timeout=timeout)
        logging.info(f"成功连接到串口: {port}，波特率: {baudrate}")
        return ser
    except serial.SerialException as e:
        logging.error(f"无法连接到串口 {port}: {str(e)}")
        return None

def receive_data(ser: serial.Serial) -> None:
    """从串口接收数据并记录日志"""
    try:
        buffer = bytearray() # 用于累积数据的缓冲区
        while True:
            if ser.in_waiting:
                new_data = ser.read(ser.in_waiting)  # 读取所有可用字节
                buffer.extend(new_data)
                
                # hex_str = ' '.join(f"{byte:02X}" for byte in new_data)  # 转换为十六进制字符串用于调试
                # logging.info(f"收到数据: {hex_str}")
                
                # 解析数据帧（假设帧尾是0xAA 0x5A）
                while len(buffer) >= 4:  # 至少包含帧头、长度、帧尾
                    if buffer[0] == 0xFA and buffer[1] == 0xFB:  # 帧头
                        frame_len = buffer[2]
                        expected_len = frame_len + 5
                        
                        if len(buffer) >= expected_len:
                            frame = bytes(buffer[:expected_len])
                            success, data = parse_frame(frame)
                            if success:
                                print("=== 传感器数据解析成功 ===")
                                # 加速度计数据
                                print(f"加速度计:")
                                print(f"  X轴: {data.ax:.6f} m/s2")
                                print(f"  Y轴: {data.ay:.6f} m/s2")
                                print(f"  Z轴: {data.az:.6f} m/s2")
                                # 陀螺仪数据
                                print(f"陀螺仪:")
                                print(f"  X轴: {data.gx:.6f} °/s")
                                print(f"  Y轴: {data.gy:.6f} °/s")
                                print(f"  Z轴: {data.gz:.6f} °/s")
                                # 角度数据
                                print(f"欧拉角:")
                                print(f"  俯仰角(Pitch): {data.roll:.6f} °")
                                print(f"  横滚角(Roll): {data.pitch:.6f} °")
                                print(f"  偏航角(Yaw): {data.yaw:.6f} °")
                                # 磁力计数据
                                print(f"磁力计:")
                                print(f"  X轴: {data.mx:.6f} μT")
                                print(f"  Y轴: {data.my:.6f} μT")
                                print(f"  Z轴: {data.mz:.6f} μT")
                                # 四元数数据
                                print(f"四元数:")
                                print(f"  q0: {data.q0:.9f}")
                                print(f"  q1: {data.q1:.9f}")
                                print(f"  q2: {data.q2:.9f}")
                                print(f"  q3: {data.q3:.9f}")
                                # 环境数据
                                print(f"环境数据:")
                                print(f"  温度: {data.temperature:.2f} °C")
                                print(f"  气压: {data.pressure:.2f} Pa")
                                print(f"  高度: {data.height:.2f} m")
                                # 验证四元数归一化
                                norm = data.q0**2 + data.q1**2 + data.q2**2 + data.q3**2
                                print(f"四元数模长: {norm:.9f} (理想值应为1)")
                            else:
                                print("帧解析失败")
                            # 从缓冲区移除已处理的帧
                            del buffer[:expected_len]
                        else:
                            break  # 数据不足，等待更多数据
                    else:
                        # 不是有效的帧头，丢弃第一个字节
                        del buffer[0]
            time.sleep(0.05)  # 减少CPU使用率
    except KeyboardInterrupt:
        logging.info("程序被用户中断")
    except Exception as e:
        logging.error(f"接收数据时发生错误: {str(e)}")
    finally:
        if ser and ser.is_open:
            ser.close()
            logging.info("串口已关闭")

def main():
    """主函数"""
    available_ports = list_available_ports()
    
    if not available_ports:
        logging.error("未找到可用的串口")
        return
    
    logging.info(f"可用的串口: {', '.join(available_ports)}")
    
    # 默认使用第一个串口，也可以修改为手动选择
    port_to_use = available_ports[0]
    logging.info(f"将使用串口: {port_to_use}")
    
    # 初始化串口
    ser = initialize_serial_port("COM2", baudrate=115200) #注意：串口号需要改成电脑识别出来的！！！
    if not ser:
        return
    
    # 开始接收数据
    receive_data(ser)

if __name__ == "__main__":
    main()    

