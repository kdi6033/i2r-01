//global.Service.ts
import { Injectable} from '@angular/core';
import { BleClient, BleDevice } from '@capacitor-community/bluetooth-le';
import { BehaviorSubject } from 'rxjs';

declare var cordova: any;


// 인터페이스 정의
interface DataDevice {
  selectedCommMethod: number;
  mac: string; // Bluetooth mac address
  bHex: boolean; // 일반 msg 인지 hex 데이터인지 구분
  sendData: string; // 전송 데이타
  sendDataHex: string; // Uint8Array 타입으로 추가
  msg: string;  // 통신에서 리턴되는 문자 표시
} 

interface DataBle {
  SERVICE_UUID: string;  // 블루투스 서비스 UUID
  CHARACTERISTIC_UUID: string;  // 블루투스 특성 UUID
  isConnected: boolean;
  connectedDevice: BleDevice | null;
}

export interface DataWifiMqtt {
  isConnected: boolean;
  isConnectedMqtt: boolean;
  ssid: string;
  password: string;
  mqttBroker: string;
  customMqttBroker: string; // mqtt boker에서 custom을 선택해도 사라지지 않게
  outTopic: string;
  inTopic: string;
}
 

@Injectable({
  providedIn: 'root'
})
export class GlobalService {

  dev: DataDevice = {
    selectedCommMethod:-1, // 0:Ble 1:Wifi 2:RS232 3:RS485
    mac: "",
    bHex: false,
    sendData: "",
    sendDataHex: "", // 초기값으로 빈 Uint8Array 할당
    msg: ""
  };

  // DataBle 타입의 객체 초기화
  ble: DataBle = {
    SERVICE_UUID: "4fafc201-1fb5-459e-8fcc-c5c9c331914b",
    CHARACTERISTIC_UUID: "beb5483e-36e1-4688-b7f5-ea07361b26a8",
    isConnected: false,
    connectedDevice: null
  };

  wifi: DataWifiMqtt = {
    isConnected: false,
    isConnectedMqtt: false,
    ssid: "",
    password: "",
    mqttBroker: "",
    customMqttBroker: "",
    outTopic: "",
    inTopic: ""
  };
  
  devices: BleDevice[] = [];
  intervalId: any;
  mqttBroker1: string = ""; // 브로커 주소
  receivedMessage: string = ""; // 수신된 메시지 저장

  private wifiDataSubject = new BehaviorSubject<DataWifiMqtt>(this.wifi);
  wifiData$ = this.wifiDataSubject.asObservable();

  private messageSubject = new BehaviorSubject<string>("");
  message$ = this.messageSubject.asObservable();

  private hexDataSubject = new BehaviorSubject<string>("");
  hexData$ = this.hexDataSubject.asObservable();

  updateWifiData(newData: DataWifiMqtt) {
    this.wifi = newData;
    this.wifiDataSubject.next(this.wifi);
  }

  constructor() { 
    this.initializeBluetooth();   
    this.loadMqttBrokerFromLocalStorage(); // 로컬 스토리지에서 MQTT 브로커와 주제를 로드합니다.
    this.connectToMQTT(); // MQTT 연결 시작
    //console.log("mqtt 시작");
  }

  async initializeBluetooth() {
    try {
      await BleClient.initialize();
      console.log('Bluetooth initialized successfully');
    } catch (error) {
      console.error('Error initializing Bluetooth:', error);
    }
  }

  async scanAndConnect() {
    try {
      // 블루투스 기기 검색
      const result = await BleClient.requestDevice({
        // namePrefix를 사용하여 "i2r-"로 시작하는 기기만 필터링
        namePrefix: 'i2r-'
      });

      if (result) {
        // "i2r-"로 시작하는 기기만 devices 배열에 추가
        this.devices = [result];
        await this.connectToDevice(result); // 선택된 기기에 연결
      }
    } catch (error) {
      console.error('Error scanning and connecting to device:', error);
    }
  }

