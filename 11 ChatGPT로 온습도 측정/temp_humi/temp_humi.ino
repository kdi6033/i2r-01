#include <Wire.h>
#include <Adafruit_AHTX0.h>

Adafruit_AHTX0 aht;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);  // for Leonardo/Micro/Zero

  // I2C 핀 설정
  Wire.begin(14, 12);  // SDA, SCL 핀 설정

  if (!aht.begin()) {
    Serial.println("Failed to find AHT sensor!");
    while (1) delay(10);
  }
  Serial.println("AHT10 or AHT20 found");
}

void loop() {
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data

  Serial.print("온도: "); Serial.print(temp.temperature); Serial.println(" degrees C");
  Serial.print("습도: "); Serial.print(humidity.relative_humidity); Serial.println("% rH");

  delay(2000); // 2초 간격으로 데이터 출력
}
