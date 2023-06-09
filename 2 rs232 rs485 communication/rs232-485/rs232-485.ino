unsigned int counter = 0;
unsigned long previousMillis = 0;     
const long interval = 10000; 
String inputString = "";         // 받은 문자열

void tick();
void serial2Event();
void setup();

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, 26, 27); //rs232
  Serial2.begin(115200, SERIAL_8N1, 16, 17); //rs485
}

void loop() {
  tick();
  serial1Event();
  serial2Event();
}

void tick() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    Serial.println("send");
    Serial1.print("rs232");
    delay(5000);
    Serial2.print("rs485");
  }  
}

void serial1Event() {
  while(Serial1.available()) {
    char ch = Serial1.read();
    Serial.write(ch);
  }
}

void serial2Event() {
  while(Serial2.available()) {
    char ch = Serial2.read();
    Serial.write(ch);
  }
}
