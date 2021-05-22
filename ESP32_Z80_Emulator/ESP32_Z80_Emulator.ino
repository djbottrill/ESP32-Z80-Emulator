#include "FS.h"
#include <SPI.h>
#include <SD.h>
#include "SPIFFS.h"
#define SS    13
#define MOSI  15
#define MISO  2
#define SCK   14

bool debug = false;
int debug_delay = 500;

//Virtual GPIO Port A
#define PA0 32
#define PA1 33
#define PA2 25
#define PA3 26
#define PA4 27
#define PA5 12
#define PA6 16
#define PA7 17

//Virtual GPIO Port B
#define PB0 18
#define PB1 19

//BreakPoint switches
#define sw1 4           //Breakpoints on / Off
#define sw2 0           //Single Setp push button

//Virtual GPIO Port
const uint8_t GPP = 0;

//Virtual 8250 UART ports
const uint8_t UART_PORT = 0x80;
const uint8_t UART_LSR  = UART_PORT + 5;

//Virtual disk controller ports
const uint8_t DCMD  = 0x20;
const uint8_t DPARM = 0x30;

//Z80 Registers
uint8_t   A = 0;
uint8_t   Fl = 0;
uint8_t   B = 0;
uint8_t   C = 0;
uint8_t   D = 0;
uint8_t   E = 0;
uint8_t   H = 0;
uint8_t   L = 0;

uint8_t   Aa = 0;
uint8_t   Fla = 0;
uint8_t   Ba = 0;
uint8_t   Ca = 0;
uint8_t   Da = 0;
uint8_t   Ea = 0;
uint8_t   Ha = 0;
uint8_t   La = 0;


uint16_t IX = 0;
uint16_t IY = 0;
uint16_t PC = 0;
uint16_t SP = 0;

//Z80 flags
bool Zf = false;                //Zero flag
bool Cf = false;                //Carry flag
bool Sf = false;                //Sign flag (MSB of A Set)
bool Hf = false;                //Half Carry flag
bool Pf = false;                //Parity / Overflow flag
bool Nf = false;                //Add / Subtract flag

bool RUN = false;               //RUN flag
bool intE = false;              //Interrupt enable
uint16_t BP = 0;                //Breakpoint
uint8_t BPmode = 0;             //Breakpoint mode
bool bpOn = false;              // BP passed flag
uint8_t OC;                     //Opcode store
uint8_t JR;                     //Signed relative jump
uint8_t V8;                     //8 Bit operand temp storge
uint16_t V16;                   //16 Bit operand temp storge
uint16_t V16a;                  //16 Bit operand temp storge
uint32_t V32;                   //32 Bit operand temp storage user for 16 bit addition and subration
uint8_t v1;                     //Temporary storage
uint8_t v2;                     //Temporary storage
bool cfs;                       //Temp carry flag storage

uint8_t RAM[65536] = {};        //RAM
uint8_t pOut[256];              //Output port buffer
uint8_t pIn[256];               //Output port buffer
uint8_t rxBuf[256];             //Serial receive buffer
uint8_t rxInPtr;                //Serial buffer input pointer
uint8_t rxOutPtr;               //Serial buffer output pointer

int vdrive;                     //Virtual drive number
char sdfile[50] = {};           //SD card filename
char sddir[50] = {"/download"}; //SD card path
bool sdfound = true;            //SD Card present flag

TaskHandle_t Task1;             //Task handle
SemaphoreHandle_t baton;        //Baton, currently not used


void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("");
  Serial.println("");
  Serial.println("");
  Serial.write(27);             //Print "esc"
  Serial.println("c");            //Send esc c to reset screen
  Serial.println("****        Shady Grove        ****");
  Serial.println("****  Z80 Emulator for ESP32   ****");
  Serial.println("**** David Bottrill 2021  V1.1 ****");
  Serial.println();

  pinMode(LED_BUILTIN, OUTPUT);             //Built in LED functions as disk active indicator

  //BreakPoint switch inputs
  pinMode(sw1, INPUT_PULLUP);
  pinMode(sw2, INPUT_PULLUP);


  Serial.println("Initialising Virtual GPIO Port");
  portOut(GPP  , 0) ;                        //Port 0 GPIO  A 0 - 7 off
  portOut(GPP + 1, 255) ;                    //Port 1 GPIO A 0 - 7 Outputs
  portOut(GPP + 2, 0) ;                      //Port 0 GPIO B 1 & 1 off
  portOut(GPP + 3, 255) ;                    //Port 1 GPIO B 0 & 1 Outputs

  Serial.println("Initialising Virtual 6850 UART");
  //Initialise virtual 6850 UART
  pIn[UART_LSR] = 0x40;                       //Set bit to say TX buffer is empty


  //Start the Serial receive char task
  baton = xSemaphoreCreateMutex();

  xTaskCreatePinnedToCore(
    serialTask,
    "SerialTask",
    3000,
    NULL,
    1,
    &Task1,
    0);       // Core 0

  delay(500);  // needed to start-up task1

  Serial.println("Initialising Virtual Disk Controller");
  if (!SD.begin()) {
    Serial.println("SD Card Mount Failed");
    sdfound = false;
  }

  //Try and boot from SD Card or SPIFFS
  File boot;

  if (sdfound == true) {
    Serial.println("Booting from SD Card");
    boot = SD.open("/boot.txt");
    if (boot == 0) Serial.println("boot.txt not found");
  } else {

    Serial.println("Booting from SPIFFS");
    if (!SPIFFS.begin(true)) {
      Serial.println("An Error has occurred while mounting SPIFFS");

    }
    boot = SPIFFS.open("/boot.txt");
    if (boot == 0) {
      Serial.println("boot.txt not found");
      Serial.println("No bootable media found");
      while (1);
    }
  }

  //Read boot.txt and load files into RAM
  char linebuf[10][50];
  char c;
  int n = 0;
  int l = 0;
  if (boot) {
    while (boot.available()) {
      c = boot.read();
      if (c != '\n') {
        linebuf[l][n] = c;
        n++;
      } else {
        if (l < 10) {             //We can only store 10 load commands
          linebuf[l][n] = 0;      //Terminate string
          //Serial.println(linebuf[l]);
          n = 0;
          if (linebuf[l][0] == '/') l++;  //Ignore lines that dont start with /
        }
      }
    }
  }
  int i = 0;
  for (i = 0; i < l; i++) {
    //Serial.println(linebuf[i]);
    int z = 0;
    char Fs[20] = {};
    while (linebuf[i][z] != ' ') {
      Fs[z] = linebuf[i][z];
      z++;
    }
    //Serial.println(Fs);
    int zs = z + 1;
    while (linebuf[i][zs] == ' ') zs++; //find the first hex digit
    int zf = zs;
    char AA[5] = {};
    while (linebuf[i][zf] != 0) {
      AA[zf - zs] = linebuf[i][zf];
      zf++; //find the end of the address string
    }
    //Serial.println(AA);
    uint32_t Add = strtoul(AA, NULL, 16);
    FileToRAM(Fs, Add, sdfound);  //False means load from SPIFFS
  }
  boot.close();


  PC = 0;
  BP = 0x0000;
  BPmode = 0;


  if (digitalRead(sw1) == 0) {
    Serial.println("Breakpoint mode: press step button to start");
    while (digitalRead(sw2) == 1);
    delay(100);
    while (digitalRead(sw2) == 0);
    delay(100);
    Serial.println("\n\rStarting Z80 with breakpoints enabled\n\r");

    //Main loop in breakpoint mode, runs about 50% slower than normal mode
    while (1) {
      //Single Step and breakpoint logic
      if (digitalRead(sw1) == 0) {
        if (bpOn == false && PC == BP) bpOn = true;
        switch (BPmode) {
          case 0:
            BreakPoint();    //Singe step if SW1 is on (low)
            break;
          case 1:
            if (bpOn == true) BreakPoint();    //Stop once breakpoint hit
            break;
          case 2:
            if (PC == BP) BreakPoint();        //Stop only at breakpoint
            break;
        }
      } else {
        //If not in single step mode then step button becomes a reset button.
        if (digitalRead(sw2) == 0) {
          PC = 0;
          while (digitalRead(sw2) == 0);
        }
      }
      cpu();                              //Execute next instruction
    }
  } else {
    //Main loop in normal mode
    Serial.println("\n\rStarting Z80\n\r");
    while (1) cpu();                             //Execute next instruction
  }
}


void loop() {
  //Never gets here !
}


void BreakPoint(void) {
  dumpReg();
  while (digitalRead(sw2) == 1 && digitalRead(sw1) == 0);
  delay(100);                                     //Crude de-bounce
  while (digitalRead(sw2) == 0 );                 //Wait for single step button to be released
  delay(100);                                     //Crude de-bounce
}


uint8_t get8(void) {
  uint8_t v = RAM[PC];
  PC++;
  return (v);
}

uint16_t get16(void) {
  uint16_t V = RAM[PC];
  PC++;
  V += (256 * RAM[PC]);
  PC++;
  return (V);
}

void calcP(uint8_t v) {                 //Calc Parity and set flag
  uint8_t i;
  uint8_t z = 0;
  for (i = 0; i < 8; i++) {
    if (bitRead(v, i) == 1) z++;
  }
  Pf = !bitRead(z, 0);
}

