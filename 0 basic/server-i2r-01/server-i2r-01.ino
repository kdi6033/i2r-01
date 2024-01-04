/*
0 : 프로그램 다운로드
1 : ssid, password, mqttBroker, wifi.use 접속시 보드의 정보를 블루투스로 스마트폰으로 보냄
2 : ssid, password, mqttBroker, wifi.use 스마트폰에서 설정 값을 스마트폰에서 보드로 저장 보드는 리셋 됨
3 : 서버(보드)통신 방법 설정 0=블루투스 1=와이파이 mqtt 3=rs232 4=rs485 ============ 제거
4 : 보드제어 할 때 블루투스와 mqtt 사용 선택 ============ 제거
5 : 전송할 데이터를 스마트폰에서 보드로 보냄 그러면 이를 제외한 3개 통신으로 보냄
6 : 전송할 HEX 데이터를 스마트폰에서 보드로 보냄 그러면 이를 제외한 3개 통신으로 보냄
10 : 작성된 메세지만 보냄
11 : local에서 rs232 rs485 로 받은 문자를 hex 값으로 서버에 전송
  
*/
#include <WiFi.h>
#include <PubSubClient.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <map>
#include <ArduinoJson.h>
#include "SPIFFS.h"
#include <FS.h>
#include <Wire.h>
#include <HTTPUpdate.h>

#define TRIGGER_PIN 34 // trigger pin GPIO36:i2r-04 GPIO34:i2r-03
const int ledPin = 2;

// Define the Data structure
struct DataDevice {
  int selectedCommMethod = -1; // 통신 방식 선택 변수, 초기값은 -1로 설정
  bool bHex = false;  // 전송데이터가 hex 데이터 인지 표시
  String mac=""; // Bluetooth mac address 를 기기 인식 id로 사용한다.
  int dataFrom = -1; // ble=0 mqtt=1 rs232=2 rs485=3
  String sendData=""; 
  String sendDataHex="";
};

struct DataRs232 {
  int baudrate = 115200;
  String sendData="";
};

struct DataRs485 {
  int baudrate = 115200;
  String sendData="";
};

struct DataBle {
  char *service_uuid="4fafc201-1fb5-459e-8fcc-c5c9c331914b";
  char *characteristic_uuid="beb5483e-36e1-4688-b7f5-ea07361b26a8";
  bool isConnected=false;
};

struct DataWifiMqtt {
  bool selectMqtt=false;
  //bool use=false;
  bool loadSpiffs=false;
  bool isConnected=false;
  bool isConnectedMqtt=false;
  String ssid="";
  String password="";
  String mqttBroker = ""; // 브로커 
  char outTopic[50]="";  // "Ble mac address/out" 로 생성
  char inTopic[50]="";   // "Ble mac address/in" 으로 생성
};

// Create an instance of the Data structure
DataDevice dev;
DataBle ble;
DataWifiMqtt wifi,wifiSave;

WiFiClient espClient;
PubSubClient client(espClient);

unsigned int counter = 0;
unsigned long lastTime = 0;  // 마지막으로 코드가 실행된 시간을 기록할 변수
const long interval = 3000;  // 실행 간격을 밀리초 단위로 설정 (3초)
unsigned int event = 0;
String returnMsg="";

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
std::map<uint16_t, bool> deviceConnectedMap; // 클라이언트 연결 상태 관리
uint32_t value = 0;

void callback(char* topic, byte* payload, unsigned int length);
void checkFactoryDefault();
void connectToWiFi();
void doTick();
void loadConfigFromSPIFFS();
void parseJSONPayload(byte* payload, unsigned int length);
void publishMqtt();
void readBleMacAddress();
void reconnectMQTT();
void returnMessage(int order);
void saveConfigToSPIFFS();
//void sendViaSelect();
void setup();
void setupBLE();
void updateCharacteristicValue();
bool initializeSPIFFS();
int hexStringToCharArray(String hexString, char *outputArray);
void broadcast(int fromNo); 
void writeToBle(int order);

