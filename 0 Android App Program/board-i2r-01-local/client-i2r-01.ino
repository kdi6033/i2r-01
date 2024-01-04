/*
0 : 프로그램 다운로드
1 : ssid, password, mqttBroker, wifi.use 접속시 보드의 정보를 블루투스로 스마트폰으로 보냄
2 : ssid, password, mqttBroker, wifi.use 스마트폰에서 설정 값을 스마트폰에서 보드로 저장 보드는 리셋 됨
3 : 서버(보드)통신 방법 설정 0=블루투스 1=와이파이 mqtt 3=rs232 4=rs485 
4 : 보드제어 할 때 블루투스와 mqtt 사용 선택
5 : 전송할 데이터를 스마트폰에서 보드로 보냄 그러면 이를 제외한 3개 통신으로 보냄
10 : 작성된 메세지만 보냄
11 : local에서 rs232 rs485 로 받은 문자를 hex 값으로 서버에 전송

*/
#include <WiFi.h>
#include <PubSubClient.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
//#include <BLEServer.h>
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
  String mac=""; // Bluetooth mac address 를 기기 인식 id로 사용한다.
  String sendData=""; // 보드의 입력,출려,전압 데이터를 json 형태로 저장
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
  static std::string targetMacAddress;  // 클래스 외부에서 초기화할 것
  char *service_uuid="4fafc201-1fb5-459e-8fcc-c5c9c331914b";
  char *characteristic_uuid="beb5483e-36e1-4688-b7f5-ea07361b26a8";
  static BLERemoteCharacteristic* pRemoteCharacteristic;
  static BLEAdvertisedDevice* myDevice;
  bool isConnected=false;
};

struct DataWifiMqtt {
  bool selectMqtt=false;
  bool use=true;
  bool isConnected=false;
  bool isConnectedMqtt=false;
  String ssid="i2r";
  String password="00000000";
  String mqttBroker = "test.mosquitto.org"; // 브로커 
  char outTopic[50]="48:E7:29:37:C4:B6/in";  // "Ble mac address/out" 로 생성
  char inTopic[50]="48:E7:29:37:C4:B6/out";   // "Ble mac address/in" 으로 생성
};

// Create an instance of the Data structure
DataDevice dev;
DataBle ble;
DataWifiMqtt wifi,wifiSave;
// 클래스 외부에서 targetMacAddress 초기화
//std::string DataBle::targetMacAddress = "48:E7:29:37:C4:B6";
std::string DataBle::targetMacAddress = "48:e7:29:37:c4:b6";
BLERemoteCharacteristic* DataBle::pRemoteCharacteristic = nullptr;
BLEAdvertisedDevice* DataBle::myDevice = nullptr;
BLECharacteristic *pCharacteristic;

WiFiClient espClient;
PubSubClient client(espClient);

static boolean doConnect = false;
static boolean doScan = false;
unsigned int counter = 0;
unsigned long lastTime = 0;  // 마지막으로 코드가 실행된 시간을 기록할 변수
const long interval = 10000;  // 실행 간격을 밀리초 단위로 설정 (3초)
unsigned int event = 0;
String returnMsg="";

unsigned long lastAttemptTime = 0;
const long reconnectInterval = 5000; // 5초 간격으로 재연결 시도

void callback(char* topic, byte* payload, unsigned int length);
//void checkFactoryDefault();
bool connectToServer();
void connectToWiFi();
void doTick();
//void loadConfigFromSPIFFS();
void publishMqtt();
void readBleMacAddress();
void reconnectMQTT();
//void returnMessage(int order);
//void saveConfigToSPIFFS();
void sendViaSelect();
void setup();
void setupBLE();
void startScan();
void updateCharacteristicValue();
void writeToBle(int order);
//bool initializeSPIFFS();
/*
void download_program(String fileName);
void update_started();
void update_finished(); 
void update_progress(int cur, int total);
void update_error(int error);
*/
void setup() {
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);

  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, 26, 27); //rs232
  Serial2.begin(115200, SERIAL_8N1, 16, 17); //rs485
  //loadConfigFromSPIFFS();

  setupClientBLE();
  // BLE이 제대로 초기화될 수 있도록 약간의 시간을 기다립니다.
  delay(1000);
  // 이제 BLE MAC 주소를 읽어 봅니다.
  readBleMacAddress();
  Serial.println("BLE ready!");

  if (wifi.use) {
    // Wi-Fi 연결 설정
    connectToWiFi();
    // MQTT 설정
    client.setServer(wifi.mqttBroker.c_str(), 1883);
    client.setCallback(callback);
  } else {
    Serial.println("Wi-Fi and MQTT setup skipped as wifi.use is false.");
  }

}

