#include <Arduino.h>
#include <FS.h>
#include <SPI.h>
#include <SD.h>
#include <SPIFFS.h>
#include <WiFi.h>

#include "globals.h"
#include "credentials.h"
#include "cpu.h"
#include "disk.h"

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("");
  Serial.println("");
  Serial.println("");
  Serial.write(27);             //Print "esc"
  Serial.println("c");          //Send esc c to reset screen
  Serial.println("****        Shady Grove        ****");
  Serial.println("****  Z80 Emulator for ESP32   ****");
  Serial.println("**** David Bottrill 2022  V2.1 ****");
  Serial.println();

  pinMode(LED_BUILTIN, OUTPUT);   //Built in LED functions as disk active indicator

  //BreakPoint switch inputs
  pinMode(swA, INPUT_PULLUP);

  //Initialise virtual GPIO ports
  Serial.println("Initialising Z80 Virtual GPIO Port");
  portOut(GPP  , 0) ;                        //Port 0 GPIO A 0 - 7 off
  portOut(GPP + 1, 255) ;                    //Port 1 GPIO A 0 - 7 Outputs
  portOut(GPP + 2, 0) ;                      //Port 0 GPIO B 1 & 1 off
  portOut(GPP + 3, 255) ;                    //Port 1 GPIO B 0 & 1 Outputs

  //Initialise virtual 6850 UART
  Serial.println("Initialising Z80 Virtual 6850 UART");
  pIn[UART_LSR] = 0x40;                       //Set bit to say TX buffer is empty

  //Depending if swA is pressed run in breakpoint mode or normal mode.
  if (digitalRead(swA) == 0) {

    BP = 0x0000;                //Set initial breakpoint
    BPmode = 0;                 //BP mode 0 runs normally
    Serial.println("Breakpoints enabled");
    Serial.print("\n\rEnter Breakpoint address in HEX: ");
    BP = hexToDec(getInput());
    if (BP > 0xffff) BP = 0xffff;
    Serial.printf("\n\rBreakpoint set to: %.4X\n\r", BP);
    Serial.println("\n\rBreakpoint modes: ");
    Serial.println("1 - Single Step");
    Serial.println("2 - Single Step from Breakpoint");
    Serial.println("3 - Stop each time Breakpoint Reached");
    Serial.print("\n\rEnter Breakpoint mode: ");
    BPmode = getInput().toInt();
    if (BPmode > 3) BPmode = 3;
    if (BPmode < 1) BPmode = 1;
    Serial.printf("\n\rBreakpoint mode set to: %1d\n\r", BPmode);
    Serial.println("Press button A to start");
    buttonA();
  }

  //Start the CPU task
  RUN = false;
  //baton = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(
    CPUTask,
    "CPUTask",
    3000,
    NULL,
    1,
    &Task1,
    1);         // Core 1
  delay(500);   // needed to start-up task


  //Start the Serial receive char task
  xTaskCreatePinnedToCore(
    serialTask,
    "SerialTask",
    3000,
    NULL,
    1,
    &Task2,
    0);         // Core 0
  delay(500);   // needed to start-up task

  Serial.println("Initialising Z80 Virtual Disk Controller");
  sdSPI.begin(SCK, MISO, MOSI, SS);
  if (!SD.begin(SS, sdSPI)) {
    Serial.println("SD Card Mount Failed");
    sdfound = false;
  }

  Serial.print("\n\rConnecting Wifi ");
  WiFi.begin(mySSID, myPASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.print("\n\rWiFi connected ");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.begin();
  server.setNoDelay(true);

  //Start the Telnet task
  xTaskCreatePinnedToCore(
    TelnetTask,
    "TelnetTask",
    3000,
    NULL,
    1,
    &Task3,
    0);         // Core 0
  delay(500);   // needed to start-up task

  Serial.print("Ready! Use 'telnet ");
  Serial.print(WiFi.localIP());
  Serial.println(" 23' to connect");
  Serial.print("Preferably use 'nc ");
  Serial.print(WiFi.localIP());
  Serial.println(" 23' to connect");

#ifdef T2
  //Start the OLED task
  xTaskCreatePinnedToCore(
    OLEDTask,
    "OLEDTask",
    3000,
    NULL,
    1,
    &Task4,
    0);       // Core 0
  delay(500);  // needed to start-up task
#endif

}