void download_program(String fileName);
void update_started();
void update_finished(); 
void update_progress(int cur, int total);
void update_error(int error);

void setup() {
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);

  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, 26, 27); //rs232
  Serial2.begin(115200, SERIAL_8N1, 16, 17); //rs485
  loadConfigFromSPIFFS();

  setupBLE();
  // BLE이 제대로 초기화될 수 있도록 약간의 시간을 기다립니다.
  delay(1000);
  // 이제 BLE MAC 주소를 읽어 봅니다.
  readBleMacAddress();
  Serial.println("BLE ready!");

  if(wifi.mqttBroker == nullptr) {
    returnMsg += "브로커가 설정되지 않았습니다.";
    writeToBle(10);
  }
  if (wifi.loadSpiffs) {
    // Wi-Fi 연결 설정
    connectToWiFi();
    // MQTT 설정
    client.setServer(wifi.mqttBroker.c_str(), 1883);
    client.setCallback(callback);
  } else {
    Serial.println("Wi-Fi and MQTT setup skipped as wifi.use is false.");
  }

}

void loop() {
  doTick();
  serial1Event();
  serial2Event();

  if(event != 0) {
    writeToBle(event);
    delay(1000);
    writeToBle(event);
    event = 0;
  }

  
  if (wifi.isConnected) {
    if (!client.connected()) {
      reconnectMQTT();
    }
    client.loop();
  }
 
  checkFactoryDefault();

}

// Serial 통신  ============================================
// 문자열을 HEX 형식으로 변환하는 함수
String stringToHex(const String &input) {
    String output;
    for (unsigned int i = 0; i < input.length(); i++) {
        char buf[3];
        sprintf(buf, "%02X", (unsigned char)input[i]);
        output += buf;
        output += " ";  // 각 바이트 사이에 공백 추가
    }
    output.trim();  // 마지막 공백 제거
    return output;
}

// fromNo : 0=ble 1=mqtt 2=rs232 3=rs485
void broadcast(int fromNo) {
  Serial.println("데이터 보냄 dataFrom : "+ String(dev.dataFrom)
  +"  sendData : "+ String(dev.sendData));
  // 블루투스 전송 로직
  writeToBle(11);
  // MQTT 전송 로직
  //Serial.println("데이터 보냄 mqtt : "+ String(dev.sendData));
  publishMqtt();
  //rs232
  if(dev.dataFrom != 2)
    Serial1.println(dev.sendData);
  //rs485
  if(dev.dataFrom != 3)
    Serial2.println(dev.sendData);
}

void serial1Event() {
  String s="";
  while(Serial1.available()) {
    char ch = Serial1.read();
    s += ch;
    //Serial.write(ch);
  }
  if(!s.isEmpty())  {
    Serial.print("[Received rs232]");
    Serial.println(s);
    dev.dataFrom = 2;
    dev.sendData = stringToHex(s);
    broadcast(2);
  } 
}

void serial2Event() {
  String s="";
  while(Serial2.available()) {
    char ch = Serial2.read();
    s += ch;
    //Serial.write(ch);
  }
  if(!s.isEmpty())  {
    Serial.print("[Received rs485]");
    Serial.println(s);
    dev.dataFrom = 3;
    dev.sendData = stringToHex(s);
    broadcast(3);
  } 
}
// Serial 통신  끝============================================

/* 블루투스 함수 시작===============================================*/