// Serial 통신  ============================================
void serial1Event() {
  String s="";
  char rsIn[50];
  int i=0;
  while(Serial1.available()) {
    char ch = Serial1.read();
    s += ch;
    rsIn[i++]=ch;
    //Serial.write(ch);
  }
  rsIn[i]=0;
  if(!s.isEmpty())  {
    Serial.println("[Received rs232]"+String(rsIn));
  } 
}

void serial2Event() {
  String s="";
  char rsIn[50];
  int i=0;
  while(Serial2.available()) {
    char ch = Serial2.read();
    s += ch;
    rsIn[i++]=ch;
    //Serial.write(ch);
  }
  if(!s.isEmpty())  {
    Serial.println("[Received rs485]"+String(rsIn));
  }  
}
// Serial 통신  끝============================================

/* 블루투스 함수 시작===============================================*/
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    Serial.print("[Received Ble]: ");
    for (int i = 0; i < length; i++) {
        Serial.print((char)pData[i]);
    }
    Serial.println();
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    ble.isConnected = true;
    event = 1;
  }

  void onDisconnect(BLEClient* pclient) {
    ble.isConnected = false;
    //BLEDevice::startAdvertising();
    Serial.println("onDisconnect");
  }
};

bool connectToServer() {
    Serial.print("Forming a connection to ");
    Serial.println(ble.myDevice->getAddress().toString().c_str());
    
    BLEClient* pClient = BLEDevice::createClient();
    Serial.println(" - Created client");

    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the BLE Server using MAC address
    pClient->connect(ble.myDevice);
    Serial.println(" - Connected to server");
    pClient->setMTU(517);
  
    // Obtain a reference to the service in the remote BLE server
    BLERemoteService* pRemoteService = pClient->getService(ble.service_uuid);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find our service UUID: ");
      Serial.println(ble.service_uuid);
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our service");

    // Obtain a reference to the characteristic in the service of the remote BLE server
    ble.pRemoteCharacteristic = pRemoteService->getCharacteristic(ble.characteristic_uuid);
    if (ble.pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(ble.characteristic_uuid);
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our characteristic");

    // Read the value of the characteristic, if possible
    if(ble.pRemoteCharacteristic->canRead()) {
      std::string value = ble.pRemoteCharacteristic->readValue();
      Serial.print("The characteristic value was: ");
      Serial.println(value.c_str());
    }

    if(ble.pRemoteCharacteristic->canNotify())
      ble.pRemoteCharacteristic->registerForNotify(notifyCallback);

    ble.isConnected = true;
    return true;
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("Found Device: ");
    Serial.println(advertisedDevice.toString().c_str());

    // MAC 주소 확인
    if (advertisedDevice.getAddress().toString() == ble.targetMacAddress) {
      BLEDevice::getScan()->stop();
      ble.myDevice = new BLEAdvertisedDevice(advertisedDevice);
      Serial.println("=============");
      Serial.println("Device found:");
      Serial.print("Address: ");
      Serial.println(ble.myDevice->getAddress().toString().c_str()); // MAC 주소 출력
      doConnect = true;
      doScan = true;
    }
  }
};

void setupClientBLE() {
  Serial.println("Starting Arduino BLE Client application...");
  BLEDevice::init("cli-01");

  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 5 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);

  // 블루투스 장치 주소
  std::string btAddress = BLEDevice::getAddress().toString();
  Serial.print("블루투스 장치 주소: ");
  Serial.println(btAddress.c_str());
  Serial.println("Waiting for a client connection to notify...");
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
  //snprintf(wifi.outTopic, sizeof(wifi.outTopic), "%s/out", dev.mac.c_str());
  //snprintf(wifi.inTopic, sizeof(wifi.inTopic), "%s/in", dev.mac.c_str());
  Serial.println(wifi.outTopic);
  Serial.println(wifi.inTopic);;
}

void startScan() {
  BLEDevice::getScan()->start(5, false);
}
/* 블루투스 함수 끝===============================================*/