void portOut(uint8_t p, uint8_t v) {
  pOut[p] = v;                     //Save a copy of byte sent to IO Port
  switch (p) {
    case GPP:                                     //Write to GPIO Port
      pIn[GPP] = v;                            //Copy to read buffer so it can be read.
      digitalWrite(PA0, bitRead(v, 0));
      digitalWrite(PA1, bitRead(v, 1));
      digitalWrite(PA2, bitRead(v, 2));
      digitalWrite(PA3, bitRead(v, 3));
      digitalWrite(PA4, bitRead(v, 4));
      digitalWrite(PA5, bitRead(v, 5));
      digitalWrite(PA6, bitRead(v, 6));
      digitalWrite(PA7, bitRead(v, 7));
      break;
    case GPP+1:                                //GPIO Direction Port
      pIn[GPP + 1] = v;                        //Copy to read buffer so it can be read.
      if (bitRead(v, 0) == 1) pinMode(PA0, OUTPUT); else pinMode(PA0, INPUT_PULLUP);
      if (bitRead(v, 1) == 1) pinMode(PA1, OUTPUT); else pinMode(PA1, INPUT_PULLUP);
      if (bitRead(v, 2) == 1) pinMode(PA2, OUTPUT); else pinMode(PA2, INPUT_PULLUP);
      if (bitRead(v, 3) == 1) pinMode(PA3, OUTPUT); else pinMode(PA3, INPUT_PULLUP);
      if (bitRead(v, 4) == 1) pinMode(PA4, OUTPUT); else pinMode(PA4, INPUT_PULLUP);
      if (bitRead(v, 5) == 1) pinMode(PA5, OUTPUT); else pinMode(PA5, INPUT_PULLUP);
      if (bitRead(v, 6) == 1) pinMode(PA6, OUTPUT); else pinMode(PA6, INPUT_PULLUP);
      if (bitRead(v, 7) == 1) pinMode(PA7, OUTPUT); else pinMode(PA7, INPUT_PULLUP);
      break;
    case GPP+2:
      pIn[GPP + 2] = v;                        //Copy to read buffer so it can be read.
      digitalWrite(PB0, bitRead(v, 0));
      digitalWrite(PB1, bitRead(v, 1));
      break;
    case GPP+3:                                //GPP Direction Port
      pIn[GPP + 3] = v;                        //Copy to read buffer so it can be read.
      if (bitRead(v, 0) == 1) pinMode(PB0, OUTPUT); else pinMode(PB0, INPUT_PULLUP);
      if (bitRead(v, 1) == 1) pinMode(PB1, OUTPUT); else pinMode(PB1, INPUT_PULLUP);
      break;
    case UART_PORT:                             //UART Write
      Serial.write(v);                          //Send Char
      bitWrite(pIn[UART_LSR], 6, 1);            //Set bit to indicate sent
      break;

    case DCMD:
      pIn[p] = v;
      if (sdfound == true) {
        switch (v) {
          case 1: diskRead(SD);   pIn[DCMD] = 0; break;
          case 2: diskWrite(SD);  pIn[DCMD] = 0; break;
          case 4: SDfileOpen(SD); pIn[DCMD] = 0; break;
          case 5: SDfileRead(SD); pIn[DCMD] = 0; break;
          case 6: SDprintDir(SD); pIn[DCMD] = 0; break;
          case 7: SDsetPath(SD);  pIn[DCMD] = 0; break;
          default: Serial.printf("Unknown Disk Command: 0x%.2X\n\r", v); break;
        }
      }
      break;
    case DPARM:   break;
    case DPARM+1: break;
    case DPARM+2: break;
    case DPARM+3: break;
    case DPARM+4: break;
    case DPARM+5: break;
    default:                                    //Just print data
      //Serial.printf("Port Out: %.2X  Val: %.2X PC: %.4X\n\r", p, v, PC);
      break;
  }
}

void serialTask( void * parameter ) {
  for (;;) {
    //if (Serial.available()) {
    //  rxBuf[rxInPtr] = Serial.read();           //Store received char in buffer
    //  rxInPtr++;                                //Inc pointer
    //}
    int c = Serial.read();
    if (c >= 0) {
      rxBuf[rxInPtr] = c;
      rxInPtr++;
    }
    delay(5);
  }
}


uint8_t portIn(uint8_t p) {
  switch (p) {
    case GPP:        //Read GPIO port if Direction be is 0 (Input)
      if (bitRead(pOut[GPP + 1], 0) == 0) bitWrite(pIn[GPP], 0, digitalRead(PA0));
      if (bitRead(pOut[GPP + 1], 1) == 0) bitWrite(pIn[GPP], 1, digitalRead(PA1));
      if (bitRead(pOut[GPP + 1], 2) == 0) bitWrite(pIn[GPP], 2, digitalRead(PA2));
      if (bitRead(pOut[GPP + 1], 3) == 0) bitWrite(pIn[GPP], 3, digitalRead(PA3));
      if (bitRead(pOut[GPP + 1], 4) == 0) bitWrite(pIn[GPP], 4, digitalRead(PA4));
      if (bitRead(pOut[GPP + 1], 5) == 0) bitWrite(pIn[GPP], 5, digitalRead(PA5));
      if (bitRead(pOut[GPP + 1], 6) == 0) bitWrite(pIn[GPP], 6, digitalRead(PA6));
      if (bitRead(pOut[GPP + 1], 7) == 0) bitWrite(pIn[GPP], 7, digitalRead(PA7));
      break;
    case GPP+2:      //Read GPIO port if Direction be is 0 (Input)
      if (bitRead(pOut[GPP + 3], 0) == 0) bitWrite(pIn[GPP + 2], 0, digitalRead(PB0));
      if (bitRead(pOut[GPP + 3], 1) == 0) bitWrite(pIn[GPP + 2], 1, digitalRead(PB1));
      break;

    case UART_PORT:   //Read virtual 8250 UART LSR
      bitWrite(pIn[UART_LSR], 0, 0);  //clear bit to say char has been read
      break;

    case UART_LSR:    //Check for received char
      if (rxOutPtr != rxInPtr) {              //Have we received any chars?
        pIn[UART_PORT] = rxBuf[rxOutPtr];     //Put char in UART port
        rxOutPtr++;                          //Inc Output buffer pointer
        bitWrite(pIn[UART_LSR], 0, 1);       //Set bit to say char can be read
      }
      break;
    case DCMD:
      break;
    default:
      Serial.printf("Port In: %.2X  Val: %.2X PC: %.4X\n\r", p, pIn[p], PC);
      break;
  }
  return (pIn[p]);                            //Return buffered port input
}


bool  SDprintDir(fs::FS & fs) {                                     // Show files in /download
  //File dir = SD.open(sddir, FILE_READ);
  File dir = fs.open(sddir, FILE_READ);

  dir.rewindDirectory();
  int cols = 0;
  Serial.println(sddir);
  while (true) {
    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }

    String st = entry.name();
    if (st.charAt(0) != '_') {                            // Skip deleted files
      Serial.print(st);
      if (st.length() < 8) Serial.print("\t");            // Extra tab if the filename is short
      if (entry.isDirectory()) {
        Serial.print("\t   DIR");
      } else {
        // files have sizes, directories do not
        Serial.print("\t");
        //Serial.print(entry.size(), DEC);
        Serial.printf("%6d", entry.size());
      }
      cols++;
      if (cols < 3) {
        Serial.print("\t");
      } else {
        Serial.println();
        cols = 0;
      }
    }
    entry.close();
  }
  dir.close();
}

bool  SDsetPath(fs::FS & fs) {                      // Set SD Card path for file download
  int a = pOut[DPARM + 1] << 8 | pOut[DPARM];       // Get buffer address as this contains the path

  int l = RAM[a];                                   // The first byte in the buffer contains the byte count
  int i;
  int j = 0;
  char d[50] = {};
  for (i = 1 ; i <= l; i++) {
    d[j] = RAM[a + i];                              // Read directory path
    if (d[j] != 0x20) j++;                          // Skip amy leading spaces
  }
  Serial.print("New Path: ");
  Serial.println(d);
  //File dir = SD.open(d);                            // Check if path is valid
  File dir = fs.open(d, FILE_READ);                    // Check if path is valid
  if (!dir) {
    Serial.println("Invalid path");
    return (false);
  }
  dir.close();
  for (i = 0 ; i < 50; i++) sddir[i] = d[i];          // Copy path to global variable
  Serial.print("Setting SD Card path to: ");
  Serial.println(sddir);
  return (true);
}


void SDfileOpen(fs::FS & fs) {
  uint16_t sdbuffer;
  sdbuffer = pOut[DPARM + 1] << 8 | pOut[DPARM];       // Get buffer address
  int n;
  int nn;
  for (nn = 0 ; nn < 50 ; nn++) {
    sdfile[nn] = sddir[nn];                               // Get current directory
    if (sddir[nn] == 0) break;                            // Null so end of string
  }
  sdfile[nn] = '/';                                        // append a /
  nn++;

  for (n = 0; n < 12; n++) sdfile[n + nn] = pOut[DPARM + n + 2];  // Append filename
  unsigned int blocks = 0;
  Serial.printf("open: %s ", sdfile);
  //File f = SD.open(sdfile, FILE_READ);
  File f = fs.open(sdfile, FILE_READ);
  if (!f) {
    Serial.println("file open failed");
  } else {
    blocks = f.size() / 128;                              // Calculate the number of 128 byte blocks
    if ((f.size() % 128) > 0) blocks++;                   // Round up if not an exact number of 128 byte blocks
    Serial.printf("%7d Bytes %5d Block(s)", f.size(), blocks);
  }
  Serial.println();
  f.close();

  RAM[sdbuffer + 1] = blocks >> 8;
  RAM[sdbuffer]     = blocks & 0xff;                      // Write the number of blocks to the buffer
}

bool SDfileRead(fs::FS & fs) {                            // Read block from file command
  int s = pOut[DPARM + 3] << 8 | pOut[DPARM + 2];         // Block Number
  int a = pOut[DPARM + 1] << 8 | pOut[DPARM];             // Get buffer address
  //File f = SD.open(sdfile);
  File f = fs.open(sdfile, FILE_READ);
  if (!f) {
    Serial.println("file open failed");
    return (false);
  }
  unsigned long fz = f.size();                            // Get file size
  unsigned long fr = s * 128;                             // Calculate block start
  f.seek(fr);                                             // Seek to first byte of block
  for (int i = 0 ; i < 128 ; i++) {
    if (fr + i >= fz) {
      RAM[a + i] = 0;                          // Pad out the last sector with zeros if past end of file
    } else {
      RAM[a + i] = f.read();                   // Else read next byte from file
    }
  }
  f.close();
  return (true);
}

