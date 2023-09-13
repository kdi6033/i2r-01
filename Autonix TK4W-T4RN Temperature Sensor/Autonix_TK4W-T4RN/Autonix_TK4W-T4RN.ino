uint8_t data[] = {0x01, 0x04, 0x03, 0xE8, 0x00, 0x01};
unsigned int counter = 0;
unsigned long previousMillis = 0;     
const long interval = 4000; 
String inputString = "";         // 받은 문자열

const int bufferSize = 10; // 임의의 크기. 필요에 따라 적절하게 조정하세요.
uint8_t receivedData[bufferSize];
int dataIndex = 0;


uint16_t crc16(const uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFF;  // 초기 값
    while (length--) {
        crc ^= *data++;  // XOR byte into least significant byte of crc
        
        for (int i = 0; i < 8; i++) {  // Loop over each bit
            if (crc & 1) {
                crc >>= 1;
                crc ^= 0xA001;  // Toggle the LSB using polynomial 0xA001
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

void setup() {
  Serial.begin(9600);
  Serial2.begin(9600, SERIAL_8N2, 16, 17);
}

void serial2Event() {
  /*
  while(Serial2.available()) {
    char ch = Serial2.read();
    Serial.print(ch,HEX);
    Serial.print(" ");
  }
  */
    while (Serial2.available()) {
    receivedData[dataIndex] = Serial2.read();
    //Serial.print(receivedData[dataIndex],HEX);
    //Serial.print(" ");
    dataIndex++;
    //if (dataIndex >= 4)
    //  dataIndex=0;
    // 데이터가 충분히 도착한 경우
    if (dataIndex >= 7) { // 바이트 수 + 데이터 길이 (2바이트) + 이전 바이트
      for (int i = 0; i < dataIndex; i++) {
        Serial.print(receivedData[i], HEX);
        Serial.print(" ");
      }
      Serial.println(); // 줄 바꿈
      
      int dataLength = receivedData[2]; // 바이트 수
      
      // 데이터 길이가 2바이트인 경우
      if (dataLength == 2) {
        int value = (receivedData[3] << 8) | receivedData[4]; // 2바이트 데이터를 10진수로 변환
        Serial.print("Received value: ");
        Serial.println(value); // 변환된 값을 출력
      }
      
      // 버퍼와 인덱스 초기화
      memset(receivedData, 0, bufferSize);
      dataIndex = 0;
    }
  }

}

void tick() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    sendData();
  }  
}

void sendData() {
  uint16_t length = sizeof(data) / sizeof(data[0]);
  uint16_t crcResult = crc16(data, length);

  // CRC 값을 데이터 배열 뒤에 추가
  uint8_t sendData[length + 2];
  memcpy(sendData, data, length);
  sendData[length] = crcResult & 0xFF;       // CRC Low byte
  sendData[length + 1] = (crcResult >> 8) & 0xFF;  // CRC High byte

  // Serial2를 통해 데이터 배열 전송
  Serial2.write(sendData, length + 2);

  // Serial을 사용하여 전송되는 데이터 출력
  Serial.println("");
  Serial.print("Sending data: ");
  for (int i = 0; i < length + 2; i++) {
    Serial.print(sendData[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}


void loop() {
  tick();
  serial2Event();
}
