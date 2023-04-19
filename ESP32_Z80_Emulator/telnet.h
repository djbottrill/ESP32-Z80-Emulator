#pragma once

//*********************************************************************************************
//****                                 Telnet task                                         ****
//*********************************************************************************************
void TelnetTask(void *parameter) {
  server.begin();
  server.setNoDelay(true);
  Serial.println("Telnet Task Started");
  Serial.print("Use 'telnet ");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");

  for (;;) {

    if (server.hasClient()) {
      serverClient = server.available();
      Serial.print("\n\rNew Telnet client @ ");
      Serial.println(serverClient.remoteIP());
      vTaskDelay(100);
      serverClient.write(255);  // IAC
      serverClient.write(251);  // WILL
      serverClient.write(1);    // ECHO
      vTaskDelay(100);
      serverClient.write(255);  // IAC
      serverClient.write(251);  // WILL
      serverClient.write(3);    // suppress go ahead
      vTaskDelay(100);
      serverClient.write(255);  // IAC
      serverClient.write(252);  // WONT
      serverClient.write(34);   // LINEMODE
      vTaskDelay(100);
      serverClient.write(27);   //Print "esc"
      serverClient.print("c");  //Send esc c to reset screen
      vTaskDelay(100);
      while (serverClient.available()) serverClient.read(); //Get rid of any garbage received   
      vTaskDelay(100);   
      RUN = false;              //Force Z80 reboot
    }
   
    vTaskDelay(100);
  }
}