  async connectToDevice(device: BleDevice) {
    try {
      await BleClient.connect(device.deviceId);
      console.log('Connected to', device.name, 'with deviceId', device.deviceId);
      this.ble.connectedDevice = device; // 연결된 장치 저장
      this.ble.isConnected = true;
      // 블루투스 연결 시 selectedCommMethod를 0으로 설정
      this.dev.selectedCommMethod = 0;

      // 토픽 설정
      this.wifi.outTopic = `${device.deviceId}/in`;
      this.wifi.inTopic = `${device.deviceId}/out`;
      this.connectToMQTT();
      // 연결 성공 시 전역 변수 업데이트
      this.ble.isConnected = true;
      // 블루투스 장치로부터 메시지 수신을 시작합니다.
      this.startNotifications(device.deviceId);
      // ... 기타 연결 로직 ...
    } catch (error) {
      console.error('Failed to connect:', error);
      this.ble.isConnected = false;
    }
  }

  async disconnect() {
    if (this.ble.connectedDevice) {
      try {
        await BleClient.disconnect(this.ble.connectedDevice.deviceId);
        console.log('Disconnected from', this.ble.connectedDevice.name);
        this.ble.connectedDevice = null;
        this.ble.isConnected = false; // 연결 해제 시 전역 변수 업데이트
      } catch (error) {
        console.error('Failed to disconnect:', error);
      }
    } else {
      console.log('No device is connected');
    }
  }

  
  //message를 받아서 처리한다.
  async startNotifications(deviceId: string) {
    try {
      await BleClient.startNotifications(
        deviceId,
        this.ble.SERVICE_UUID,
        this.ble.CHARACTERISTIC_UUID,
        (data) => {
          // 여기서 데이터 처리를 합니다.
          const message = new TextDecoder().decode(data);
          console.log('Received message from ble device:', message);
          this.processReceivedMessage(message);
        }
      );
      console.log('Started notifications for', deviceId);
    } catch (error) {
      console.error('Failed to start notifications:', error);
    }
  }

  // 수신된 메시지를 처리하는 함수
  processReceivedMessage(jsonString: string) {
    try {
      console.log("processReceivedMessage : ",jsonString);
      //console.log("************************");
      // jsonString의 길이가 0인지 확인
      if (!jsonString.length) {
        console.error('Received empty message');
        return;
      }
      
      // JSON 문자열을 객체로 파싱합니다.
      const messageObject = JSON.parse(jsonString);
      //console.log("messageObject.order : ",messageObject.order);
      //console.log("messageObject.msg : ",messageObject.msg);
      //console.log("messageObject.msgHex : ",messageObject.msg);

      // 'order' 값에 따라 다른 처리를 수행합니다.
      switch (messageObject.order) {
        case 1:
          // 'order'가 1일 때의 처리 로직
          this.updateWifiData({
            ...this.wifi,
            ssid: messageObject.ssid,
            password: messageObject.password,
            customMqttBroker: messageObject.mqttBroker,
          });
          this.messageSubject.next(messageObject.msg);
          console.log('After update:', this.wifi); 
          break;
        case 2:
          // 'order'가 2일 때의 처리 로직 from Ble
          this.updateReceivedMessage(jsonString);
          break;
        case 3:
            // 글로벌 서비스의 수신된 메시지를 업데이트합니다
            this.updateReceivedMessage(jsonString);
            break;
        case 10:
          this.receivedMessage = messageObject.msg;
          this.messageSubject.next(this.receivedMessage);
            break;
        case 11:
          let displayMessage = messageObject.msg || ""; // msg가 undefined일 경우 빈 문자열로 초기화
          // msgHex가 존재하면 msgHex 를 문자로 바꾸어 msg에 저장하고 아래를 진행해줘
          // msgHex가 존재하면 이를 문자열로 변환하여 msg에 추가
          /*
          if (messageObject.msgHex) {
            // 예를 들어, HEX 값을 ASCII 문자로 변환
            const hexString = messageObject.msgHex.split(' ').map((hex: string) => String.fromCharCode(parseInt(hex, 16))).join('');
            messageObject.msg = hexString;
            displayMessage = messageObject.msg + "  ["+ messageObject.msgHex+"]";
          }
          */

          // 화면에 표시될 메시지 업데이트
          this.messageSubject.next(displayMessage);
          break;
        case 12:
          // 글로벌 서비스의 수신된 메시지를 업데이트합니다
          //this.updateReceivedMessage(messageObject.msg);
          this.receivedMessage = messageObject.msg;
          this.messageSubject.next(this.receivedMessage);
          break;
        default:
          this.updateReceivedMessage(jsonString);
          //console.log('알 수 없는 order:', messageObject.order);
      }
    } catch (error) {
      console.error('JSON 메시지 처리 중 오류 발생:', error);
    }

    //const newIn = /* 변환된 in 배열 */;
    //this.updateDevIn(newIn);
  }