// Ble 수신된 문자를 받는다.
class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();

    if (rxValue.length() > 0) {
      Serial.println("[Received Ble : ");
      //for (int i = 0; i < rxValue.length(); i++)
      //  Serial.print(rxValue[i]);
      //Serial.println("");
      dev.dataFrom = 0;
      // std::string을 byte*로 변환
      parseJSONPayload((byte*)rxValue.c_str(),rxValue.length());
    }
  }
};

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t *param) {
      ble.isConnected = true;
      dev.selectedCommMethod =0;
      event = 1;
      uint16_t connId = param->connect.conn_id;
      deviceConnectedMap[connId] = true;
      BLEDevice::startAdvertising();
      // 클라이언트 연결 정보 출력
      Serial.print("Client Connected: ID = ");
      Serial.print(connId);
      char addr_str[18];
      esp_bd_addr_t remote_bda;
      memcpy(remote_bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
      sprintf(addr_str, "%02x:%02x:%02x:%02x:%02x:%02x",
              remote_bda[0], remote_bda[1], remote_bda[2],
              remote_bda[3], remote_bda[4], remote_bda[5]);
      Serial.println(addr_str);
    };

    void onDisconnect(BLEServer* pServer, esp_ble_gatts_cb_param_t *param) {
      uint16_t connId = param->disconnect.conn_id;
      deviceConnectedMap.erase(connId);
      ble.isConnected = false;

      // 클라이언트 연결 해제 정보 출력
      Serial.print("Client Disconnected: ID = ");
      Serial.println(connId);
    }
};

void setupBLE() {
  BLEDevice::init("i2r-01-Wifi Ble Serial");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(ble.service_uuid);
  pCharacteristic = pService->createCharacteristic(
                      ble.characteristic_uuid,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );
  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setCallbacks(new MyCallbacks());
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(ble.service_uuid);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();
}

void writeToBle(int order) {
  //Serial.println("writeBle order : "+String(order));
  if (!ble.isConnected) {
    Serial.println("Bluetooth is not connected.");
    return;
  }
  // Create a JSON object
  DynamicJsonDocument responseDoc(1024);
  // Fill the JSON object based on the order
  responseDoc["order"] = order;
  if(order == 0) {
    responseDoc["msg"] = "프로그램 다운로드";
  }
  else if (order == 1) {  //블루투스가 접속됬을 때 와이파이 정보를 스마트폰으로 보냅
    responseDoc["ssid"] = wifi.ssid;
    responseDoc["password"] = wifi.password;
    responseDoc["mqttBroker"] = wifi.mqttBroker;
    responseDoc["msg"] = " mqtt broker:"+wifi.mqttBroker+" 등 보드에 저장된 값을 받았습니다.";
  }
  else if(order == 2) {
    responseDoc["msg"] = wifi.ssid+" 등 정보가 보드에 저장 되었습니다.";
  }
  else if(order == 11) {  //local 에서 받은 데이터를 서버로 보낸다.
    responseDoc["dataFrom"] = dev.dataFrom;
    responseDoc["msg"] = dev.sendData;
  }
  else if(order == 12) {  //local 에서 받은 데이터를 서버로 보낸다.
    responseDoc["dataFrom"] = dev.dataFrom;
    responseDoc["msg"] = dev.sendData;
  }
  //responseDoc["msg"] = returnMsg;
  returnMsg = "";

  // Serialize JSON object to string
  String responseString;
  serializeJson(responseDoc, responseString);

  // Convert String to std::string to be able to send via BLE
  if(order == 5)   //폰에서 받은 데이터를 로컬로 보낸다.
    responseString=dev.sendData;
  else if(order == 6)   //폰에서 받은 데이터를 로컬로 보낸다.
    responseString=dev.sendDataHex;
  std::string response(responseString.c_str());

  // Send the JSON string over BLE
  /*
  if (pCharacteristic) {
    pCharacteristic->setValue(response);
    pCharacteristic->notify(); // If notification enabled
  }
  */

  if (!deviceConnectedMap.empty()) {
    for (const auto &pair : deviceConnectedMap) {
      if (pair.second) {
        pCharacteristic->setValue((uint8_t*)responseString.c_str(), responseString.length());
        pCharacteristic->notify(pair.first);
      }
    }
  }
}

