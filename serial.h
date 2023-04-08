#pragma once

//*********************************************************************************************
//****                      Serial input and output buffer task                            ****
//*********************************************************************************************
void serialTask(void *parameter) {
  Serial.println("Serial Task Started");
  char c;
  int cw;
  for (;;) {

    //Check for chars to be sent
    //if (txOutPtr != txInPtr) {  //Have we received any chars?
    while(txOutPtr != txInPtr) {  //Have we received any chars?)
      c = txBuf[txOutPtr];
      Serial.write(c);  //Send char to console
      if (serverClient.connected()) {
        serverClient.write(c);
        vTaskDelay(1);     
      }
      txOutPtr++;  //Inc Output buffer pointer
      if (txOutPtr == sizeof(txBuf)) txOutPtr = 0;
    }

    // Check for Received chars
    while (Serial.available()) {
      c = Serial.read();
      rxBuf[rxInPtr] = c;
      rxInPtr++;
      if (rxInPtr == sizeof(rxBuf)) rxInPtr = 0;
    }

    if (serverClient.connected() && serverClient.available()) {
      cw = serverClient.available();
      while (cw > 0) {
        c = serverClient.read();
        rxBuf[rxInPtr] = c;
        rxInPtr++;
        if (rxInPtr == sizeof(rxBuf)) rxInPtr = 0;
        cw--;
        vTaskDelay(1);        
      }
    }
    
    vTaskDelay(1);
  }
}