bool diskWrite(fs::FS & fs) {
  digitalWrite(LED_BUILTIN, HIGH);
  uint32_t s = pOut[DPARM + 4] << 16 | pOut[DPARM + 3] << 8 | pOut[DPARM + 2];
  uint16_t a = pOut[DPARM + 1] << 8 | pOut[DPARM];
  vdrive = s >> 14;
  s = s & 0x3fff;
  char dd[] = "/disks/A.dsk";
  dd[7] = vdrive + 65;
  //File f = SD.open(dd, FILE_APPEND);
  File f = fs.open(dd, "r+w+");
  if (!f) {
    Serial.println("Virtual Disk file open failed");
    return (false);
  }
  f.seek(s * 512);                                          // Seek to first byte of sector
  //16 Disks with 512 tracks, 128 Sectors per track, 128 byte per sector
  for (int i = 0 ; i < 512 ; i++) {
    //f.write(secBuf[i]);
    f.write(RAM[a + i]);
  }
  f.close();
  digitalWrite(LED_BUILTIN, LOW);
  return (true);
}


bool diskRead(fs::FS & fs) {
  digitalWrite(LED_BUILTIN, HIGH);
  uint32_t s = pOut[DPARM + 4] << 16 | pOut[DPARM + 3] << 8 | pOut[DPARM + 2];
  uint16_t a = pOut[DPARM + 1] << 8 | pOut[DPARM];
  // Memory Address, Page bit, Disk Drive, Sector
  vdrive = s >> 14;
  s = s & 0x3fff;
  char dd[] = "/disks/A.dsk";
  dd[7] = vdrive + 65;
  //File f = SD.open(dd, FILE_READ);
  File f = fs.open(dd, FILE_READ);
  if (!f) {
    Serial.println("Virtual Disk file open failed");
    return (false);
  }
  f.seek(s * 512);                                         // Seek to first byte of sector
  //16 Disks with 512 tracks, 128 Sectors per track, 128 byte per sector
  for (int i = 0 ; i < 512 ; i++) {
    //secBuf[i] = f.read();
    RAM[a + i] = f.read();
  }
  f.close();
  //for (int i = 0 ; i < 512 ; i++) {
  //  RAM[a + i] = secBuf[i];
  //}
  digitalWrite(LED_BUILTIN, LOW);
  //Serial.printf("Read Sector Buffer: %d Drive: %s\n\r", s, dd);
  //dumpRAM(a);
  //dumpRAM(a+256);
  digitalWrite(LED_BUILTIN, LOW);
  return (true);
  return (true);
}



//Read file from SD Card or SPIFFS into RAM
void FileToRAM(char c[], uint16_t l, bool sd) {
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.printf("Loading: %s at 0x%04x ", c, l);
  File f;
  if (sd == true) {
    f = SD.open(c, FILE_READ);
  } else {
    f = SPIFFS.open(c, FILE_READ);
  }
  if (!f) {
    Serial.println("file open failed");
  } else {
    Serial.printf("%5d bytes... ", f.size());
  }
  uint16_t a = 0;
  for (a = 0; a < f.size(); a++ ) RAM[l + a] = f.read();
  Serial.println("Done");
  f.close();
  digitalWrite(LED_BUILTIN, LOW);
}




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

void dumpReg(void) {
  bitWrite(Fl, 7 , Sf);
  bitWrite(Fl, 6 , Zf);
  bitWrite(Fl, 4 , Hf);
  bitWrite(Fl, 2 , Pf);
  bitWrite(Fl, 1 , Nf);
  bitWrite(Fl, 0 , Cf);
  Serial.println();
  Serial.printf("PC: %.4X  %.2X %.2X %.2X %.2x\n\r", PC, RAM[PC], RAM[PC + 1], RAM[PC + 2], RAM[PC + 3]);
  Serial.printf("AF: %.2X %.2X\t\tAF': %.2X %.2X \n\r", A, Fl, Aa, Fla);
  Serial.printf("BC: %.2X %.2X\t\tBC': %.2X %.2X \n\r", B, C, Ba, Ca);
  Serial.printf("DE: %.2X %.2X\t\tDE': %.2X %.2X \n\r", D, E, Da, Ca);
  Serial.printf("HL: %.2X %.2X\t\tHL': %.2X %.2X \n\r", H, L, Ha, La);
  Serial.printf("IX: %.4X  IY: %.4X\n\r", IX, IY);
  Serial.printf("SP: %.4X  Top entry: %.4X\n\r", SP, (RAM[SP] + (256 * RAM[SP + 1])));
  Serial.printf("S:%1d  Z:%1d  H:%1d  P/V:%1d  N:%1d  C:%1d\n\r", Sf, Zf, Hf, Pf, Nf, Cf);
}


uint8_t SUB8(uint8_t a, uint8_t v, bool c) {
  uint16_t vv;
  vv = a - (v + c);
  a = vv & 0xff;
  Cf = bitRead(vv, 8);
  if (vv > 255) {
    Pf = true;
  } else {
    Pf = false;
  }
  Hf = false;                //Not sure about this !!
  if (a == 0) {
    Zf = true;
  } else {
    Zf = false;
  }
  Nf = true;
  Sf = bitRead(a, 7);
  return (a);
}

uint8_t ADD8(uint8_t a, uint8_t v, bool c) {
  if ((a & 0x0f) == 0x0f) Hf = true;          //Not 100% correct but hopefully near enough
  uint16_t vv, vc, ra;
  bool vvp;
  vc = v + c;
  vv = a + vc;
  ra = vv & 0xff;
  vvp = bitRead(a, 7) ^ bitRead(vc, 7);
  if (vvp = true) {
    Pf = false;
  } else {
    if (bitRead(vv, 7) ^ bitRead(a, 7) == true || bitRead(vv, 7) ^ bitRead(vc, 7) == true) {
      Pf = true;
    }
  }

  Cf = bitRead(vv, 8);     //Carry
  //  if (vv > 255) {
  //    Pf = true;
  //  } else {
  //    Pf = false;
  //  }
  if (ra == 0) {
    Zf = true;
  } else {
    Zf = false;
  }
  Nf = false;
  Sf = bitRead(ra, 7);
  return (ra);
}

