#include "globals.h"
#pragma once

//*********************************************************************************************
//****                      Serial input and output buffer task                            ****
//*********************************************************************************************
void serialTask(void *parameter) {
  char c;
  Serial.println("Serial I/O Task Started");
  serial_t = true;

  for (;;) {

    //Check for chars to be sent
    while (txOutPtr != txInPtr) {     //Have we received any chars?)
      Serial.write(txBuf[txOutPtr]);  //Send char to console
      if (serverClient.connected()) {
        serverClient.write(txBuf[txOutPtr]);  //Send via Telnet if client connected
        //vTaskDelay(1);
      }
      txOutPtr++;                                   //Inc Output buffer pointer
      if (txOutPtr == sizeof(txBuf)) txOutPtr = 0;  //Wrap around circular buffer
    }
    //vTaskDelay(1);
    // Check for Received chars from Serial
    while (Serial.available()) {
      rxBuf[rxInPtr] = Serial.read();
      rxInPtr++;
      if (rxInPtr == sizeof(rxBuf)) rxInPtr = 0;
    }

    // Check for Received chars from Telnet
    while (serverClient.available()) {
      c = serverClient.read();
      if (c == 127) c = 8;  //Translate backspace
      rxBuf[rxInPtr] = c;
      rxInPtr++;
      if (rxInPtr == sizeof(rxBuf)) rxInPtr = 0;
    }

    // Check if the virtual UART register is empty if so and there is a char waiting then put in UART register
    if (rxOutPtr != rxInPtr && bitRead(pIn[UART_LSR], 0) == 0) {  //Have we received any chars and the read buffer is empty?
      pIn[UART_PORT] = rxBuf[rxOutPtr];                           //Put char in UART port
      rxOutPtr++;                                                 //Inc Output buffer pointer
      if (rxOutPtr == sizeof(rxBuf)) rxOutPtr = 0;
      bitWrite(pIn[UART_LSR], 0, 1);  //Set bit to say char can be read
    }

    vTaskDelay(1);
  }
}

//*********************************************************************************************
//****     Print string to output buffer, will send to serial and telnet if connected      ****
//*********************************************************************************************
void outString(char buf[]) {
  int i = 0;
  while (buf[i] > 0) {
    txBuf[txInPtr] = buf[i];  //Write char to output buffer
    txInPtr++;
    i++;
    if (txInPtr == sizeof(txBuf)) txInPtr = 0;
  }
}
