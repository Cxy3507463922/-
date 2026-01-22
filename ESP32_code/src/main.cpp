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
void syncWithServer();
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
  pinMode(PIN_LED_SENSE, INPUT); // LED 状态检测引脚
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
  checkWiFiConnection();
  
  // 1. 哑终端：仅收集信息 (只更新 motionDetected 变量)
  int pirValue = digitalRead(PIN_PIR);
  if (pirValue == MOTION_DETECTED) {
    if (!motionDetected) {
      motionDetected = true;
      lastMotionTime = millis();
      debug("传感器：检测到人体运动");
    }
  } else {
    if (motionDetected && (millis() - lastMotionTime >= 2000)) {
      motionDetected = false;
      debug("传感器：人体运动结束");
    }
  }
  
  // 2. 核心：每秒强制同步 (上传数据 -> 获取指令 -> 执行动作)
  if (millis() - lastStatusUpdate >= STATUS_UPDATE_INTERVAL && WiFi.isConnected()) {
    syncWithServer();
    lastStatusUpdate = millis();
  }
  
  delay(50);
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

void activateRelay() {
  if (!relayActive) {
    relayActive = true;
    debug("执行服务器指令：开启继电器");
    digitalWrite(PIN_RELAY, HIGH);
  }
}

void deactivateRelay() {
  if (relayActive) {
    relayActive = false;
    debug("执行服务器指令：关闭继电器");
    digitalWrite(PIN_RELAY, LOW);
  }
}

void syncWithServer() {
  if (!WiFi.isConnected()) return;

  // 1. 上报当前传感器原始数据给服务器决策
  bool ledLit = (digitalRead(PIN_LED_SENSE) == HIGH);
  String statusUrl = "http://" + String(SERVER_HOST) + ":" + String(SERVER_PORT) + "/api/v1/status";
  http.begin(wifiClient, statusUrl);
  http.addHeader("Content-Type", "application/json");

  JsonDocument doc;
  doc["device_id"] = DEVICE_ID;
  doc["motion"] = motionDetected;
  doc["led_sensed"] = ledLit;
  
  int situation = motionDetected ? 1 : (ledLit ? 2 : 3);
  doc["situation"] = situation;

  String jsonString;
  serializeJson(doc, jsonString);
  http.POST(jsonString);
  http.end();

  // 2. 纯听从指令：获取服务器计算后的继电器状态值
  String relayUrl = "http://" + String(SERVER_HOST) + ":" + String(SERVER_PORT) + "/api/v1/relay_state";
  http.begin(wifiClient, relayUrl);
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    if (payload == "1") {
      activateRelay();
    } else if (payload == "0") {
      deactivateRelay();
    }
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