void cpu(void) {
  OC = RAM[PC];                 //Get Opcode
  PC++;                         //Increment Program counter
  switch (OC)  {                //Switch based on OPCode
    //********************************************
    // Instructions 00 to 0F fully implemented
    //********************************************
    case 0x00:                  //** NOP **
      break;
    case 0x01:                  //** LD BC, VALUE **
      C = get8();
      B = get8();
      break;
    case 0x02:                  //** LD (BC), A **
      RAM[(256 * B) + C] = A;
      break;
    case 0x03:                  //** INC BC **
      V16 = C;
      V16 += 256 * B;
      V16++;
      B = V16 / 256;
      C = V16 & 255;
      break;
    case 0x04:                  //** INC B **
      cfs = Cf;
      B = ADD8(B, 1, 0);
      Cf = cfs;
      break;
    case 0x05:                  //** DEC B **
      cfs = Cf;
      B = SUB8(B, 1, 0);
      Cf = cfs;
      break;
    case 0x06:                  //** LD B, value **
      B = get8();
      break;
    case 0x07:                  //** RLCA **
      Cf = bitRead(A, 7);
      A = A << 1;
      bitWrite(A, 0, Cf);
      Hf == false;
      Nf == false;
      break;
    case 0x08:                  //** EX AF AF' **
      bitWrite(Fl, 7 , Sf);
      bitWrite(Fl, 6 , Zf);
      bitWrite(Fl, 4 , Hf);
      bitWrite(Fl, 2 , Pf);
      bitWrite(Fl, 1 , Nf);
      bitWrite(Fl, 0 , Cf);
      V8 = A;
      A = Aa;
      Aa = V8;
      V8 = Fl;
      Fl = Fla;
      Fla = V8;
      Sf = bitRead(Fl, 7);
      Zf = bitRead(Fl, 6);
      Hf = bitRead(Fl, 4);
      Pf = bitRead(Fl, 2);
      Nf = bitRead(Fl, 1);
      Cf = bitRead(Fl, 0);
      break;
    case 0x09:                  //** ADD HL, BC **
      V32 = (H * 256) + L;
      V16 = (B * 256) + C;
      V32 += V16;
      Cf = bitRead(V32, 16);    //** Update carry flag **  *
      H = (V32 / 256) & 0xff;
      L = V32 & 0xff;
      Nf = false;               //False as it's an addition
      break;
    case 0x0A:                  //** LD A, (BC) **
      A = RAM[(B * 256) + C];
      break;
    case 0x0B:                  //** DEC BC **
      V16 = C + (256 * B);
      V16--;
      B = V16 / 256;
      C = V16 & 255;
      break;
    case 0x0C:                  //** INC C **
      cfs = Cf;
      C = ADD8(C, 1, 0);
      Cf = cfs;
      break;
    case 0x0D:                  //** DEC C **
      cfs = Cf;
      C = SUB8(C, 1, 0);
      Cf = cfs;
      break;
    case 0x0E:                  //** LD C, value **
      C = get8();
      break;
    case 0x0F:                  //** RRCA **
      Cf = bitRead(A, 0);
      A = A >> 1;
      bitWrite(A, 7, Cf);
      Hf = false;
      Nf = false;
      break;
    //********************************************

    //********************************************
    // Instructions 10 to 1F fully implemented
    //********************************************
    case 0x10:                  //** DJNZ, value **
      JR = get8();
      B--;
      if (B != 0) {
        if (JR < 128) {
          PC += JR;               //Forward Jump
        } else {
          PC = PC - (256 - JR);   //Backward jump
        }
      }
      break;
    case 0x11:                  //** LD DE, VALUE **
      E = get8();
      D = get8();
      break;
    case 0x12:                  //** LD (DE), A **
      RAM[(256 * D) + E] = A;
      break;
    case 0x13:                  //** INC DE **
      V16 = E + (256 * D);
      V16++;
      D = V16 / 256;
      E = V16 & 255;
      break;
    case 0x14:                  //** INC D **
      cfs = Cf;
      D = ADD8(D, 1, 0);
      Cf = cfs;
      break;
    case 0x15:                  //** DEC D **
      cfs = Cf;
      D = SUB8(D, 1, 0);
      Cf = cfs;
      break;
    case 0x16:                  //** LD D, value **
      D = get8();
      break;
    case 0x17:                  //** RLA **
      cfs = Cf;                 //Save carry flag
      Cf = bitRead(A, 7);       //Bit 7 becomes new carry
      A = A << 1;               //Shift left
      bitWrite(A, 0, cfs);      //bit 0 becomes old carry
      Hf == false;
      Nf == false;
      break;
    case 0x18:                  //** JR offset **
      JR = get8();
      if (JR < 128) {
        PC += JR;               //Forward Jump
      } else {
        PC = PC - (256 - JR);   //Backward jump
      }
      break;
    case 0x19:                  //** ADD HL, DE **
      V32 = (H * 256) + L;
      V16 = (D * 256) + E;
      V32 += V16;
      Cf = bitRead(V32, 16);    //Update carry flag
      H = (V32 / 256) & 0xff;
      L = V32 & 0xff;
      Nf = false;               //False as it's an addition
      break;
    case 0x1A:                  //** LD A, (DE) **
      A = RAM[(D * 256) + E];
      break;
    case 0x1B:                  //** DEC DE **
      V16 = E + (256 * D);
      V16--;
      D = V16 / 256;
      E = V16 & 255;
      break;
    case 0x1C:                  //** INC E **
      cfs = Cf;
      E = ADD8(E, 1, 0);
      Cf = cfs;
      break;
    case 0x1D:                  //** DEC E **
      cfs = Cf;
      E = SUB8(E, 1, 0);
      Cf = cfs;
      break;
    case 0x1E:                  //** LD E, value **
      E = get8();
      break;
    case 0x1F:                  //** RR A **
      cfs = Cf;                 //Save carry flag
      Cf = bitRead(A, 0);       //Update carry flag
      A = A >> 1;               //Shift right
      bitWrite(A, 7, cfs);      //add in the old carry flag
      Hf = false;
      Nf = false;
      break;
    //********************************************

    //********************************************
    // Instructions 20 to 2F fully implemented
    //********************************************
    case 0x20:                  //** JR NZ , value **
      JR = get8();
      if (Zf == false) {
        if (JR < 128) {
          PC += JR;               //Forward Jump
        } else {
          PC = PC - (256 - JR);   //Backward jump
        }
      }
      break;
    case 0x21:                  //** LD HL, VALUE **
      L = get8();
      H = get8();
      break;
    case 0x22:                  //** LD (value), HL **
      V16 = get16();
      RAM[V16] = L;
      RAM[V16 + 1] = H;
      break;
    case 0x23:                  //** INC HL **
      V16 = L + (256 * H);
      V16++;
      H = V16 / 256;
      L = V16 & 255;
      break;
    case 0x24:                  //** INC H **
      cfs = Cf;
      H = ADD8(H, 1, 0);
      Cf = cfs;
      break;
    case 0x25:                  //** DEC H **
      cfs = Cf;
      H = SUB8(H, 1, 0);
      Cf = cfs;
      break;
    case 0x26:                  //** LD H, value **
      H = get8();
      break;
    case 0x27:                  //** DAA **
      if ((A & 0x0f) > 9 || Hf == true) A = A + 0x06;
      if (((A & 0xf0) >> 4) > 9 || Cf == true) {
        A = A + 0x60;
        Cf = true;
      } else {
        Cf = false;
      }
      Zf = false;
      if (A == 0) Zf = true;
      calcP(A);                  //update parity flag
      Sf = bitRead(A, 7);       //Update S flag
      break;
    case 0x28:                  //** JR Z value **
      JR = get8();
      if (Zf == true) {
        if (JR < 128) {
          PC += JR;               //Forward Jump
        } else {
          PC = PC - (256 - JR);   //Backward jump
        }
      }
      break;
    case 0x29:                  //** ADD HL, HL **
      V32 = (H * 256) + L;
      V32 = V32 + (H * 256) + L;            //add it again
      Cf = bitRead(V32, 16);    //** Update carry flag **  *
      H = (V32 / 256) & 0xff;
      L = V32 & 0xff;
      Nf = false;               //False as it's an addition
      break;
    case 0x2A:                  //** LD HL, (value) **
      V16 = get16();
      L = RAM[V16];
      H = RAM[V16 + 1];
      break;
    case 0x2B:                  //** DEC HL **
      V16 = L + (256 * H);
      V16--;
      H = V16 / 256;
      L = V16 & 255;
      break;
    case 0x2C:                  //** INC L **
      cfs = Cf;
      L = ADD8(L, 1, 0);
      Cf = cfs;
      break;
    case 0x2D:                  //** DEC L **
      cfs = Cf;
      L = SUB8(L, 1, 0);
      Cf = cfs;
      break;
    case 0x2E:                  //** LD L, value **
      L = get8();
      break;
    case 0x2f:                  //** CPL **
      A = ~ A ;                //Invert all bits in A
      Hf = true;
      Nf = true;
      break;
    //********************************************

    //********************************************
    // Instructions 30 to 3F fully implemented
    //********************************************
    case 0x30:                  //**JR NC , value **
      JR = get8();
      if (Cf == false) {
        if (JR < 128) {
          PC += JR;               //Forward Jump
        } else {
          PC = PC - (256 - JR);   //Backward jump
        }
      }
      break;
    case 0x31:                  //** LD SP, VALUE **
      SP = get16();
      break;
    case 0x32:                  //** LD (VALUE), A **
      RAM[get16()] = A;
      break;
    case 0x33:                  //** INC SP **
      SP++;
      break;
    case 0x34:                  //** INC (HL) **
      cfs = Cf;
      V8 = RAM[(H * 256) + L];             //Get value from memory
      V8 = ADD8(V8, 1, 0);
      RAM[(H * 256) + L] = V8;            //Save back
      Cf = cfs;
      break;
    case 0x35:                  //** DEC (HL) **
      cfs = Cf;
      V8 = RAM[(H * 256) + L];             //Get value from memory
      V8 = SUB8(V8, 1, 0);
      RAM[(H * 256) + L] = V8;            //Save back
      Cf = cfs;
      break;
    case 0x36:                  //** LD (HL), VALUE **
      RAM[(H * 256) + L] = get8();
      break;
    case 0x37:                  //** SCF **
      Cf = true;
      Nf = false;
      Hf = false;
      break;
    case 0x38:                  //** JR C , value ***
      JR = get8();
      if (Cf == true) {
        if (JR < 128) {
          PC += JR;               //Forward Jump
        } else {
          PC = PC - (256 - JR);   //Backward jump
        }
      }
      break;
    case 0x39:                  //** ADD HL, SP **
      V32 = (H * 256) + L;
      V32 += SP;
      Cf = bitRead(V32, 16);    //** Update carry flag **  *
      H = (V32 / 256) & 0xff;
      L = V32 & 0xff;
      Nf = false;               //False as it's an addition
      Sf = bitRead(V32 , 15);      //Update S flag
      Zf = false;
      if ((V32 & 0xffff) == 0) Zf = true; //Set Z flag if HL = 0
      break;
    case 0x3A:                  //** LD A, (VALUE) **
      A = RAM[get16()];
      break;
    case 0x3B:                  //** DEC SP **
      SP--;
      break;
    case 0x3C:                  //** INC A **
      cfs = Cf;
      A = ADD8(A , 1, 0);
      Cf = cfs;
      break;
    case 0x3D:                  //** DEC A **
      cfs = Cf;
      A = SUB8(A , 1, 0);
      Cf = cfs;
      break;
    case 0x3E:                  //** LD A, VALUE **
      A = get8();
      break;
    case 0x3f:                  //** CCF **
      Cf = ! Cf;
      break;
    //********************************************

    //********************************************
    // Instructions 40 to 4F fully implemented
    //********************************************
    case 0x40:                  //** LD B , B **
      break;                                //Does nothing
    case 0x41:                  //** LD B, C **
      B = C;
      break;
    case 0x42:                  //** LD B, D **
      B = D;
      break;
    case 0x43:                  //** LD B, E **
      B = E;
      break;
    case 0x44:                  //** LD B, H **
      B = H;
      break;
    case 0x45:                  //** LD B, L **
      B = L;
      break;
    case 0x46:                  //** LD B, (HL) **
      B = RAM[(256 * H) + L];
      break;
    case 0x47:                  //** LD B, A **
      B = A;
      break;
    case 0x48:                  //** LD C, B **
      C = B;
      break;
    case 0x49:                  //** LD C, C **
      break;                                            //Does nothing
    case 0x4A:                  //** LD C , D **
      C = D;
      break;
    case 0x4B:                  //**  LD C, E **
      C = E;
      break;
    case 0x4C:                  //** LD C, H **
      C = H;
      break;
    case 0x4D:                  //** LD C, L **
      C = L;
      break;
    case 0x4E:                  //** LD C, (HL) **
      C = RAM[(H * 256) + L];
      break;
    case 0x4F:                  //** LD C, A **
      C = A;
      break;
    //********************************************

    //********************************************
    // Instructions 50 to 5F fully implemented
    //********************************************
    case 0x50:                  //** LD D, B **
      D = B;
      break;
    case 0x51:                  //** LD D, C **
      D = C;
      break;
    case 0x52:                  //** LD D, D **
      break;                                    //Does nothing !
    case 0x53:                  //** LD D, E **
      D = E;
      break;
    case 0x54:                  //** LD D, H **
      D = H;
      break;
    case 0x55:                  //**LD D, L **
      D = L;
      break;
    case 0x56:                  //**LD D, (HL) **
      D = RAM[(H * 256) + L];
      break;
    case 0x57:                  //** LD D, A **
      D = A;
      break;
    case 0x58:                  //** LD E, B **
      E = B;
      break;
    case 0x59:                  //** LD E, C **
      E = C;
      break;
    case 0x5A:                  //** LD E, D **
      E = D;
      break;
    case 0x5B:                  //** LD E, E **
      break;                                        //Does nothing
    case 0x5C:                  //** LD E, H **
      E = H;
      break;
    case 0x5D:                  //** LD E, L **
      E = L;
      break;
    case 0x5E:                  //** LD E, (HL) **
      E = RAM[(H * 256) + L];
      break;
    case 0x5F:                  //** LD E, A **
      E = A;
      break;
    //********************************************

    //********************************************
    // Instructions 60 to 6F fully implemented
    //********************************************
    case 0x60:                  //** LD H, B **
      H = B;
      break;
    case 0x61:                  //** LD H, C **
      H = C;
      break;
    case 0x62:                  //** LD H, D **
      H = D;
      break;
    case 0x63:                  //** LD H, E **
      H = E;
      break;
    case 0x64:                  //** LD H, H **
      break;
    case 0x65:                  //** LD H, L **
      H = L;
      break;
    case 0x66:                  //** LD H, (HL) **
      H = RAM[(256 * H) + L];
      break;
    case 0x67:                  //** LD H, A **
      H = A;
      break;
    case 0x68:                  //** LD L, B **
      L = B;
      break;
    case 0x69:                  //** LD L, C **
      L = C;
      break;
    case 0x6A:                  //** LD L, D **
      L = D;
      break;
    case 0x6B:                  //** LD L, E **
      L = E;
      break;
    case 0x6C:                  //** LD L, H **
      L = H;
      break;
    case 0x6D:                  //** LD L, L **
      break;
    case 0x6E:                  //** LD L, (HL) **
      L = RAM[(256 * H) + L];
      break;
    case 0x6F:                  //** LD L, A **
      L = A;
      break;
    //********************************************

    //********************************************
    // Instructions 70 to 7F fully implemented
    //********************************************
    case 0x70:                  //** LD (HL), B **
      RAM[(H * 256) + L] = B;
      break;
    case 0x71:                  //** LD (HL), C **
      RAM[(H * 256) + L] = C;
      break;
    case 0x72:                  //** LD (HL), D **
      RAM[(H * 256) + L] = D;
      break;
    case 0x73:                  //** LD (HL), E **
      RAM[(H * 256) + L] = E;
      break;
    case 0x74:                  //** LD (HL), H **
      RAM[(H * 256) + L] = H;
      break;
    case 0x75:                  //** LD (HL), L **
      RAM[(H * 256) + L] = L;
      break;
    case 0x76:                  //** HALT **
      RUN = false;
      break;
    case 0x77:                  //** LD (HL), A **
      RAM[(H * 256) + L] = A;
      break;
    case 0x78:                  //** LD A, B **
      A = B;
      break;
    case 0x79:                  //** LD A, C **
      A = C;
      break;
    case 0x7A:                  //** LD A, D **
      A = D;
      break;
    case 0x7B:                  //** LD A, E **
      A = E;
      break;
    case 0x7C:                  //** LD A, H **
      A = H;
      break;
    case 0x7D:                  //** LD A, L **
      A = L;
      break;
    case 0x7E:                  //** LD A, (HL) **
      A = RAM[(H * 256) + L];
      break;
    case 0x7f:                  //** LD A, A **
      break;                                      //useless instruction
    //********************************************

    //********************************************
    // Instructions 80 to 8F fully implemented
    //********************************************
    case 0x80:                  //** ADD A, B **
      A = ADD8(A, B, 0);
      break;
    case 0x81:                  //** ADD A, C **
      A = ADD8(A, C, 0);
      break;
    case 0x82:                  //** ADD A, D **
      A = ADD8(A, D, 0);
      break;
    case 0x83:                  //** ADD A, E **
      A = ADD8(A, E, 0);
      break;
    case 0x84:                  //** ADD A, H **
      A = ADD8(A, H, 0);
      break;
    case 0x85:                  //** ADD A, L **
      A = ADD8(A, L, 0);
      break;
    case 0x86:                  //** ADD A, (HL) **
      V8 = RAM[(H * 256) + L];
      A = ADD8(A, V8, 0);
      break;
    case 0x87:                  //** ADD A, A **
      A = ADD8(A, A, 0);
      break;
    case 0x88:                  //** ADC A, B **
      A = ADD8(A, B, Cf);
      break;
    case 0x89:                  //** ADC A, C **
      A = ADD8(A, C, Cf);
      break;
    case 0x8A:                  //** ADC A, D **
      A = ADD8(A, D, Cf);
      break;
    case 0x8B:                  //** ADC A, E **
      A = ADD8(A, E, Cf);
      break;
    case 0x8C:                  //** ADC A, H **
      A = ADD8(A, H, Cf);
      break;
    case 0x8D:                  //** ADC A, L **
      A = ADD8(A, L, Cf);
      break;
    case 0x8E:                  //** ADC A, (HL) **
      V8 = RAM[(H * 256) + L];
      A = ADD8(A, V8, Cf);
      break;
    case 0x8F:                  //** ADC A, A **
      A = ADD8(A, A, Cf);
      break;
    //********************************************

    //********************************************
    // Instructions 90 to 9F fully implemented
    //********************************************
    case 0x90:                  //** SUB B **
      A = SUB8(A, B, 0);
      break;
    case 0x91:                  //** SUB C **
      A = SUB8(A, C, 0);
      break;
    case 0x92:                  //** SUB D **
      A = SUB8(A, D, 0);
      break;
    case 0x93:                  //** SUB E **
      A = SUB8(A, E, 0);
      break;
    case 0x94:                  //** SUB H **
      A = SUB8(A, H, 0);
      break;
    case 0x95:                  //** SUB L **
      A = SUB8(A, L, 0);
      break;
    case 0x96:                  //** SUB (HL) **
      V8 = RAM[(H * 256) + L];
      A = SUB8(A, V8, 0);
      break;
    case 0x97:                  //** SUB A **
      A = SUB8(A, A, 0);
      break;
    case 0x98:                    //** SBC A, B ***
      A = SUB8(A, B, Cf);
      break;
    case 0x99:                    //** SBC A, C ***
      A = SUB8(A, C, Cf);
      break;
    case 0x9A:                    //** SBC A, D ***
      A = SUB8(A, D, Cf);
      break;
    case 0x9B:                    //** SBC A, E ***
      A = SUB8(A, E, Cf);
      break;
    case 0x9C:                    //** SBC A, H ***
      A = SUB8(A, H, Cf);
      break;
    case 0x9D:                    //** SBC A, L ***
      A = SUB8(A, L, Cf);
      break;
    case 0x9E:                    //** SBC A, (HL) ***
      V8 = RAM[(H * 256) + L];
      A = SUB8(A, V8, Cf);
      break;
    case 0x9F:                    //** SBC A, A ***
      A = SUB8(A, A, Cf);
      break;
    //********************************************

    //********************************************
    // Instructions A0 to AF fully implemented
    //********************************************
    case 0xA0:                  //** AND B **
      A = A & B;
      Cf = false;
      Nf = false;
      calcP(A);
      Hf = true;
      Zf = false;
      if (A == 0) Zf = true;
      Sf = bitRead(A, 7);
      break;
    case 0xA1:                  //** AND C **
      A = A & C;
      Cf = false;
      Nf = false;
      calcP(A);
      Hf = true;
      Zf = false;
      if (A == 0) Zf = true;
      Sf = bitRead(A, 7);
      break;
    case 0xA2:                  //** AND D **
      A = A & D;
      Cf = false;
      Nf = false;
      calcP(A);
      Hf = true;
      Zf = false;
      if (A == 0) Zf = true;
      Sf = bitRead(A, 7);
      break;
    case 0xA3:                  //** AND E **
      A = A & E;
      Cf = false;
      Nf = false;
      calcP(A);
      Hf = true;
      Zf = false;
      if (A == 0) Zf = true;
      Sf = bitRead(A, 7);
      break;
    case 0xA4:                  //** AND H **
      A = A & H;
      Cf = false;
      Nf = false;
      calcP(A);
      Hf = true;
      Zf = false;
      if (A == 0) Zf = true;
      Sf = bitRead(A, 7);
      break;
    case 0xA5:                  //** AND L **
      A = A & L;
      Cf = false;
      Nf = false;
      calcP(A);
      Hf = true;
      Zf = false;
      if (A == 0) Zf = true;
      Sf = bitRead(A, 7);
      break;
    case 0xA6:                  //** AND (HL) **
      A = A & RAM[(H * 256) + L];
      Cf = false;
      Nf = false;
      calcP(A);
      Hf = true;
      Zf = false;
      if (A == 0) Zf = true;
      Sf = bitRead(A, 7);
      break;
    case 0xA7:                  //** AND A **
      A = A & A;
      Cf = false;
      Nf = false;
      calcP(A);
      Hf = true;
      Zf = false;
      if (A == 0) Zf = true;
      Sf = bitRead(A, 7);
      break;
    case 0xA8:                  //** XOR B **
      A = A ^ B;
      Cf = false;
      Nf - false;
      calcP(A);
      Hf = false;
      Zf = false;
      if (A == 0) Zf = true;
      Sf = bitRead(A, 7);
      break;
    case 0xA9:                  //** XOR C **
      A = A ^ C;
      Cf = false;
      Nf - false;
      calcP(A);
      Hf = false;
      Zf = false;
      if (A == 0) Zf = true;
      Sf = bitRead(A, 7);
      break;
    case 0xAA:                  //** XOR D **
      A = A ^ D;
      Cf = false;
      Nf - false;
      calcP(A);
      Hf = false;
      Zf = false;
      if (A == 0) Zf = true;
      Sf = bitRead(A, 7);
      break;
    case 0xAB:                  //** XOR E **
      A = A ^ E;
      Cf = false;
      Nf - false;
      calcP(A);
      Hf = false;
      Zf = false;
      if (A == 0) Zf = true;
      Sf = bitRead(A, 7);
      break;
    case 0xAC:                  //** XOR H **
      A = A ^ H;
      Cf = false;
      Nf - false;
      calcP(A);
      Hf = false;
      Zf = false;
      if (A == 0) Zf = true;
      Sf = bitRead(A, 7);
      break;
    case 0xAD:                  //** XOR L **
      A = A ^ L;
      Cf = false;
      Nf - false;
      calcP(A);
      Hf = false;
      Zf = false;
      if (A == 0) Zf = true;
      Sf = bitRead(A, 7);
      break;
    case 0xAE:                  //** XOR (HL) **
      A = A ^ RAM[(H * 256) + L];
      Cf = false;
      Nf - false;
      calcP(A);
      Hf = false;
      Zf = false;
      if (A == 0) Zf = true;
      Sf = bitRead(A, 7);
      break;
    case 0xAF:                  //** XOR A **
      A = 0;                    //Clears A and sets flags accordingly
      Sf = false;
      Zf = true;
      Hf = false;
      Pf = true;
      Nf = false;
      Cf = false;
      break;
    //********************************************

    //********************************************
    // Instructions B0 to BF fully implemented
    //********************************************
    case 0xB0:                  //*** OR B **
      A = A | B;
      Cf = false;
      Nf - false;
      calcP(A);
      Hf = false;
      Zf = false;
      if (A == 0) Zf = true;
      Sf = bitRead(A, 7);
      break;
    case 0xB1:                  //** OR C **
      A = A | C;
      Cf = false;
      Nf - false;
      calcP(A);
      Hf = false;
      Zf = false;
      if (A == 0) Zf = true;
      Sf = bitRead(A, 7);
      break;
    case 0xB2:                  //** OR D **
      A = A | D;
      Cf = false;
      Nf - false;
      calcP(A);
      Hf = false;
      Zf = false;
      if (A == 0) Zf = true;
      Sf = bitRead(A, 7);
      break;
    case 0xB3:                  //** OR E **
      A = A | E;
      Cf = false;
      Nf - false;
      calcP(A);
      Hf = false;
      Zf = false;
      if (A == 0) Zf = true;
      Sf = bitRead(A, 7);
      break;
    case 0xB4:                  //** OR H **
      A = A | H;
      Cf = false;
      Nf - false;
      calcP(A);
      Hf = false;
      Zf = false;
      if (A == 0) Zf = true;
      Sf = bitRead(A, 7);
      break;
    case 0xB5:                  //** OR L **
      A = A | L;
      Cf = false;
      Nf - false;
      calcP(A);
      Hf = false;
      Zf = false;
      if (A == 0) Zf = true;
      Sf = bitRead(A, 7);
      break;
    case 0XB6:                  //** OR (HL) **
      A = A | RAM[H * 256 + L];
      Cf = false;
      Nf - false;
      calcP(A);
      Hf = false;
      Zf = false;
      if (A == 0) Zf = true;
      Sf = bitRead(A, 7);
      break;
    case 0XB7:                  //** OR A **
      A = A | A;
      Cf = false;
      Nf - false;
      calcP(A);
      Hf = false;
      Zf = false;
      if (A == 0) Zf = true;
      Sf = bitRead(A, 7);
      break;
    case 0xB8:                  //** CP B **
      SUB8(A, B, 0);            //Update flags
      break;
    case 0xB9:                 //** CP C **
      SUB8(A, C, 0);           //Update flags
      break;
    case 0xBA:                  //** CP D **
      SUB8(A, D, 0);            //Update flags
      break;
    case 0xBB:                //** CP E **
      SUB8(A, E, 0);            //Update flags
      break;
    case 0xBC:                  //** CP H **
      SUB8(A, H, 0);            //Update flags
      break;
    case 0xBD:                  //** CP L **
      SUB8(A, L, 0);            //Update flagss
      break;
    case 0xBE:                  //** CP (HL) **
      SUB8(A, RAM[(H * 256) + L], 0);  //Update flags
      break;
    case 0xBF:                  //** CP A **
      SUB8(A, A, 0);            //Update flags
      break;
    //********************************************
    // Instructions C0 to CF fully implemented
    //********************************************
    case 0xC0:                  //** RET NZ **
      if (Zf == false) {
        PC = RAM[SP];
        SP++;
        PC += 256 * RAM[SP];    //POP return address off the stack
        SP++;
      }
      break;
    case 0xC1:                  //** POP BC **
      C = RAM[SP];
      SP++;
      B = RAM[SP];
      SP++;
      break;
    case 0xC2:                  //** JP NZ **
      V16 = get16();
      if (Zf == false) PC = V16; //Jump to absolute address if not zero
      break;
    case 0xC3:                  //** JP **
      PC = get16();             //Get 2 byte address
      break;
    case 0xC4:                  //** CALL NZ, value **
      V16 = get16();
      if (Zf == false) {
        SP--;
        RAM[SP] = PC / 256;       //Push program counter onto the stack
        SP--;
        RAM[SP] = PC & 255;
        PC = V16;                 //Put call address in Program Counter
      }
      break;
    case 0xC5:                    //** PUSH BC **
      SP--;
      RAM[SP] = B;
      SP--;
      RAM[SP] = C;
      break;
    case 0xC6:                  //** ADD A, value **
      V8 = get8();
      A = ADD8(A, V8, 0);
      break;
    case 0xC7:                  //** RST 00 **
      SP--;
      RAM[SP] = PC / 256;        //Push program counter onto the stack
      SP--;
      RAM[SP] = PC & 255;
      PC = 0x00;                 //Put 0c00 in Program Counter
      break;
    case 0xC8:                  //** RET Z **
      if (Zf == true) {
        PC = RAM[SP];
        SP++;
        PC += 256 * RAM[SP];    //POP return address off the stack
        SP++;
      }
      break;
    case 0xC9:                  //** RET **
      PC = RAM[SP];
      SP++;
      PC += 256 * RAM[SP];      //POP return address off the stack
      SP++;
      break;
    case 0xCA:                  //** JP Z, value **
      V16 = get16();
      if (Zf == true) PC = V16;
      break;
    //*** CB Extended bit instrucions ***
    case 0xCB:
      CPU_CB();
      break;
    //********************************************
    case 0xCC:                  //** CALL Z, value **
      V16 = get16();
      if (Zf == true) {
        SP--;
        RAM[SP] = PC / 256;        //Push program counter onto the stack
        SP--;
        RAM[SP] = PC & 255;
        PC = V16;                 //Put call address in Program Counter
      }
      break;
    case 0xCD:                  //** CALL value **
      V16 = get16();
      SP--;
      RAM[SP] = PC / 256;        //Push program counter onto the stack
      SP--;
      RAM[SP] = PC & 255;
      PC = V16;                 //Put call address in Program Counter
      break;
    case 0xCE:                  //** ADC A, value **
      V8 = get8();
      A = ADD8(A, E, Cf);
      break;
    case 0xCF:                  //** RST 08 **
      SP--;
      RAM[SP] = PC / 256;        //Push program counter onto the stack
      SP--;
      RAM[SP] = PC & 255;
      PC = 0x08;                 //Put 0c00 in Program Counter
      break;
    //********************************************
    // Instructions D0 to DF fully implemented
    //********************************************
    case 0xD0:                  //** RET NC **
      if (Cf == false) {
        PC = RAM[SP];
        SP++;
        PC += 256 * RAM[SP];    //POP return address off the stack
        SP++;
      }
      break;
    case 0xD1:                  //** POP DE **
      E = RAM[SP];
      SP++;
      D = RAM[SP];
      SP++;
      break;
    case 0xD2:                  //** JP NC, value **
      V16 = get16();
      if (Cf == false) PC = V16;
      break;
    case 0xD3:                  //** OUT PORT, A **
      portOut(get8(), A);
      break;
    case 0xD4:                  //** CALL NC, value **
      V16 = get16();
      if (Cf == false) {
        SP--;
        RAM[SP] = PC / 256;        //Push program counter onto the stack
        SP--;
        RAM[SP] = PC & 255;
        PC = V16;                 //Put call address in Program Counter
      }
      break;
    case 0xD5:                  //** PUSH DE **
      SP--;
      RAM[SP] = D;
      SP--;
      RAM[SP] = E;
      break;
    case 0xD6:                  //** SUB value **
      V8 = get8();
      A = SUB8(A, V8, 0);
      break;
    case 0xD7:                  //** RST 10 **
      SP--;
      RAM[SP] = PC / 256;       //Push program counter onto the stack
      SP--;
      RAM[SP] = PC & 255;
      PC = 0x10;                 //Put 0c00 in Program Counter
      break;
    case 0xD8:                  //** RET C **
      if (Cf == true) {
        PC = RAM[SP];
        SP++;
        PC += 256 * RAM[SP];    //POP return address off the stack
        SP++;
      }
      break;
    case 0xD9:                  //** EXX **
      V8 = Ba;
      Ba = B;
      B = V8;
      V8 = Ca;
      Ca = C;
      C = V8;
      V8 = Da;
      Da = D;
      D = V8;
      V8 = Ea;
      Ea = E;
      E = V8;
      V8 = Ha;
      Ha = H;
      H = V8;
      V8 = La;
      La = L;
      L = V8;
      break;
    case 0xDA:                  //** JP C, value **
      V16 = get16();
      if (Cf == true) PC = V16;
      break;
    case 0xDB:                  //** IN A, PORT **
      A = portIn(get8());
      break;
    case 0xDC:                  //** CALL C, value **
      V16 = get16();
      if (Cf == true) {
        SP--;
        RAM[SP] = PC / 256;        //Push program counter onto the stack
        SP--;
        RAM[SP] = PC & 255;
        PC = V16;                 //Put call address in Program Counter
      }
      break;
    //*** DD IX Extended instrucions ***
    case 0xDD:
      CPU_DD();
      break;

    //********************************************
    case 0xDE:                    //** SBC A, value ***
      V8 = get8();
      A = SUB8(A, V8, Cf);
      break;
    case 0xDF:                  //** RST 18 **
      SP--;
      RAM[SP] = PC / 256;        //Push program counter onto the stack
      SP--;
      RAM[SP] = PC & 255;
      PC = 0x18;                 //Put 0c00 in Program Counter
      break;
    //********************************************
    // Instructions E0 to EF fully implemented
    //********************************************
    case 0xE0:                  //** RET PO **
      if (Pf == false) {
        PC = RAM[SP];
        SP++;
        PC += 256 * RAM[SP];    //POP return address off the stack
        SP++;
      }
      break;
    case 0xE1:                  //** POP HL **
      L = RAM[SP];
      SP++;
      H = RAM[SP];
      SP++;
      break;
    case 0xE2:                  //** JP PO, value **
      V16 = get16();
      if (Pf == false) PC = V16;
      break;
    case 0xE3:                  //** EX (SP), HL **
      V8 = RAM[SP];
      RAM[SP] = L;
      L = V8;
      V8 = RAM[SP + 1];
      RAM[SP + 1] = H;
      H = V8;
      break;
    case 0xE4:                  //** CALL PO, value **
      V16 = get16();
      if (Pf == false) {
        SP--;
        RAM[SP] = PC / 256;       //Push program counter onto the stack
        SP--;
        RAM[SP] = PC & 255;
        PC = V16;                 //Put call address in Program Counter
      }
      break;
    case 0xE5:                  //** PUSH HL **
      SP--;
      RAM[SP] = H;
      SP--;
      RAM[SP] = L;
      break;
    case 0xE6:                  //** AND VALUE **
      V8 = get8();
      A = A & V8;
      Cf = false;
      Nf = false;
      calcP(A);
      Hf = true;
      Zf = false;
      if (A == 0) Zf = true;
      Sf = bitRead(A, 7);
      break;
    case 0xE7:                  //** RST 20 **
      SP--;
      RAM[SP] = PC / 256;        //Push program counter onto the stack
      SP--;
      RAM[SP] = PC & 255;
      PC = 0x20;                 //Put 0c00 in Program Counter
      break;
    case 0xE8:                  //** RET PE **
      if (Pf == true) {
        PC = RAM[SP];
        SP++;
        PC += 256 * RAM[SP];    //POP return address off the stack
        SP++;
      }
      break;
    case 0xE9:                  //** JP (HL) **
      PC = (H * 256) + L;
      break;
    case 0xEA:                  //** JP PE, value **
      V16 = get16();
      if (Pf == true) PC = V16;
      break;
    case 0xEB:                  //** EX DE, HL **
      V8 = H;
      H = D;
      D = V8;
      V8 = L;
      L = E;
      E = V8;
      break;
    case 0xEC:                  //** CALL PE, value **
      V16 = get16();
      if (Pf == true) {
        SP--;
        RAM[SP] = PC / 256;        //Push program counter onto the stack
        SP--;
        RAM[SP] = PC & 255;
        PC = V16;                 //Put call address in Program Counter
      }
      break;
    //*** ED Extended instrucions ***
    case 0xED:
      CPU_ED();
      break;
    case 0xEE:                  //** XOR value **
      V8 = get8();
      A = A ^ V8;
      Cf = false;
      Nf - false;
      calcP(A);
      Hf = false;
      Zf = false;
      if (A == 0) Zf = true;
      Sf = bitRead(A, 7);
      break;
    case 0xEF:                  //** RST 28 **
      SP--;
      RAM[SP] = PC / 256;        //Push program counter onto the stack
      SP--;
      RAM[SP] = PC & 255;
      PC = 0x28;                 //Put 0c00 in Program Counter
      break;
    //********************************************
    // Instructions F0 to FF
    //********************************************
    case 0xF0:                  //** RET P **
      if (Sf == false) {
        PC = RAM[SP];
        SP++;
        PC += 256 * RAM[SP];    //POP return address off the stack
        SP++;
      }
      break;
    case 0xF1:                  //** POP AF **
      Fl = RAM[SP];
      SP++;
      A = RAM[SP];
      SP++;
      Sf = bitRead(Fl, 7);
      Zf = bitRead(Fl, 6);
      Hf = bitRead(Fl, 4);
      Pf = bitRead(Fl, 2);
      Nf = bitRead(Fl, 1);
      Cf = bitRead(Fl, 0);
      break;
    case 0xF2:                  //** JP P, value **
      V16 = get16();
      if (Sf == false) PC = V16;
      break;
    case 0xF3:                  //** DI **
      intE = false;                //Disable interrupts
      break;
    case 0xF4:                  //** CALL P, value **
      V16 = get16();
      if (Sf == false) {
        SP--;
        RAM[SP] = PC / 256;       //Push program counter onto the stack
        SP--;
        RAM[SP] = PC & 255;
        PC = V16;                 //Put call address in Program Counter
      }
      break;
    case 0xF5:                  //** PUSH AF **
      bitWrite(Fl, 7 , Sf);
      bitWrite(Fl, 6 , Zf);
      bitWrite(Fl, 4 , Hf);
      bitWrite(Fl, 2 , Pf);
      bitWrite(Fl, 1 , Nf);
      bitWrite(Fl, 0 , Cf);
      SP--;
      RAM[SP] = A;
      SP--;
      RAM[SP] = Fl;
      break;
    case 0xF6:                  //*** OR value **
      V8 = get8();
      A = A | V8;
      Cf = false;
      Nf - false;
      calcP(A);
      Hf = false;
      Zf = false;
      if (A == 0) Zf = true;
      Sf = bitRead(A, 7);
      break;
    case 0xF7:                  //** RST 30 **
      SP--;
      RAM[SP] = PC / 256;        //Push program counter onto the stack
      SP--;
      RAM[SP] = PC & 255;
      PC = 0x30;                 //Put 0x30 in Program Counter
      break;
    case 0xF8:                  //** RET M **
      if (Sf == true ) {
        PC = RAM[SP];
        SP++;
        PC += 256 * RAM[SP];      //POP return address off the stack
        SP++;
      }
      break;
    case 0xF9:                  //** LD SP, HL **
      SP = (H * 256) + L;
      break;
    case 0xFA:                  //** JP M, value **
      V16 = get16();
      if (Sf == true) PC = V16;
      break;
    case 0xFB:                  //** EI **
      intE = true;
      break;
    case 0xFC:                  //** CALL M, value **
      V16 = get16();
      if (Sf == true) {
        SP--;
        RAM[SP] = PC / 256;        //Push program counter onto the stack
        SP--;
        RAM[SP] = PC & 255;
        PC = V16;                 //Put call address in Program Counter
      }
      break;
    //*** FD Extended Instructions ***
    case 0xFD:
      CPU_FD();
      break;
    //********************************************
    case 0xFE:                  //** CP A, value **
      V8 = get8();
      SUB8(A, V8, 0);            //Update flags
      break;
    case 0xFF:                  //** RST 38 **
      SP--;
      RAM[SP] = PC / 256;        //Push program counter onto the stack
      SP--;
      RAM[SP] = PC & 255;
      PC = 0x38;                 //Put 0x38 in Program Counter
      break;

    default:                    //NOP or anything not recognised
      Serial.printf("Unknown OP-Code %.2X at %.4X\n\r", OC, PC - 1);
      break;
  }

}

