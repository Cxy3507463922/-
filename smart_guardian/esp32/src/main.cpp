#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

// WiFi客户端
WiFiClient wifiClient;
HTTPClient http;

// 全局变量
bool deviceConnected = false;
bool motionDetected = false;
bool alarmActive = false;
unsigned long lastMotionTime = 0;
unsigned long lastStatusUpdate = 0;
unsigned long alarmStartTime = 0;
unsigned long lastWiFiCheck = 0;

// 函数声明
void setupWiFi();
void checkWiFiConnection();
void updateServerStatus();
void handleMotionDetection();
void activateAlarm();
void deactivateAlarm();
void blinkStatusLED(int times, int delayTime);
void debug(String message);

void setup() {
  // 初始化串口
  Serial.begin(DEBUG_BAUD);
  while (!Serial) {
    delay(100);
  }
  
  debug("=== 智能实验室安全卫士初始化 ===");
  
  // 初始化引脚
  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_STATUS_LED, OUTPUT);
  
  // 初始状态
  digitalWrite(PIN_RELAY, LOW);
  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_STATUS_LED, LOW);
  
  // 闪烁LED表示启动
  blinkStatusLED(3, 500);
  
  // 连接WiFi
  setupWiFi();
  
  // 等待WiFi连接
  while (!WiFi.isConnected()) {
    delay(500);
    Serial.print(".");
    digitalWrite(PIN_STATUS_LED, !digitalRead(PIN_STATUS_LED));
  }
  
  digitalWrite(PIN_STATUS_LED, HIGH);
  deviceConnected = true;
  
  debug("WiFi连接成功: " + WiFi.localIP().toString());
  debug("=== 初始化完成 ===");
}

void loop() {
  // 检查WiFi连接
  checkWiFiConnection();
  
  // 处理人体检测
  handleMotionDetection();
  
  // 处理警报状态
  if (alarmActive) {
    if (millis() - alarmStartTime >= ALARM_DURATION) {
      deactivateAlarm();
    }
  }
  
  // 定期更新服务器状态
  if (millis() - lastStatusUpdate >= STATUS_UPDATE_INTERVAL && WiFi.isConnected()) {
    updateServerStatus();
    lastStatusUpdate = millis();
  }
  
  // 延迟以稳定系统
  delay(100);
}

void setupWiFi() {
  debug("正在连接WiFi: " + String(WIFI_SSID));
  
  // 设置WiFi模式
  WiFi.mode(WIFI_STA);
  
  // 连接WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  // 等待连接
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000) {
    delay(500);
    Serial.print(".");
    digitalWrite(PIN_STATUS_LED, !digitalRead(PIN_STATUS_LED));
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    debug("WiFi连接成功");
    debug("IP地址: " + WiFi.localIP().toString());
    debug("MAC地址: " + WiFi.macAddress());
    digitalWrite(PIN_STATUS_LED, HIGH);
  } else {
    debug("WiFi连接失败");
    digitalWrite(PIN_STATUS_LED, LOW);
  }
}

void checkWiFiConnection() {
  // 定期检查WiFi连接
  if (millis() - lastWiFiCheck >= 5000) {
    if (!WiFi.isConnected()) {
      debug("WiFi连接丢失，正在重新连接...");
      digitalWrite(PIN_STATUS_LED, LOW);
      WiFi.reconnect();
      
      // 等待重新连接
      unsigned long startTime = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
        delay(500);
        digitalWrite(PIN_STATUS_LED, !digitalRead(PIN_STATUS_LED));
      }
      
      if (WiFi.isConnected()) {
        debug("WiFi重新连接成功");
        digitalWrite(PIN_STATUS_LED, HIGH);
        deviceConnected = true;
        blinkStatusLED(2, 300);
      }
    }
    lastWiFiCheck = millis();
  }
}

void handleMotionDetection() {
  int pirValue = digitalRead(PIN_PIR);
  
  if (pirValue == MOTION_DETECTED && !motionDetected) {
    motionDetected = true;
    lastMotionTime = millis();
    debug("检测到人体运动");
    
    // 触发警报
    if (AUTO_ALARM_ON_MOTION) {
      activateAlarm();
    }
    
    // 闪烁LED指示检测到运动
    blinkStatusLED(5, 200);
  } else if (pirValue != MOTION_DETECTED && motionDetected) {
    // 检测到运动结束
    if (millis() - lastMotionTime >= 2000) {
      motionDetected = false;
      debug("人体运动结束");
    }
  }
}

void activateAlarm() {
  if (!alarmActive) {
    alarmActive = true;
    alarmStartTime = millis();
    debug("警报触发");
    
    // 激活继电器和蜂鸣器
    digitalWrite(PIN_RELAY, HIGH);
    
    // 蜂鸣器发出警报声
    for (int i = 0; i < 5; i++) {
      digitalWrite(PIN_BUZZER, HIGH);
      delay(200);
      digitalWrite(PIN_BUZZER, LOW);
      delay(200);
    }
    
    // 更新服务器状态
    if (WiFi.isConnected()) {
      updateServerStatus();
    }
  }
}

void deactivateAlarm() {
  if (alarmActive) {
    alarmActive = false;
    debug("警报关闭");
    
    // 关闭继电器和蜂鸣器
    digitalWrite(PIN_RELAY, LOW);
    digitalWrite(PIN_BUZZER, LOW);
    
    // 更新服务器状态
    if (WiFi.isConnected()) {
      updateServerStatus();
    }
  }
}

void updateServerStatus() {
  if (!WiFi.isConnected()) {
    return;
  }
  
  String serverUrl = "http://" + String(SERVER_HOST) + ":" + String(SERVER_PORT) + API_PATH;
  
  http.begin(wifiClient, serverUrl);
  http.addHeader("Content-Type", "application/json");
  
  // 创建JSON数据
  StaticJsonDocument<256> doc;
  doc["device_id"] = DEVICE_ID;
  doc["motion"] = motionDetected;
  doc["alarm"] = alarmActive;
  doc["signal_strength"] = WiFi.RSSI();
  doc["ip_address"] = WiFi.localIP().toString();
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  debug("发送状态到服务器: " + jsonString);
  
  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    debug("服务器响应 (" + String(httpResponseCode) + "): " + response);
  } else {
    debug("服务器请求失败: " + http.errorToString(httpResponseCode));
  }
  
  http.end();
}

void blinkStatusLED(int times, int delayTime) {
  for (int i = 0; i < times; i++) {
    digitalWrite(PIN_STATUS_LED, HIGH);
    delay(delayTime);
    digitalWrite(PIN_STATUS_LED, LOW);
    if (i < times - 1) {
      delay(delayTime);
    }
  }
}

void debug(String message) {
  if (DEBUG_SERIAL) {
    Serial.println("[" + String(millis()) + "] " + message);
  }
}
