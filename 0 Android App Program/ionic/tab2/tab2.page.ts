import { Component,  ChangeDetectorRef,  OnInit} from '@angular/core';
import { GlobalService, DataWifiMqtt } from '../global.service'; 

@Component({
  selector: 'app-tab2',
  templateUrl: 'tab2.page.html',
  styleUrls: ['tab2.page.scss']
})
export class Tab2Page implements OnInit {
  hexData: string = ""; // 16진수 데이터 입력값 저장
  message: string = "";

  constructor(
    public globalService: GlobalService,
    private changeDetectorRef: ChangeDetectorRef
  ) {
    this.globalService.message$.subscribe(msg => {
      this.message = msg;
      this.changeDetectorRef.detectChanges();
      //this.updateHexData(); // HEX 데이터 업데이트
    });
    // hexData$를 구독하여 hexData를 업데이트
    this.globalService.hexData$.subscribe(hexString => {
      if (hexString) {
        // HEX 문자열을 그대로 hexData에 할당
        this.hexData = hexString;
      }
    });
  }

  ngOnInit() {
    // 블루투스가 연결된 경우 selectedCommMethod를 확인
    if (this.globalService.ble.isConnected) {
      this.globalService.dev.selectedCommMethod = 0;
    }
  }

  onCommMethodChange() {
    // 사용자가 통신 방법을 변경할 때마다 호출됩니다.
    this.globalService.sendData(3);
    console.log(this.globalService.dev.selectedCommMethod);
  }

  sendHexData() {
    this.globalService.dev.bHex = true;
    console.log('Sending Hex:', this.globalService.dev.sendDataHex);
    this.globalService.sendData(6);
  }

  // 보냄 버튼을 눌러 메세지 전송한다.
  sendData() {
    this.globalService.dev.bHex = false;
    console.log(this.globalService.dev.sendData);
    this.globalService.sendData(5);
  }

}
