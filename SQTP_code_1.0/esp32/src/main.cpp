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
bool relayActive = false; // 继电器状态（控制LED灯）
unsigned long lastMotionTime = 0;
unsigned long lastStatusUpdate = 0;
unsigned long lastCommandCheck = 0;
unsigned long lastWiFiCheck = 0;

// 函数声明
void setupWiFi();
void checkWiFiConnection();
void updateServerStatus();
void checkServerCommands();
void handleMotionDetection();
void activateRelay();
void deactivateRelay();
void blinkStatusLED(int times, int delayTime);
void debug(String message);

void setup() {
  // 初始化串口
  Serial.begin(DEBUG_BAUD);
  while (!Serial) {
    delay(100);
  }
  
  debug("=== 智能断电装置初始化 ===");
  
  // 初始化引脚
  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_STATUS_LED, OUTPUT);
  
  // 初始状态
  digitalWrite(PIN_RELAY, LOW);
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
  
  // 定期检查服务器命令
  if (millis() - lastCommandCheck >= 3000 && WiFi.isConnected()) {
    checkServerCommands();
    lastCommandCheck = millis();
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
    
    // 检测到人时激活继电器（LED灯常亮）
    activateRelay();
    
    // 闪烁LED指示检测到运动
    blinkStatusLED(5, 200);
  } else if (pirValue != MOTION_DETECTED && motionDetected) {
    // 检测到运动结束
    if (millis() - lastMotionTime >= 2000) {
      motionDetected = false;
      debug("人体运动结束");
      // 注意：这里不立即关闭继电器，由服务器控制倒计时后关闭
    }
  }
}

void activateRelay() {
  if (!relayActive) {
    relayActive = true;
    debug("继电器激活（LED灯开启）");
    
    // 激活继电器
    digitalWrite(PIN_RELAY, HIGH);
    
    // 更新服务器状态
    if (WiFi.isConnected()) {
      updateServerStatus();
    }
  }
}

void deactivateRelay() {
  if (relayActive) {
    relayActive = false;
    debug("继电器关闭（LED灯关闭）");
    
    // 关闭继电器
    digitalWrite(PIN_RELAY, LOW);
    
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
  doc["relay"] = relayActive;
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

void checkServerCommands() {
  if (!WiFi.isConnected()) {
    return;
  }
  
  String serverUrl = "http://" + String(SERVER_HOST) + ":" + String(SERVER_PORT) + "/api/v1/command";
  
  http.begin(wifiClient, serverUrl);
  http.addHeader("Content-Type", "application/json");
  
  // 创建请求数据
  StaticJsonDocument<128> doc;
  doc["device_id"] = DEVICE_ID;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    debug("命令检查响应 (" + String(httpResponseCode) + "): " + response);
    
    // 解析响应
    StaticJsonDocument<256> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);
    
    if (!error) {
      String command = responseDoc["command"];
      if (command == "relay_on") {
        activateRelay();
      } else if (command == "relay_off") {
        deactivateRelay();
      }
    }
  } else {
    debug("命令检查失败: " + http.errorToString(httpResponseCode));
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