void CPU_ED(void) {
  uint8_t V8a = get8();

  switch (V8a) {
    case 0x41:  //** OUT (C), B **
      portOut(C, B);
      break;

    case 0x43:      //LD (Value), BC
      V16 = get16();
      RAM[V16] = C;
      RAM[V16 + 1] = B;
      break;

    case 0x45:      //RETN ** need to do something about IFF1 and IFF2 flags
      PC = RAM[SP];
      SP++;
      PC += 256 * RAM[SP];    //POP return address off the stack
      SP++;
      break;
    case 0x49:    //** OUT (C), C **
      portOut(C, C);
      break;
    case 0x51:  //** OUT (C), D **
      portOut(C, D);
      break;

    case 0x53:               // ** LD (value), DE **
      V16 = get16();
      RAM[V16] = E;
      RAM[V16 + 1] = D;
      break;

    case 0x56:               // IM 1 *** interrupts not implemented
      break;

    case 0x59:   //** OUT (C), E **
      portOut(C, E);
      break;
    case 0x5A:                //** ADC HL, DE **
      V32 = (H * 256) + L;
      V16 = (D * 256) + E;
      V32 = V32 + V16 + Cf;
      H = (V32 / 256) & 0xff;
      L = V32 & 0xff;
      Sf = bitRead(V32, 15);
      if ((V32 & 0xffff) == 0) {
        Zf = true;
      } else {
        Zf = false;
      }
      if ((V32 & 0xffff0000) > 0) {
        Pf = true;
      } else {
        Pf = false;
      }
      Nf = false;
      Hf = bitRead(V32, 12);
      Cf = bitRead(V32, 16);
      break;
    case 0x5B:              //** LD DE, (value) **
      V16 = get16();
      E = RAM[V16];
      D = RAM[V16 + 1];
      break;
    case 0x61:  //** OUT (C), H **
      portOut(C, H);
      break;
    case 0x69:  //** OUT (C), L **
      portOut(C, L);
      break;
    case 0x6F:              //** RLD **
      //Serial.printf("Before RLD %.2x %.2x \n\r", A,  RAM[(H * 256) +L]);
      V8 = A & 0xf0;                  //Preserve high nibble of A
      V16 = RAM[(H * 256) + L];       //get (HL)
      V16 = V16 << 4;                       //Shift left 4
      V16 = V16 + (A & 0x0f);         //Add on low nibble of A
      RAM[(H * 256) + L] = V16 & 0xff; //rewrite (HL)
      V16 = V16 >> 8;                       //Get most significant nibble
      A = V16 & 0x0f;                 //Get the new low nibble of A
      A = A + V8;                     //reassemble A

      Hf = false;                     //Set the flags
      Nf = false;
      Zf = false;
      if (A == 0) Zf = true;
      Sf = bitRead(A, 7);
      break;
    case 0x71:  //** OUT (C), 0 **
      portOut(C, 0);
      break;
    case 0x78:  //** In A, (C) **
      A = portIn(C);
      Nf = false;
      calcP(A);
      Hf = false;
      if (A == 0) Zf = true; else Zf = false;
      Sf = bitRead(A, 7);
      break;
    case 0x79:    //** OUT (C), A **
      portOut(C, A);
      break;
    case 0xA1:    //** CPI **
      V16 = (H * 256) + L;
      SUB8(A, RAM[V16], 0);
      V16++;
      H = V16 / 256;
      L = V16 & 0xff;
      V16 = (B * 256) + C;
      V16--;
      B = V16 / 256;
      C = V16 & 0xff;
      Nf = true;
      break;
    case 0xB0:    //** LDIR **
      uint16_t src, dst, cc;
      V16 = (256 * B) + C;  //Byte count
      src = (256 * H) + L;  //Source
      dst = (256 * D) + E;  //Destination
      for (cc = 0; cc < V16; cc++) {
        RAM[dst] = RAM[src];  //copy byte
        dst++;
        src++;
      }
      H = src / 16;
      L = src & 0x0f;
      D = dst / 16;
      E = dst & 0x0f;
      B = 0;
      C = 0;
      Nf = false;
      Pf = false;
      Hf = false;
      break;
    default:
      Serial.printf("Unknown OP-Code ED %.2X at %.4X\n\r", V8a, PC - 1);
      break;
  }
}

