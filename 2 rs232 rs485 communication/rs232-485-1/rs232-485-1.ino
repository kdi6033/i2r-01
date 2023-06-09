unsigned int counter = 0;
unsigned long previousMillis = 0;     
const long interval = 2000; 
String inputString = "";         // 받은 문자열

void tick();
void serial2Event();
void setup();

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, 26, 27);
  Serial2.begin(115200, SERIAL_8N1, 16, 17);
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
    //Serial1.print("rs232");
    //delay(1000);
    //Serial2.print("rs485");
  }  
}

void serial1Event() {
  if(Serial1.available())
    Serial.write("-");
  //delay(500);
  int in=0; 
  while(Serial1.available()) {
    char ch = Serial1.read();
    //Serial.print(ch,HEX);
    //Serial.print(" ");
    Serial.write(ch);
    in=1;
  }
  if(in==1)
    Serial.println();
}

void serial2Event() {
  if(Serial2.available())
    Serial.write("*");
  //delay(500);
  int in=0; 
  while(Serial2.available()) {
    char ch = Serial2.read();
    //Serial.print(ch,HEX);
    //Serial.print(" ");
    Serial.write(ch);
    in=1;
  }
  if(in==1)
    Serial.println();
}