void readBleMacAddress() {
  // BLE 디바이스에서 MAC 주소를 가져옵니다.
  BLEAddress bleAddress = BLEDevice::getAddress();
  // MAC 주소를 String 타입으로 변환합니다.
  dev.mac = bleAddress.toString().c_str();
  // MAC 주소를 모두 대문자로 변환합니다.
  dev.mac.toUpperCase();
  // 시리얼 모니터에 BLE MAC 주소를 출력합니다.
  Serial.print("BLE MAC Address: ");
  Serial.println(dev.mac);
  // MQTT 토픽을 설정합니다.
  snprintf(wifi.outTopic, sizeof(wifi.outTopic), "%s/out", dev.mac.c_str());
  snprintf(wifi.inTopic, sizeof(wifi.inTopic), "%s/in", dev.mac.c_str());
  Serial.println(wifi.outTopic);
  Serial.println(wifi.inTopic);;
}
/* 블루투스 함수 끝===============================================*/

/* 와이파이 MQTT 함수 시작===============================================*/
void callback(char* topic, byte* payload, unsigned int length) {
  if(length <= 0)
    return;
  Serial.print("Message received on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  dev.sendData="";
  for (unsigned int i = 0; i < length; i++) {
    dev.sendData += (char)payload[i];
    Serial.print((char)payload[i]);
  }

  Serial.println("selectedCommMethod : "+String(dev.selectedCommMethod));
  // JSON 파싱 JSON이 아니면 메세지를 모든 통신에 전달
  dev.dataFrom = 1;
  parseJSONPayload(payload, length);
}

void publishMqtt()
{ 
  char msg[200];

  //if(wifi.isConnected != 1)
  //  return;
  //Serial.println("mqtt : "+dev.sendData);

  // Create a JSON object
  DynamicJsonDocument responseDoc(1024);
  // Fill the JSON object based on the order
  responseDoc["order"] = 11;
  responseDoc["dataFrom"] = dev.dataFrom;
  responseDoc["msg"] = dev.sendData;
  // Serialize JSON object to string
  String responseString;
  serializeJson(responseDoc, responseString);
  Serial.println("데이터 보냄 mqtt: "+responseString);
  client.publish(wifi.outTopic, responseString.c_str());

}

void connectToWiFi() {
  Serial.print("Connecting to Wi-Fi...");
  WiFi.begin(wifi.ssid, wifi.password);
  
  int wCount = 0;
  wifi.isConnected = true;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    checkFactoryDefault();
    wCount++;
    if(wCount > 20) {
      wifi.isConnected = false;
      returnMsg += "와이파이 정보가 잘못되었습니다.";
      writeToBle(101);
      break; // while 루프를 벗어납니다.
    }
  }
  if(wifi.isConnected == true)
    Serial.println("\nConnected to Wi-Fi");
  else
    Serial.println("\nCan not find Wi-Fi");
  
}

void reconnectMQTT() {
  while (!client.connected()) {
    //checkFactoryDefault();
    Serial.println("Connecting to MQTT...");
    if (client.connect(dev.mac.c_str())) {
      Serial.println("Connected to MQTT");
      client.subscribe(wifi.inTopic); // MQTT 토픽 구독
      wifi.isConnectedMqtt=true;
    } else {
      Serial.print("MQTT connection failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 5 seconds...");
      wifi.isConnectedMqtt=false;
      delay(5000);
    }
  }
  digitalWrite(ledPin, wifi.isConnectedMqtt);
}
/* 와이파이 MQTT 함수 끝===============================================*/

