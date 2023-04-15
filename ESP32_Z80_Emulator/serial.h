#pragma once

//*********************************************************************************************
//****                      Serial input and output buffer task                            ****
//*********************************************************************************************
void serialTask(void *parameter) {
  char c;
  Serial.println("Serial I/O Task Started");

  for (;;) {

    //Check for chars to be sent
    while (txOutPtr != txInPtr) {                   //Have we received any chars?)
      Serial.write(txBuf[txOutPtr]);                //Send char to console
      vTaskDelay(1);
      if (serverClient.connected())  serverClient.write(txBuf[txOutPtr]);   //Send via Telnet if client connected
      txOutPtr++;                                   //Inc Output buffer pointer
      if (txOutPtr == sizeof(txBuf)) txOutPtr = 0;  //Wrap around circular buffer
    }

    // Check for Received chars from Serial
    while (Serial.available()) {
      rxBuf[rxInPtr] = Serial.read();
      rxInPtr++;
      if (rxInPtr == sizeof(rxBuf)) rxInPtr = 0;
    }
    
    // Check for Received chars from Telnet
    while (serverClient.available()) {
      c = serverClient.read();
      if (c == 127) c = 8;                        //Translate backspace
      rxBuf[rxInPtr] = c;
      rxInPtr++;
      if (rxInPtr == sizeof(rxBuf)) rxInPtr = 0;
    }

    vTaskDelay(1);
  }
}