  async checkAndReconnectBluetooth() {
    if (!this.ble.connectedDevice) {
      // 연결된 디바이스가 없으면 바로 반환
      return;
    }

    try {
      // 연결 상태를 확인하기 위해 해당 디바이스의 서비스를 가져옵니다.
      await BleClient.getServices(this.ble.connectedDevice.deviceId);
      this.ble.isConnected = true;  // 성공적으로 가져온 경우, 전역 변수 업데이트
    } catch (error) {
      console.error("블루투스 연결 상태를 확인하는 중 오류 발생:", error);
      this.ble.isConnected = false;  // 연결이 끊어졌거나 오류가 발생한 경우, 전역 변수 업데이트

      // 이전에 연결되었던 블루투스 장치로 재연결을 시도합니다.
      try {
        await BleClient.connect(this.ble.connectedDevice.deviceId);
        console.log('블루투스 재연결 성공:', this.ble.connectedDevice.deviceId);
        this.ble.isConnected = true;  // 재연결 성공, 전역 변수 업데이트
      } catch (reconnectError) {
        console.error("블루투스 재연결 중 오류 발생:", reconnectError);
      }
    }
    console.log(this.ble.isConnected );
  }

  updateReceivedMessage(message: string) {
    //this.receivedMessage = message;
    this.processMessage(message);
  }


  private processMessage(jsonString: string) {
    try {
      console.log("------------------");
      console.log(jsonString);
      const messageObject = JSON.parse(jsonString);
      const inString: string = messageObject.in;
      const outString: string = messageObject.out;
      console.log(messageObject.in);
      console.log(inString);

      // 여기에서 dev 객체의 현재 상태를 출력합니다.
      console.log('Current state of dev:', this.dev);

    } catch (error) {
      console.error('Error processing message:', error);
    }
  }

