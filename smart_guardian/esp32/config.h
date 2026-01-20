#ifndef SMART_GUARDIAN_ESP32_CONFIG_H
#define SMART_GUARDIAN_ESP32_CONFIG_H

#include <Arduino.h>

// WiFi 配置
#define WIFI_SSID "your_wifi_ssid"          // 替换为你的WiFi名称
#define WIFI_PASSWORD "your_wifi_password"  // 替换为你的WiFi密码

// 服务器配置
#define SERVER_HOST "192.168.1.100"         // 替换为服务器IP地址
#define SERVER_PORT 5000                     // 服务器端口号
#define API_PATH "/api/v1/status"           // API路径

// 设备配置
#define DEVICE_ID "esp32_smart_guardian"    // 设备唯一标识
#define DEVICE_NAME "智能实验室安全卫士"    // 设备名称

// 引脚配置
#define PIN_PIR 13                           // 人体传感器引脚
#define PIN_RELAY 12                         // 继电器引脚
#define PIN_BUZZER 14                        // 蜂鸣器引脚
#define PIN_STATUS_LED 2                     // 板载LED引脚

// 传感器配置
#define MOTION_DETECTED HIGH                 // 人体传感器检测到信号的电平
#define SENSOR_DELAY 500                     // 传感器读取延迟（毫秒）

// 警报配置
#define ALARM_DURATION 5000                  // 警报持续时间（毫秒）
#define BUZZER_FREQUENCY 1000                // 蜂鸣器频率（Hz）

// 通信配置
#define HTTP_TIMEOUT 3000                    // HTTP请求超时时间（毫秒）
#define STATUS_UPDATE_INTERVAL 2000          // 状态更新间隔（毫秒）
#define RETRY_COUNT 3                        // 网络请求重试次数

// 调试配置
#define DEBUG_SERIAL 1                       // 是否启用串口调试（1/0）
#define DEBUG_BAUD 115200                    // 串口调试波特率

// 安全策略
#define AUTO_ALARM_ON_MOTION 1               // 检测到人体是否自动触发警报（1/0）
#define ALARM_COOLDOWN 30000                 // 警报冷却时间（毫秒）

#endif