void loop() {
  bootstrap();                    //Load boot images from SD Card or SPIFFS
  PC = 0;                         //Set program counter
  switch (BPmode) {
    case 0:
      //Main loop in normal mode
      Serial.println("\n\rStarting Z80\n\r");
      SingleStep = false;
      bpOn = false;
      RUN = true;
      while (RUN == true) {
        if (digitalRead(swA) == 0) { //Pressing button A will force a restart
          RUN = false;
          while (digitalRead(swA) == 0) delay(50);
        }
        delay(50);
      }
      Serial.printf("\n\rCPU Halted @ %.4X ...rebooting...\n\r", PC - 1);
      PC = 0;
      break;
    case 1:
      Serial.println("\n\rStarting Z80 in Single Step Mode\n\r");
      SingleStep = true;
      while (1) {
        dumpReg();
        buttonA();
        RUN = true;                         //Start CPU
        delay(50);
        while (RUN == true) delay(50);      //Wait for CPU to stop
      }
      break;
    case 2:
      Serial.println("\n\rStarting Z80 in Single step from Breakpoint Mode\n\r");
      bpOn = true;                          //Run until BP
      SingleStep = false;
      RUN = true;
      delay(50);
      while (RUN == true)delay(50);         //Wait for CPU to stop
      bpOn = false;
      SingleStep = true;
      while (1) {                           //Now Single step
        dumpReg();
        buttonA();
        RUN = true;                         //Start CPU
        delay(50);
        while (RUN == true) delay(50);      //Wait for CPU to stop
      }
      break;
    case 3:
      Serial.println("\n\rStarting Z80 in Stop at Breakpoint Mode\n\r");
      bpOn = true;                          //Run until BP
      SingleStep = false;
      while (1) {
        RUN = true;
        delay(50);
        while (RUN == true)delay(50);       //Wait for CPU to stop
        dumpReg();

        buttonA();
        Serial.println(BP, HEX);
      }
      break;
    default:
      break;
  }

}

void buttonA(void) {
  while (digitalRead(swA) == 1) delay(5);
  while (digitalRead(swA) == 0) delay(5);
}



//*********************************************************************************************
//****                       Serial input string function                                  ****
//*********************************************************************************************
String getInput() {
  bool gotS = false;
  String rs = "";
  char received;
  while (gotS == false ) {
    while (Serial.available() > 0)
    {
      received = Serial.read();
      Serial.write(received);                                     //Echo input
      if (received == '\r' || received == '\n' ) {
        gotS = true;
      } else {
        rs += received;
      }
    }
  }
  return (rs);
}



//*********************************************************************************************
//****                           Convert HEX to Decimal                                    ****
//*********************************************************************************************
unsigned int hexToDec(String hexString) {
  unsigned int decValue = 0;
  int nextInt;
  for (int i = 0; i < hexString.length(); i++) {
    nextInt = int(hexString.charAt(i));
    if (nextInt >= 48 && nextInt <= 57) nextInt = map(nextInt, 48, 57, 0, 9);
    if (nextInt >= 65 && nextInt <= 70) nextInt = map(nextInt, 65, 70, 10, 15);
    if (nextInt >= 97 && nextInt <= 102) nextInt = map(nextInt, 97, 102, 10, 15);
    nextInt = constrain(nextInt, 0, 15);

    decValue = (decValue * 16) + nextInt;
  }
  return decValue;
}



//*********************************************************************************************
//****                      Serial input and output buffer task                            ****
//*********************************************************************************************
void serialTask( void * parameter ) {
  Serial.println("Serial Task Started");
  int c;
  for (;;) {

    //Check for chars to be sent
    while (txOutPtr != txInPtr) {           //Have we received any chars?
      Serial.write(txBuf[txOutPtr]);        //Send char to console
      if (serverClient.connected()) serverClient.write(txBuf[txOutPtr]);
      delay(1);
      txOutPtr++;                           //Inc Output buffer pointer
      if (txOutPtr == 1024) txOutPtr = 0;
    }
    delay(1);
    // Check for Received chars
    int c = Serial.read();
    if (c >= 0) {
      rxBuf[rxInPtr] = c;
      rxInPtr++;
      if (rxInPtr == 1024) rxInPtr = 0;
    }
    delay(1);
    while (serverClient.available()) {
      c = serverClient.read();
      if (c == 0x0A) c = 0x0D;
      rxBuf[rxInPtr] = c;
      rxInPtr++;
      if (rxInPtr == 1024) rxInPtr = 0;
      delay(1);
    }

    delay(1);
  }
}