void CPU_CB(void) {
  uint8_t V8a = get8();
  switch (V8a) {
    case 0x00:    //RLC B
      Cf = bitRead(B, 7);
      B = B << 1;
      bitWrite(B, 0, Cf);
      if (B == 0) Zf = true; else Zf = false;
      Sf = bitRead(B, 7);
      calcP(B);
      Hf == false;
      Nf == false;
      break;
    case 0x01:    //RLC C
      Cf = bitRead(C, 7);
      C = C << 1;
      bitWrite(C, 0, Cf);
      if (C == 0) Zf = true; else Zf = false;
      Sf = bitRead(C, 7);
      calcP(C);
      Hf == false;
      Nf == false;
      break;
    case 0x02:    //RLC D
      Cf = bitRead(D, 7);
      D = D << 1;
      bitWrite(D, 0, Cf);
      if (D == 0) Zf = true; else Zf = false;
      Sf = bitRead(D, 7);
      calcP(D);
      Hf == false;
      Nf == false;
      break;
    case 0x03:    //RLC E
      Cf = bitRead(E, 7);
      E = E << 1;
      bitWrite(E, 0, Cf);
      if (E == 0) Zf = true; else Zf = false;
      Sf = bitRead(E, 7);
      calcP(E);
      Hf == false;
      Nf == false;
      break;
    case 0x04:    //RLC H
      Cf = bitRead(H, 7);
      H = H << 1;
      bitWrite(H, 0, Cf);
      if (H == 0) Zf = true; else Zf = false;
      Sf = bitRead(H, 7);
      calcP(H);
      Hf == false;
      Nf == false;
      break;
    case 0x05:    //RLC L
      Cf = bitRead(L, 7);
      L = L << 1;
      bitWrite(L, 0, Cf);
      if (L == 0) Zf = true; else Zf = false;
      Sf = bitRead(L, 7);
      calcP(L);
      Hf == false;
      Nf == false;
      break;
    case 0x06:    //RLC (HL)
      V8 = RAM[(H * 256) + L];
      Cf = bitRead(V8, 7);
      V8 = V8 << 1;
      bitWrite(V8, 0, Cf);
      RAM[(H * 256) + L] = V8;
      if (V8 == 0) Zf = true; else Zf = false;
      Sf = bitRead(V8, 7);
      calcP(V8);
      Hf == false;
      Nf == false;
      break;
    case 0x07:    //RLC A
      Cf = bitRead(A, 7);
      A = A << 1;
      bitWrite(A, 0, Cf);
      if (A == 0) Zf = true; else Zf = false;
      Sf = bitRead(A, 7);
      calcP(A);
      Hf == false;
      Nf == false;
      break;
    case 0x08:    //RRC B
      Cf = bitRead(B, 0);
      B = B >> 1;
      bitWrite(B, 7, Cf);
      if (B == 0) Zf = true; else Zf = false;
      Sf = bitRead(B, 7);
      calcP(B);
      Hf = false;
      Nf = false;
      break;
    case 0x09:    //RRC C
      Cf = bitRead(C, 0);
      C = C >> 1;
      bitWrite(C, 7, Cf);
      if (C == 0) Zf = true; else Zf = false;
      Sf = bitRead(C, 7);
      calcP(C);
      Hf = false;
      Nf = false;
      break;
    case 0x0A:    //RRC D
      Cf = bitRead(D, 0);
      D = D >> 1;
      bitWrite(D, 7, Cf);
      if (D == 0) Zf = true; else Zf = false;
      Sf = bitRead(D, 7);
      calcP(D);
      Hf = false;
      Nf = false;
      break;
    case 0x0B:    //RRC E
      Cf = bitRead(E, 0);
      E = E >> 1;
      bitWrite(E, 7, Cf);
      if (E == 0) Zf = true; else Zf = false;
      Sf = bitRead(E, 7);
      calcP(E);
      Hf = false;
      Nf = false;
      break;
    case 0x0C:    //RRC H
      Cf = bitRead(H, 0);
      H = H >> 1;
      bitWrite(H, 7, Cf);
      if (H == 0) Zf = true; else Zf = false;
      Sf = bitRead(H, 7);
      calcP(H);
      Hf = false;
      Nf = false;
      break;
    case 0x0D:    //RRC L
      Cf = bitRead(L, 0);
      L = L >> 1;
      bitWrite(L, 7, Cf);
      if (L == 0) Zf = true; else Zf = false;
      Sf = bitRead(L, 7);
      calcP(L);
      Hf = false;
      Nf = false;
      break;
    case 0x0E:    //RRC (HL)
      V8 = RAM[(H * 256) + L];
      Cf = bitRead(V8, 0);
      V8 = V8 >> 1;
      bitWrite(V8, 7, Cf);
      RAM[(H * 256) + L] = V8;
      if (V8 == 0) Zf = true; else Zf = false;
      Sf = bitRead(V8, 7);
      calcP(V8);
      Hf = false;
      Nf = false;
      break;
    case 0x0F:    //RRC A
      Cf = bitRead(A, 0);
      A = A >> 1;
      bitWrite(A, 7, Cf);
      if (A == 0) Zf = true; else Zf = false;
      Sf = bitRead(A, 7);
      calcP(A);
      Hf = false;
      Nf = false;
      break;
    case 0x12:    //** RL D **
      cfs = bitRead(D, 7);
      D = D << 1;
      bitWrite(D, 0, Cf);
      Cf = cfs;
      Nf = false;
      Hf = false;
      calcP(D);
      Zf = false;
      if (D == 0) Zf = true;
      Sf = bitRead(D, 7);
      break;
    case 0x13:    //** RL E **
      cfs = bitRead(E, 7);
      E = E << 1;
      bitWrite(E, 0, Cf);
      Cf = cfs;
      Nf = false;
      Hf = false;
      calcP(E);
      Zf = false;
      if (E == 0) Zf = true;
      Sf = bitRead(E, 7);
      break;
    case 0x3F:    //** SRL **
      Cf = bitRead(A, 0);
      A = A >> 1;
      Sf = false;
      Hf = false;
      Nf = false;
      Zf = false;
      if (A == 0) Zf = true;
      calcP(A);
      break;
    case 0x46:    //** BIT 0, (HL) **
      Zf = !bitRead(RAM[(H * 256) + L], 0);
      Nf = false;
      Hf = true;
      break;
    case 0x47:    //** BIT 0, A **
      Zf = !bitRead(A, 0);
      Nf = false;
      Hf = true;
      break;
    case 0x4E:    //** BIT 1, (HL) **
      Zf = !bitRead(RAM[(H * 256) + L], 1);
      Nf = false;
      Hf = true;
      break;
    case 0x4F:   //BIT 1, A
      Zf = !bitRead(A, 1);
      Nf = false;
      Hf = true;
      break;
    case 0x56:    //** BIT 2, (HL) **
      Zf = !bitRead(RAM[(H * 256) + L], 2);
      Nf = false;
      Hf = true;
      break;
    case 0x57:   //BIT 2, A
      Zf = !bitRead(A, 2);
      Nf = false;
      Hf = true;
      break;
    case 0x5E:    //** BIT 3, (HL) **
      Zf = !bitRead(RAM[(H * 256) + L], 3);
      Nf = false;
      Hf = true;
      break;
    case 0x5F:   //BIT 3, A
      Zf = !bitRead(A, 3);
      Nf = false;
      Hf = true;
      break;
    case 0x66:    //** BIT 4, (HL) **
      Zf = !bitRead(RAM[(H * 256) + L], 4);
      Nf = false;
      Hf = true;
      break;
    case 0x67:   //BIT 4, A
      Zf = !bitRead(A, 4);
      Nf = false;
      Hf = true;
      break;
    case 0x6E:    //** BIT 5, (HL) **
      Zf = !bitRead(RAM[(H * 256) + L], 5);
      Nf = false;
      Hf = true;
      break;
    case 0x6F:   //BIT 5, A
      Zf = !bitRead(A, 5);
      Nf = false;
      Hf = true;
      break;
    case 0x76:    //** BIT 6, (HL) **
      Zf = !bitRead(RAM[(H * 256) + L], 6);
      Nf = false;
      Hf = true;
      break;
    case 0x77:   //BIT 6, A
      Zf = !bitRead(A, 6);
      Nf = false;
      Hf = true;
      break;
    case 0x7E:    //** BIT 7, (HL) **
      Zf = !bitRead(RAM[(H * 256) + L], 7);
      Nf = false;
      Hf = true;
      break;
    case 0x7F:   //BIT 7, A
      Zf = !bitRead(A, 7);
      Nf = false;
      Hf = true;
      break;
    default:
      Serial.printf("Unknown OP-Code CB %.2X at %.4X\n\r", V8a, PC - 1);
      break;
  }
}