// httpupdate() 프로그램 다운로드
void download_program(String fileName) {
  Serial.println(fileName);
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    // The line below is optional. It can be used to blink the LED on the board during flashing
    // The LED will be on during download of one buffer of data from the network. The LED will
    // be off during writing that buffer to flash
    // On a good connection the LED should flash regularly. On a bad connection the LED will be
    // on much longer than it will be off. Other pins than LED_BUILTIN may be used. The second
    // value is used to put the LED on. If the LED is on with HIGH, that value should be passed
    httpUpdate.setLedPin(ledPin, LOW);

    // Add optional callback notifiers
    httpUpdate.onStart(update_started);
    httpUpdate.onEnd(update_finished);
    httpUpdate.onProgress(update_progress);
    httpUpdate.onError(update_error);

    String ss;
    //ss=(String)URL_fw_Bin+fileName;
    ss="http://i2r.link/download/"+fileName;
    Serial.println(ss);
    //t_httpUpdate_return ret = httpUpdate.update(client, ss);
    t_httpUpdate_return ret = httpUpdate.update(client, "http://i2r.link/download/down-permwareBasic.bin");
    
    switch (ret) {
      case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
        break;
  
      case HTTP_UPDATE_NO_UPDATES:
        Serial.println("HTTP_UPDATE_NO_UPDATES");
        break;
  
      case HTTP_UPDATE_OK:
        Serial.println("HTTP_UPDATE_OK");
        break;
    }
  }
}

void update_started() {
  Serial.println("Update Started");
}

void update_finished() {
  Serial.println("Update Finished");
}

void update_progress(int cur, int total) {
  Serial.printf("Progress: %d%%\n", (cur * 100) / total);
}

void update_error(int error) {
  Serial.printf("Update Error: %d\n", error);
}
// httpupdate() 프로그램 다운로드


// Tools ========================================================
void sendToLocalFromPhone() {
//Serial.print("dev.selectedCommMethod : "+String(dev.selectedCommMethod));
  //Serial.print("  dev.sendData : "+dev.sendData);
  //Serial.print("  dev.sendDataHex : "+dev.sendDataHex);
  //Serial.print("");
    
  if(dev.bHex == false) {
    writeToBle(5);
    client.publish(wifi.outTopic, dev.sendData.c_str());
    publishMqtt();
    Serial1.println(dev.sendData.c_str());
    Serial2.println(dev.sendData.c_str());
  }
  else {
    writeToBle(6);
    client.publish(wifi.outTopic, dev.sendDataHex.c_str());
    publishMqtt();

    char hexDataArray[50]; // 배열 크기는 HEX 데이터에 맞게 조절
    int dataSize = hexStringToCharArray(dev.sendDataHex, hexDataArray);
    //Serial.println("dataSize : "+String(dataSize));
    for(int i=0;i<dataSize;i++)
      Serial.println((int)hexDataArray[i]);
    Serial1.write(hexDataArray, dataSize);
    Serial2.write(hexDataArray, dataSize);
  }
}

int hexStringToCharArray(String hexString, char *outputArray) {
    int arrayIndex = 0;
    int startIndex = 0;
    int endIndex = hexString.indexOf(' ', startIndex); // 첫 번째 공백 위치 찾기

    // 공백으로 분리된 모든 HEX 값 처리
    while (endIndex != -1) {
        String byteString = hexString.substring(startIndex, endIndex);
        char byteChar = (char) strtol(byteString.c_str(), NULL, 16);
        outputArray[arrayIndex++] = byteChar;

        startIndex = endIndex + 1;
        endIndex = hexString.indexOf(' ', startIndex);
    }

    // 마지막 HEX 값 처리 (공백 뒤)
    String lastByteString = hexString.substring(startIndex);
    if (lastByteString.length() > 0) {
        char lastByteChar = (char) strtol(lastByteString.c_str(), NULL, 16);
        outputArray[arrayIndex++] = lastByteChar;
    }

    return arrayIndex; // 반환 값은 실제 배열의 크기
}

void returnMessage(int order) {
  Serial.println(order);
  DynamicJsonDocument responseDoc(1024);
  responseDoc["order"] = order;
  responseDoc["msg"] = returnMsg;
  dev.sendData="";
  serializeJson(responseDoc, dev.sendData);
  
  Serial.println(returnMsg);
  Serial.print("returnMessage: ");
  Serial.println(dev.sendData);
  if(wifi.selectMqtt == true) 
    publishMqtt();
  else
    writeToBle(order);
}