//*********************************************************************************************
//****                                 Telnet task                                         ****
//*********************************************************************************************
void TelnetTask( void * parameter ) {
  Serial.println("Telnet Task Started");
  for (;;) {

    if (server.hasClient()) {
      serverClient = server.available();
      Serial.print("New client: ");
      Serial.println(serverClient.remoteIP());
      delay(100);
      RUN = false;    //Force Z80 reboot
    }
    delay(100);
  }
}

#ifdef T2
//*********************************************************************************************
//****                            OLED display task                                        ****
//*********************************************************************************************
void OLEDTask( void * parameter ) {
  display.begin();
  display.fillScreen(BLACK);
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.print("Z80 Emulator");
  display.setCursor(0, 10);
  display.print(WiFi.localIP());

  uint8_t oled1, oled2;
  const int oled1_y = 49;
  const int oled2_y = 59;
  bool dledO;
  for (;;) {

    if (oled1 != pOut[0]) {
      if (bitRead(oled1, 0) != bitRead(pOut[0], 0)) {
        if (bitRead(pOut[0], 0) == true) {
          display.fillCircle(90, oled1_y, 4, RED);
        } else {
          display.fillCircle(90, oled1_y, 4, BLACK);
        }
      }
      if (bitRead(oled1, 1) != bitRead(pOut[0], 1)) {
        if (bitRead(pOut[0], 1) == true) {
          display.fillCircle(78, oled1_y, 4, RED);
        } else {
          display.fillCircle(78, oled1_y, 4, BLACK);
        }
      }
      if (bitRead(oled1, 2) != bitRead(pOut[0], 2)) {
        if (bitRead(pOut[0], 2) == true) {
          display.fillCircle(66, oled1_y, 4, RED);
        } else {
          display.fillCircle(66, oled1_y, 4, BLACK);
        }
      }
      if (bitRead(oled1, 3) != bitRead(pOut[0], 3)) {
        if (bitRead(pOut[0], 3) == true) {
          display.fillCircle(54, oled1_y, 4, RED);
        } else {
          display.fillCircle(54, oled1_y, 4, BLACK);
        }
      }
      if (bitRead(oled1, 4) != bitRead(pOut[0], 4)) {
        if (bitRead(pOut[0], 4) == true) {
          display.fillCircle(42, oled1_y, 4, RED);
        } else {
          display.fillCircle(42, oled1_y, 4, BLACK);
        }
      }
      if (bitRead(oled1, 5) != bitRead(pOut[0], 5)) {
        if (bitRead(pOut[0], 5) == true) {
          display.fillCircle(30, oled1_y, 4, RED);
        } else {
          display.fillCircle(30, oled1_y, 4, BLACK);
        }
      }
      if (bitRead(oled1, 6) != bitRead(pOut[0], 6)) {
        if (bitRead(pOut[0], 6) == true) {
          display.fillCircle(18, oled1_y, 4, RED);
        } else {
          display.fillCircle(18, oled1_y, 4, BLACK);
        }
      }
      if (bitRead(oled1, 7) != bitRead(pOut[0], 7)) {
        if (bitRead(pOut[0], 7) == true) {
          display.fillCircle(6, oled1_y, 4, RED);
        } else {
          display.fillCircle(6, oled1_y, 4, BLACK);
        }
      }
      oled1 =  pOut[0];
    }

    if (oled2 != pOut[2]) {
      if (bitRead(oled2, 0) != bitRead(pOut[2], 0)) {
        if (bitRead(pOut[2], 0) == true) {
          display.fillCircle(90, oled2_y, 4, GREEN);
        } else {
          display.fillCircle(90, oled2_y, 4, BLACK);
        }
      }
      if (bitRead(oled2, 1) != bitRead(pOut[2], 1)) {
        if (bitRead(pOut[2], 1) == true) {
          display.fillCircle(78, oled2_y, 4, GREEN);
        } else {
          display.fillCircle(78, oled2_y, 4, BLACK);
        }
      }
      if (bitRead(oled2, 2) != bitRead(pOut[2], 2)) {
        if (bitRead(pOut[2], 2) == true) {
          display.fillCircle(66, oled2_y, 4, GREEN);
        } else {
          display.fillCircle(66, oled2_y, 4, BLACK);
        }
      }
      if (bitRead(oled2, 3) != bitRead(pOut[2], 3)) {
        if (bitRead(pOut[2], 3) == true) {
          display.fillCircle(54, oled2_y, 4, GREEN);
        } else {
          display.fillCircle(54, oled2_y, 4, BLACK);
        }
      }
      if (bitRead(oled2, 4) != bitRead(pOut[2], 4)) {
        if (bitRead(pOut[2], 4) == true) {
          display.fillCircle(42, oled2_y, 4, GREEN);
        } else {
          display.fillCircle(42, oled2_y, 4, BLACK);
        }
      }
      if (bitRead(oled2, 5) != bitRead(pOut[2], 5)) {
        if (bitRead(pOut[2], 5) == true) {
          display.fillCircle(30, oled2_y, 4, GREEN);
        } else {
          display.fillCircle(30, oled2_y, 4, BLACK);
        }
      }
      if (bitRead(oled2, 6) != bitRead(pOut[2], 6)) {
        if (bitRead(pOut[2], 6) == true) {
          display.fillCircle(18, oled2_y, 4, GREEN);
        } else {
          display.fillCircle(18, oled2_y, 4, BLACK);
        }
      }
      if (bitRead(oled2, 7) != bitRead(pOut[2], 7)) {
        if (bitRead(pOut[2], 7) == true) {
          display.fillCircle(6, oled2_y, 4, GREEN);
        } else {
          display.fillCircle(6, oled2_y, 4, BLACK);
        }
      }
      oled2 =  pOut[02];
    }

    if (dled != dledO) {
      dledO = dled;
      if (dled == true) {
        display.fillCircle(6, 39, 4, BLUE);
      } else {
        display.fillCircle(6, 39, 4, BLACK);
      }
    }

    delay(1);
  }
}
#endif