  async sendData(order: number) {
    console.log("order : ",order);
    // 추가된 조건 검사
    if (this.dev.selectedCommMethod < 0) {
      this.messageSubject.next("에러 : 먼저 통신 방식을 입력하세요");
      // 메세지만 보냅
      //return; // 통신 방법이 선택되지 않았으므로 이후의 로직을 실행하지 않음
    }
    let data;
    if (order === 0) {
      data = {
        order: 0,
        value: "i2r-01.bin",
        message: "download firmware"
      };
    } 
    else if (order==2) {
      data = {
        order: 2,
        ssid: this.wifi.ssid,
        password: this.wifi.password,
        mqttBroker: this.wifi.mqttBroker,
        message: "board config"
      };
    }
    else if (order === 3) {
      // Ble wifi rs232 rs485 선택을 보낸다.
      data = {
        order: 3,
        value: this.dev.selectedCommMethod,
        message: "Communication method"
      };
    }
    /*
    else if (order === 4) {
      // 와이파이 사용하는지? 
      data = {
        order: 4,
        value: this.wifi.selectMqtt,
        message: "select Mqtt"
      };
    }
    */
    else if (order === 5) {
      data = {
        order: 5,
        selectedCM: this.dev.selectedCommMethod,
        bHex: this.dev.bHex,
        value: this.dev.sendData,
        message: "메세지 보냄"
      };
    }
    else if (order === 6) {
      data = {
        order: 6,
        selectedCM: this.dev.selectedCommMethod,
        bHex: this.dev.bHex,
        value: this.dev.sendDataHex,
        message: "Hex Data 보냄"
      };
    }
    else {
      data = {
        order: order,
        value: this.dev.sendData,
        message: "dev.sendData 보냄"
      };
    }
    // this.dev.selectedCommMethod 검사해서 0보다 작은 값이면 tab2의 <!-- 메시지 표시 --> 
    // <div>{{ message }}</div> 에 "통신 방식을 입력하세요" 라고 메세지 남기고 return을 실행 해줘
    console.log("Sending data : ", data);
    console.log("selectedCommMethod : ", this.dev.selectedCommMethod);
  
    // 블루투스 연결 상태 확인
  if (this.ble.connectedDevice && this.ble.isConnected) {
    // 블루투스 전송 로직
    console.log("via Bluetooth:");
    const uint8Array = new TextEncoder().encode(JSON.stringify(data));
    const dataView = new DataView(uint8Array.buffer);
    try {
      await BleClient.write(
        this.ble.connectedDevice.deviceId,
        this.ble.SERVICE_UUID,
        this.ble.CHARACTERISTIC_UUID,
        dataView
      );
      console.log('Data sent successfully via Bluetooth');
    } catch (error) {
      console.error('Failed to send data via Bluetooth:', error);
    }
  } else if (this.dev.selectedCommMethod === 1) {
    // MQTT 전송 로직
    console.log("via Mqtt:");
    console.log(data);
    this.publishMessage(JSON.stringify(data));
  } else {
    console.log("No valid connection method selected");
  }

    
  }
  
  //==========================================================================================
  // mqtt 함수
  connectToMQTT() {
    cordova.plugins.CordovaMqTTPlugin.connect({
      url: `tcp://${this.wifi.mqttBroker}`, // 'mqttBroker' 사용
      port: 1883,
      clientId: "YOUR_USER_ID_LESS_THAN_24_CHARS",
      connectionTimeout: 3000,
      willTopicConfig: {
          qos: 0,
          retain: true,
          topic: this.wifi.outTopic, // 'outTopic' 사용
          payload: ""
      },
      keepAlive: 60,
      isBinaryPayload: false,
      success: (s: any) => {
        console.log("connect success MQTT");
        console.log("mqtt broker",this.wifi.mqttBroker);
        console.log("toopic",this.wifi.outTopic);
        this.wifi.isConnectedMqtt = true;
        
        // MQTT에 연결된 후에 구독 및 메시지 수신 리스너 설정
        this.subscribeToTopic();
        this.setMessageListener();
      },
      error: function(e: any) {
          console.log("connect error MQTT");
          this.isConnectedMqtt = false;
      },
      onConnectionLost: function() {
          console.log("disconnect MQTT");
          this.isConnectedMqtt = false;
          this.reconnectMQTT(); // 재연결 로직 호출
      },
    });
  }

  reconnectMQTT() {
    if (!this.wifi.isConnectedMqtt) {
      console.log("MQTT Reconnecting...");
      this.connectToMQTT();
    }
  }

  ngOnDestroy(): void {
    cordova.plugins.CordovaMqTTPlugin.disconnect({
      success: () => {
        console.log("MQTT Disconnected");
      },
      error: (error: any) => {
        console.error("MQTT Disconnect Error", error);
      }
    });
  }

  setMessageListener() {
    cordova.plugins.CordovaMqTTPlugin.listen(this.wifi.inTopic, (payload: any, params: any) => {
      // 수신된 메시지를 처리합니다.
      try {
        console.log('Received message from mqtt device:');
        this.processReceivedMessage(payload);
      } catch (e) {
        console.error('Error parsing received message:', e);
      }
      console.log(payload);
    });
  }
  