//1초 마다 실행되는 시간함수
void doTick() {
  unsigned long currentTime = millis();  // 현재 시간을 가져옵니다
  String strIn,strOut;
  if ( currentTime - lastTime >= interval) {
    lastTime = currentTime;
  }
}

// Config 파일을 SPIFFS에서 읽어오는 함수
void loadConfigFromSPIFFS() {
  Serial.println("파일 읽기");

  if (!initializeSPIFFS()) {
    Serial.println("Failed to initialize SPIFFS.");
    return;
  }

  if (!SPIFFS.exists("/config.txt")) {
    Serial.println("Config file does not exist.");
    wifi.loadSpiffs = false;
    return;
  }

  File configFile = SPIFFS.open("/config.txt", FILE_READ);
  if (!configFile) {
    Serial.println("Failed to open config file");
    return;
  }

  size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, buf.get());
  
  if (error) {
    Serial.println("Failed to parse config file");
    return;
  }

  wifi.ssid = doc["ssid"] | "";
  wifi.password = doc["password"] | "";
  wifi.mqttBroker = doc["mqttBroker"] | "";

  wifi.loadSpiffs = true; // 여기에서 loadSpiffs를 true로 설정

  Serial.print("wifi.ssid: "); Serial.println(wifi.ssid);
  Serial.print("wifi.password: "); Serial.println(wifi.password);
  Serial.print("wifi.mqttBroker: "); Serial.println(wifi.mqttBroker);

  configFile.close();
}

void saveConfigToSPIFFS() {
  Serial.println("config.txt 저장");

  if (!initializeSPIFFS()) {
    Serial.println("SPIFFS 초기화 실패.");
    return;
  }

  // SPIFFS 초기화를 시도합니다.
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS failed to initialize. Formatting...");
    // 초기화 실패 시 포맷을 시도합니다.
    if (!SPIFFS.format()) {
      Serial.println("SPIFFS format failed.");
      return;
    }
    // 포맷 후에 다시 초기화를 시도합니다.
    if (!SPIFFS.begin()) {
      Serial.println("SPIFFS failed to initialize after format.");
      return;
    }
  }

  File configFile = SPIFFS.open("/config.txt", FILE_WRITE);
  
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return;
  }

  DynamicJsonDocument doc(1024);

  // 데이터를 구조체에서 가져온다고 가정합니다.
  doc["ssid"] = wifiSave.ssid;
  doc["password"] = wifiSave.password;
  doc["mqttBroker"] = wifiSave.mqttBroker;

  Serial.print("wifi.ssid: "); Serial.println(wifiSave.ssid);
  Serial.print("wifi.password: "); Serial.println(wifiSave.password);
  Serial.print("wifi.mqttBroker: "); Serial.println(wifiSave.mqttBroker);

  if (serializeJson(doc, configFile) == 0) {
    Serial.println("Failed to write to file");
    configFile.close();
    return;
  }

  configFile.close();
  // 파일이 제대로 닫혔는지 확인합니다.
  if (configFile) {
    Serial.println("파일이 여전히 열려있습니다.");
  } else {
    Serial.println("파일이 성공적으로 닫혔습니다.");
  }
  Serial.println("파일 저장 끝");

  // 파일이 제대로 저장되었는지 확인합니다.
  if (SPIFFS.exists("/config.txt")) {
    Serial.println("Config file saved successfully.");
    // 저장이 확인된 후 재부팅을 진행합니다.
    Serial.println("Rebooting...");
    delay(2000); // 재부팅 전에 짧은 지연을 줍니다.
    //ESP.restart();
  } else {
    Serial.println("Config file was not saved properly.");
  }
  
  // ESP32 재부팅
  delay(1000);
  ESP.restart();
}