void CPU_DD(void) {
  uint8_t V8a = get8();       //get next byte of opcode
  switch (V8a) {
    case 0x21:    //LD IX, value
      IX = get16();
      break;
    case 0x23: //INC IX
      IX++;
      break;
    case 0x36:   //** LD [IX+v1], v2 **
      v1 = get8();
      v2 = get8();
      RAM[IX + v1] = v2;
      break;
    case 0x77:    //** LD [IX+v1], A **
      RAM[IX + get8()] = A;
      break;
    case 0xe1:    //** POP IX **
      IX = RAM[SP];
      SP++;
      IX += 256 * RAM[SP];
      SP++;
      break;
    case 0xe5:    //** Push IX **
      SP--;
      RAM[SP] = IX / 256;
      SP--;
      RAM[SP] += IX & 0xff;
      break;
    default:
      Serial.printf("Unknown OP-Code DD %.2X at %.4X\n\r", V8a, PC - 1);
      break;
  }
}

void CPU_FD(void) {
  uint8_t V8a = get8();
  switch (V8a) {
    case 0xe1:    //** POP IY **
      IY = RAM[SP];
      SP++;
      IY += 256 * RAM[SP];
      SP++;
      break;
    case 0xe5:    //** Push IY **
      SP--;
      RAM[SP] = IY / 256;
      SP--;
      RAM[SP] += IY & 0xff;
      break;
    default:
      Serial.printf("Unknown OP-Code FD %.2X at %.4X\n\r", V8a, PC - 1);
      break;
  }
}