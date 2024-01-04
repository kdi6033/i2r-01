//tab1.page.ts
import { Component, ChangeDetectorRef, OnDestroy  } from '@angular/core';
import { Subscription } from 'rxjs';
import { GlobalService, DataWifiMqtt } from '../global.service'; 

@Component({
  selector: 'app-tab1',
  templateUrl: 'tab1.page.html',
  styleUrls: ['tab1.page.scss']
})
export class Tab1Page implements OnDestroy  {
  customMqttBroker:string="";
  message: string = "";

  //constructor(public globalService: GlobalService) {}
  private subscription: Subscription;

  constructor(
    public globalService: GlobalService, 
    private changeDetectorRef: ChangeDetectorRef
    ) {
    this.subscription = this.globalService.wifiData$.subscribe(() => {
      this.changeDetectorRef.detectChanges();
    });
    this.globalService.message$.subscribe(msg => {
      this.message = msg;
      this.changeDetectorRef.detectChanges();
    });
  }

  ngOnDestroy() {
    this.subscription.unsubscribe();
  }

  async scanAndConnect() {
    await this.globalService.scanAndConnect();
    // 여기서 연결 결과에 따른 추가 로직을 구현할 수 있습니다.
  }

  // GlobalService의 disconnect 메서드를 호출하는 래퍼 함수
  async disconnect() {
    await this.globalService.disconnect();
  }  

  
  saveCustomBroker() {
    if (this.globalService.wifi.mqttBroker === 'custom') {
      this.globalService.wifi.mqttBroker = this.customMqttBroker;
    }
  }

  onMqttBrokerChange(event: any) {
    // MQTT Broker 선택이 변경될 때 호출될 함수
    // 필요한 로직을 추가할 수 있습니다.
    // 예를 들어, 'custom'을 선택했을 때 추가 로직을 수행합니다.
    if (this.globalService.wifi.mqttBroker === 'custom') {
      // 사용자가 직접 입력할 수 있도록 추가 필드를 표시합니다.
      // 이 부분은 자동으로 처리됩니다. HTML 템플릿의 *ngIf에 의해
    }
  }
  
  saveAndSendWifiInfo() {
    // 직접 입력한 MQTT Broker 주소 처리
    // MQTT Broker가 'custom'일 경우, customMqttBroker 값을 mqttBroker에 할당합니다.
    if (this.globalService.wifi.mqttBroker === 'custom') {
      this.globalService.wifi.mqttBroker = this.customMqttBroker;
    }
    // 저장된 정보를 보드로 보내 저장하게 한다.
    this.globalService.sendData(2);
    // 로컬 스토리지에 mqttBroker 저장
    this.globalService.saveMqttBrokerToLocalStorage();
    // 블루투스 연결이 끊허진 것으로 표시해줘
    this.globalService.disconnect();
  }

  downloadFirmware() {
    this.globalService.sendData(0);
  }
  

}
