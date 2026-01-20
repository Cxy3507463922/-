# Flask 服务器配置

# 基本配置
SECRET_KEY = 'smart_guardian_secret_key'
DEBUG = True

# 服务器设置
HOST = '0.0.0.0'
PORT = 5000

# 数据库配置
DATABASE_PATH = 'guardian.db'

# ESP32 设备配置
DEVICE_ID = 'esp32_smart_guardian'
DEVICE_TIMEOUT = 30  # 设备超时时间（秒）

# 安全策略配置
ALARM_DURATION = 5  # 警报持续时间（秒）
SENSITIVITY = 'medium'  # 灵敏度设置：low, medium, high

# 日志配置
LOG_LEVEL = 'INFO'  # 日志级别：DEBUG, INFO, WARNING, ERROR, CRITICAL
LOG_FILE = 'smart_guardian.log'

# 串口配置（如果需要通过串口通信）
SERIAL_PORT = None  # Windows: COM3, Linux: /dev/ttyUSB0, Mac: /dev/tty.usbserial-* 
SERIAL_BAUD = 115200

# Web 界面配置
REFRESH_INTERVAL = 2  # 页面自动刷新间隔（秒）
THEME = 'default'  # 界面主题：default, dark, light