// SPIFFS를 초기화하고 필요한 경우 포맷하는 함수를 정의합니다.
bool initializeSPIFFS() {
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS 초기화 실패!");
    if (!SPIFFS.format()) {
      Serial.println("SPIFFS 포맷 실패!");
      return false;
    }
    if (!SPIFFS.begin()) {
      Serial.println("포맷 후 SPIFFS 초기화 실패!");
      return false;
    }
  }
  return true;
}

void checkFactoryDefault() {
  if ( digitalRead(TRIGGER_PIN) == LOW ) {
    Serial.println("Please wait over 3 min");
    SPIFFS.format();
    delay(1000);
    ESP.restart();
    delay(1000);
  }
}

void parseJSONPayload(byte* payload, unsigned int length) {
  char payloadStr[length + 1];
  memcpy(payloadStr, payload, length);
  payloadStr[length] = '\0';  // Null-terminate the string
  Serial.println(payloadStr);

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, payloadStr);

  if (error) {
    Serial.println("JSON 파싱 실패!");
    dev.sendData=payloadStr;
    dev.sendDataHex="";
    broadcast(dev.selectedCommMethod);
    return;
  }

  int order = doc["order"] | -1;
  if (order == 0) {
    const char *filename = doc["value"] | "";
    returnMsg += "보드에 새로운 펌웨어가 다운로드 됬습니다.";
    download_program(filename);
  }
  else if (order == 2) {
    const char *ssid = doc["ssid"] | "";
    const char *password = doc["password"] | "";
    const char *mqttBroker = doc["mqttBroker"] | "";

    wifiSave.ssid = ssid;
    wifiSave.password = password;
    wifiSave.mqttBroker=mqttBroker;
    writeToBle(order);
    saveConfigToSPIFFS();
  }
  else if (order == 3) {
    const int comMethod = doc["value"].as<int>();
    dev.selectedCommMethod = comMethod;
    //Serial.println(comMethod);
    //Serial.println(dev.selectedCommMethod);
    if(dev.selectedCommMethod==0)
      returnMsg += "서버통신은 블루투스로 설정했습니다.";
    else if(dev.selectedCommMethod==1)
      returnMsg += "서버통신은 와이파이로 설정했습니다.";
    else if(dev.selectedCommMethod==2)
      returnMsg += "서버통신은 rs233로 설정했습니다.";
    else if(dev.selectedCommMethod==3)
      returnMsg += "서버통신은 rs485로 설정했습니다.";
  }
  else if (order == 5) {
    const int comMethod = doc["value"].as<int>();
    dev.selectedCommMethod = comMethod;
    bool bHex = doc["bHex"];
    dev.bHex = bHex;
    String s = doc["value"];
    dev.sendData = s;
    returnMsg += s+" 데이터를 받았습니다.";
    sendToLocalFromPhone();
  }
  else if (order == 6) {  //서버에서 hex 데이터로 받아 전송한다.
    //const int comMethod = doc["value"].as<int>();
    const int comMethod = doc["selectedCM"];
    dev.selectedCommMethod = comMethod;
    bool bHex = doc["bHex"];
    dev.bHex = bHex;
    String s = doc["value"];
    dev.sendDataHex = s;
    Serial.println("Hex Data : "+dev.sendDataHex);
    //dev.sendData = s;
    returnMsg += "hex 데이터를 받아 rs232 rs485로 보냅니다.";
    sendToLocalFromPhone();
  }
  else {
    Serial.println(dev.selectedCommMethod);
    Serial.println(wifi.selectMqtt);
    if(dev.selectedCommMethod == 1 && wifi.selectMqtt == false) {
      sendToLocalFromPhone();
    }
  }
  //writeToBle(order);
  //Serial.println(returnMsg);

  // 응답 메세지 송부
  //writeToBle(10);
  //returnMessage(order);
}

// Tools ========================================================