  subscribeToTopic() {
    cordova.plugins.CordovaMqTTPlugin.subscribe({
      topic: this.wifi.inTopic,
      qos: 0,
      success: function(s: any) {
        console.log("subscription success");
      },
      error: function(e: any) {
        console.log("subscription error");
      }
    });
  }

  /*
  sendMassage(message: String) {
    if(this.wifi.selectMqtt == true)
      this.publishMessage(message);
    else
      this.sendData(2);
    
  }
  */

  publishMessage(message: String) {
    cordova.plugins.CordovaMqTTPlugin.publish({
      topic: this.wifi.outTopic,
      payload: message,
      qos: 0,
      retain: false,
      success: (s: any) => { // 화살표 함수 사용
        console.log(this.wifi.outTopic);
        console.log(message);
        console.log("publish success");
      },
      error: (e: any) => { // 화살표 함수 사용
        console.log("publish error");
      }
    });
  }

  // MQTT 연결 상태를 주기적으로 확인하고 재연결을 시도하는 메서드
  startMQTTReconnect() {
    this.intervalId = setInterval(async () => {
      if (this.wifi.isConnectedMqtt) {
        try {
          await this.connectToMQTT();
          console.log('MQTT 재연결 시도');
        } catch (error) {
          console.error('MQTT 재연결 실패:', error);
        }
      }
    }, 2000); // 예: 2초마다 실행
  }

  // MQTT 재연결 로직을 중지하는 메서드
  stopMQTTReconnect() {
    if (this.intervalId) {
      clearInterval(this.intervalId);
      this.intervalId = null;
    }
  }

  // local storage 관리 프로그램 ================================================
  isLocalStorageAvailable(): boolean {
    try {
      const test = '__test__';
      localStorage.setItem(test, test);
      localStorage.removeItem(test);
      return true;
    } catch (e) {
      return false;
    }
  }

  saveMqttBrokerToLocalStorage() {
    if (this.isLocalStorageAvailable()) {
      localStorage.setItem('mqttBroker', this.wifi.mqttBroker);
      localStorage.setItem('outTopic', this.wifi.outTopic);
      localStorage.setItem('inTopic', this.wifi.inTopic);
    } else {
      console.error('Local Storage is not available');
    }
  }
  
  loadMqttBrokerFromLocalStorage() {
    if (this.isLocalStorageAvailable()) {
      const mqttBroker = localStorage.getItem('mqttBroker');
      const outTopic = localStorage.getItem('outTopic');
      const inTopic = localStorage.getItem('inTopic');
      
      this.wifi.mqttBroker = mqttBroker ?? this.wifi.mqttBroker;
      this.wifi.outTopic = outTopic ?? this.wifi.outTopic;
      this.wifi.inTopic = inTopic ?? this.wifi.inTopic;
    } else {
      console.error('Local Storage is not available');
    }
  }

  /*
  loadWifiSettingsFromLocalStorage() {
    if (this.isLocalStorageAvailable()) {
      const mqttBroker = localStorage.getItem('mqttBroker');
      const outTopic = localStorage.getItem('outTopic');
      const inTopic = localStorage.getItem('inTopic');
  
      if (mqttBroker) this.wifi.mqttBroker = mqttBroker;
      if (outTopic) this.wifi.outTopic = outTopic;
      if (inTopic) this.wifi.inTopic = inTopic;
    } else {
      console.error('Local Storage is not available');
    }
  }

  saveWifiSettingsToLocalStorage() {
    if (this.isLocalStorageAvailable()) {
      localStorage.setItem('mqttBroker', this.wifi.mqttBroker);
      localStorage.setItem('outTopic', this.wifi.outTopic);
      localStorage.setItem('inTopic', this.wifi.inTopic);
    } else {
      console.error('Local Storage is not available');
    }
  }
  */
  // local storage 관리 프로그램 ================================================
  /*
  sendDataViaSelectHex() {
    console.log(this.dev.sendDataHex);
    this.sendData(6);
  }
  */

}