/* 와이파이 MQTT 함수 시작===============================================*/
void callback(char* topic, byte* payload, unsigned int length) {
  //Serial.print("Message received on topic: ");
  //Serial.print(topic);
  Serial.print("[Received mqtt]: ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void publishMqtt()
{ 
  char msg[200];

  //if(wifi.isConnected != 1)
  //  return;
  client.publish(wifi.outTopic, dev.sendData.c_str());
}

void connectToWiFi() {
  Serial.print("Connecting to Wi-Fi...");
  WiFi.begin(wifi.ssid, wifi.password);
  
  int wCount = 0;
  wifi.isConnected = true;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    wCount++;
    if(wCount > 20) {
      wifi.isConnected = false;
      wifi.use = false;
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

// Tools ========================================================
//1초 마다 실행되는 시간함수
void doTick() {
  unsigned long currentTime = millis();  // 현재 시간을 가져옵니다
  String strIn,strOut;
  char rsOut[10] = {0x05, 0x31, 0x32, 0x00};
  char rsOut1[10] = {0x05, 0x31, 0x35, 0x00};
  if ( currentTime - lastTime >= interval) {
    lastTime = currentTime;
    counter++;
    int nCount = counter%4;
    String s = String(counter);
    switch(nCount) {
      case 0:
        if (ble.isConnected) {
          s="ble : "+String(counter);
          Serial.println(s);
          ble.pRemoteCharacteristic->writeValue(s.c_str(), s.length());
        }else if(doScan){
          BLEDevice::getScan()->start(0);  // this is just example to start scan after disconnect, most likely there is better way to do it in arduino
        }
        break;
      case 1:
        s="mqtt : "+String(counter);
        Serial.println(s);
        dev.sendData = s;
        publishMqtt();
        break;
      case 2:
        Serial.println("rs232");
        Serial1.print(rsOut);
        break;
      case 3:
        Serial.println("rs485");
        Serial2.print(rsOut1);
    }

  }
}

/*
// Config 파일을 SPIFFS에서 읽어오는 함수
void loadConfigFromSPIFFS() {
  Serial.println("파일 읽기");

  if (!initializeSPIFFS()) {
    Serial.println("Failed to initialize SPIFFS.");
    return;
  }

  if (!SPIFFS.exists("/config.txt")) {
    Serial.println("Config file does not exist.");
    wifi.use = false;
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
  wifi.use = doc["use"] | false;

  Serial.print("wifi.ssid: "); Serial.println(wifi.ssid);
  Serial.print("wifi.password: "); Serial.println(wifi.password);
  Serial.print("wifi.mqttBroker: "); Serial.println(wifi.mqttBroker);
  Serial.print("wifi.use: "); Serial.println(wifi.use);

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
  doc["use"] = wifiSave.use;

  Serial.print("wifi.ssid: "); Serial.println(wifiSave.ssid);
  Serial.print("wifi.password: "); Serial.println(wifiSave.password);
  Serial.print("wifi.mqttBroker: "); Serial.println(wifiSave.mqttBroker);
  Serial.print("wifi.use: "); Serial.println(wifiSave.use);

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

// Tools ========================================================

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
    t_httpUpdate_return ret = httpUpdate.update(client, ss);
    //t_httpUpdate_return ret = ESPhttpUpdate.update(client, URL_fw_Bin);
    // Or:
    //t_httpUpdate_return ret = ESPhttpUpdate.update(client, "server", 80, "file.bin");
    
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
*/
void loop() {
  doTick();
  serial1Event();
  serial2Event();

  if (wifi.use) {
    if (!client.connected()) {
      reconnectMQTT();
    }
    client.loop();
  }


  // 연결이 끊어졌고, 재연결 시도 시간이 되었을 때
  if (!ble.isConnected && millis() - lastAttemptTime > reconnectInterval) {
    lastAttemptTime = millis(); // 마지막 시도 시간 업데이트
    doConnect = true; // 연결 시도 플래그 설정
    startScan(); // 스캔 시작
  }

  // 서버에 연결을 시도
  if (doConnect) {
    if (connectToServer()) {
      Serial.println("Connected to the BLE Server.");
      ble.isConnected = true; // 연결 상태 업데이트
      doConnect = false; // 연결 시도 플래그 초기화
    } else {
      Serial.println("Failed to connect. Retry...");
      ble.isConnected = false; // 연결 상태 업데이트
      doConnect = false; // 연결 시도 플래그 초기화
      startScan(); // 재스캔 시작
    }
  }
 
  //checkFactoryDefault();

}
