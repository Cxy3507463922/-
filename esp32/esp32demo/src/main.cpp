// 定义引脚
#include <Arduino.h>  // VS Code 必须包含此头文件

// 1. 引脚定义
const int PIR_PIN = 13;    // 人体传感器输入 (对应接线指南)
const int RELAY_PIN = 12;  // 继电器控制输出 (对应接线指南)
const int LED_PIN = 2;     // ESP32 板载蓝色指示灯

// 2. 时间配置
// 设置为 60000 毫秒 (即 1 分钟)
const unsigned long OFF_DELAY = 60000; 

// 3. 状态变量
unsigned long lastMovementTime = 0; // 记录最后一次检测到人的时间
bool isPowered = true;              // 记录当前继电器状态

void setup() {
    // 初始化串口，用于在电脑上观察状态
    Serial.begin(115200);
    
    // 配置引脚模式
    pinMode(PIR_PIN, INPUT);
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    
    // 系统启动默认：通电状态
    digitalWrite(RELAY_PIN, HIGH);
    digitalWrite(LED_PIN, HIGH);
    
    lastMovementTime = millis(); // 初始化计时
    Serial.println("--- 智能实验室安全卫士已启动 ---");
    Serial.println("功能：无人状态持续 1 分钟后自动断电");
}

void loop() {
    // 读取传感器信号 (HIGH 表示有人，LOW 表示无人)
    int pirValue = digitalRead(PIR_PIN);

    if (pirValue == HIGH) {
        // 如果检测到人，持续刷新“最后活动时间”
        lastMovementTime = millis();
        
        // 如果之前是断电状态，现在立刻恢复供电
        if (!isPowered) {
            Serial.println("[状态更新] 检测到活动，恢复供电中...");
            digitalWrite(RELAY_PIN, HIGH);
            digitalWrite(LED_PIN, HIGH);
            isPowered = true;
        }
    } else {
        // 只有在当前是通电状态下，才判断是否需要断电
        if (isPowered) {
            unsigned long currentTime = millis();
            
            // 检查无人时长是否超过了预设的 1 分钟
            if (currentTime - lastMovementTime >= OFF_DELAY) {
                Serial.println("[安全警报] 检测到离开超过 1 分钟，执行自动断电！");
                digitalWrite(RELAY_PIN, LOW); // 继电器断开
                digitalWrite(LED_PIN, LOW);   // 板载灯熄灭
                isPowered = false;
            }
        }
    }

    // 每隔 5 秒在串口打印一次倒计时信息，方便你调试
    static unsigned long lastLogTime = 0;
    if (isPowered && (millis() - lastLogTime > 5000)) {
        int secondsLeft = (OFF_DELAY - (millis() - lastMovementTime)) / 1000;
        Serial.print("系统运行中... 距离自动断电还剩: ");
        Serial.print(secondsLeft);
        Serial.println(" 秒");
        lastLogTime = millis();
    }

    delay(100); // 极短延迟以提高系统稳定性
}