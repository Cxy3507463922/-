// esp32/hardware_test.ino
void setup() {
  Serial.begin(115200);
  
  // 初始化引脚
  pinMode(13, INPUT);    // 人体传感器
  pinMode(12, OUTPUT);   // 继电器
  pinMode(14, OUTPUT);   // 蜂鸣器
  pinMode(2, OUTPUT);    // 状态LED
  
  Serial.println("=== 硬件测试开始 ===");
  testRelay();
  testBuzzer();
  testPIRSensor();
}

void testRelay() {
  Serial.println("测试继电器...");
  digitalWrite(12, HIGH);
  delay(1000);
  digitalWrite(12, LOW);
  Serial.println("继电器测试完成");
}

void testBuzzer() {
  Serial.println("测试蜂鸣器...");
  for(int i=0; i<3; i++) {
    digitalWrite(14, HIGH);
    delay(200);
    digitalWrite(14, LOW);
    delay(200);
  }
  Serial.println("蜂鸣器测试完成");
}

void testPIRSensor() {
  Serial.println("测试人体传感器，请在传感器前挥手...");
  int detectionCount = 0;
  unsigned long startTime = millis();
  
  while(millis() - startTime < 10000) { // 测试10秒
    if(digitalRead(13)) {
      detectionCount++;
      digitalWrite(2, HIGH);
      Serial.println("检测到人体！");
      delay(500);
      digitalWrite(2, LOW);
    }
    delay(100);
  }
  
  Serial.println("人体传感器测试完成，检测次数: " + String(detectionCount));
}

void loop() {
  // 主循环为空，只测试一次
  delay(1000);
}