//*********************************************************************************************
//****                        HEX Dump 256 byte block of RAM                               ****
//*********************************************************************************************
void dumpRAM(uint16_t s) {
  uint8_t i, ii;
  for (i = 0; i < 16; i++) {
    Serial.printf("%.4X  ", s + i * 16);
    for (ii = 0; ii < 16; ii++) {
      Serial.printf("%.2X ", RAM[s + ii + i * 16]);
    }
    Serial.println();
  }
}

//*********************************************************************************************
//****                       Dump Z80 Register for Breakpoint                              ****
//*********************************************************************************************
void dumpReg(void) {
  bitWrite(Fl, 7 , Sf);
  bitWrite(Fl, 6 , Zf);
  bitWrite(Fl, 4 , Hf);
  bitWrite(Fl, 2 , Pf);
  bitWrite(Fl, 1 , Nf);
  bitWrite(Fl, 0 , Cf);
  V16 = RAM[PC + 1] + 256 * RAM[PC + 2];    //Get the 16 bit operand
  Serial.println();
  Serial.printf("PC: %.4X  %.2X %.2X %.2X (%.2X)\n\r", PC, RAM[PC], RAM[PC + 1], RAM[PC + 2], RAM[V16] );
  Serial.printf("AF: %.2X %.2X\t\tAF': %.2X %.2X \n\r", A, Fl, Aa, Fla);
  Serial.printf("BC: %.2X %.2X (%.2X)\t\tBC': %.2X %.2X \n\r", B, C, RAM[(B * 256) + C], Ba, Ca);
  Serial.printf("DE: %.2X %.2X (%.2X)\t\tDE': %.2X %.2X \n\r", D, E, RAM[(D * 256) + E], Da, Ca);
  Serial.printf("HL: %.2X %.2X (%.2X)\t\tHL': %.2X %.2X \n\r", H, L, RAM[(H * 256) + L], Ha, La);
  Serial.printf("IX: %.4X  IY: %.4X\n\r", IX, IY);
  Serial.printf("SP: %.4X  Top entry: %.4X\n\r", SP, (RAM[SP] + (256 * RAM[SP + 1])));
  Serial.printf("S:%1d  Z:%1d  H:%1d  P/V:%1d  N:%1d  C:%1d\n\r", Sf, Zf, Hf, Pf, Nf, Cf);
}
