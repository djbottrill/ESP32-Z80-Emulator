#pragma once
const String banner[] = {           //Startup / logon banner
  " EEEEEE   SSSSSS    PPPPPP    888888    000000  ",
  "E        S      S  P      P  8      8  0     00 ",
  "E        S         P      P  8      8  0    0 0 ",
  "EEEEEE    SSSSSS   PPPPPPP    888888   0   0  0 ",
  "E               S  P         8      8  0  0   0 ",
  "E               S  P         8      8  0 0    0 ",
  " EEEEEE   SSSSSS   P          888888    000000  ",
  "                                                ",
  "           Z80 Emulator for ESP32 V2.2          ",
  " David Bottrill - Shady Grove Electronics 2023  ",
  "                                                "
  };

WiFiServer server(23);
WiFiClient serverClient;
const char *hostName = "esp80";   //Hostname


#define S3

//*********************************************************************************************
//****                    Configuration for Lolin 32 Board                                 ****
//*********************************************************************************************
#ifdef LOLIN32
#define SS    5
#define MOSI  23
#define MISO  19
#define SCK   18
SPIClass sdSPI(VSPI);

//Define pins to use as virtual GPIO ports, -1 means not implemented
int PortA[8] = { 32, 33, 25, 26, 27, 14, 12, 13};   //Virtual GPIO Port A
int PortB[8] = { 17, 16, -1, -1, -1, -1, -1, -1};   //Virtual GPIO Port B


//BreakPoint switches
#define swA 15                  //Breakpoints on / Off
#endif
//*********************************************************************************************

//*********************************************************************************************
//****                  Configuration for Lillygo T1 Board                                 ****
//*********************************************************************************************
#ifdef T1

#define SS    13
#define MOSI  15
#define MISO  2
#define SCK   14
SPIClass sdSPI(HSPI);

//Define pins to use as virtual GPIO ports, -1 means not implemented
int PortA[8] = { -1, -1, -1, -1, -1, -1, -1, -1};   //Virtual GPIO Port A
int PortB[8] = { -1, -1, -1, -1, -1, -1, -1, -1};   //Virtual GPIO Port B

//BreakPoint switches
#define swA 4                   //Breakpoints on / Off

#endif


//*********************************************************************************************
//****                  Configuration for Lillygo T2 Board                                 ****
//*********************************************************************************************
#ifdef T2

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1331.h>
#include <SPI.h>

#define sclk 14
#define mosi 13
#define cs   15
#define rst  4
#define dc   16

// Color definitions
#define BLACK           0x0000
#define BLUE            0x001F
#define RED             0xF800
#define GREEN           0x07E0
#define CYAN            0x07FF
#define MAGENTA         0xF81F
#define YELLOW          0xFFE0
#define WHITE           0xFFFF

Adafruit_SSD1331 display = Adafruit_SSD1331(cs, dc, mosi, sclk, rst);


//SD Card SPI Pins
#define SS    05
#define MOSI  23
#define MISO  19
#define SCK   18
SPIClass sdSPI(VSPI);

//Define pins to use as virtual GPIO ports, -1 means not implemented
int PortA[8] = { 32, 33, 25, 26, 27,  2, 12, 17};   //Virtual GPIO Port A
int PortB[8] = { 21, 22, -1, -1, -1, -1, -1, -1};   //Virtual GPIO Port B


//BreakPoint switches
#define swA 36                   //Breakpoints on / Off
#endif
//*********************************************************************************************

//*********************************************************************************************
//****                  Configuration for ESP32-S3 Devkit Board                            ****
//*********************************************************************************************
#ifdef S3

#define SS    42
#define MOSI  41
#define MISO  40
#define SCK   39
SPIClass sdSPI(SPI);

//Define pins to use as virtual GPIO ports, -1 means not implemented
int PortA[8] = {  4,  5,  6,  7, 15, 16, 17, 18};   //Virtual GPIO Port A
int PortB[8] = {  8,  3, -1, -1, -1, -1, -1, -1};   //Virtual GPIO Port B


//BreakPoint switches
#define swA 47                   //Breakpoints on / Off

#endif
//*********************************************************************************************


//*********************************************************************************************
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
uint16_t IX = 0;
uint16_t IY = 0;
uint16_t PC = 0;
uint16_t SP = 0;

//Z80 alternate registers
uint8_t   Aa = 0;
uint8_t   Fla = 0;
uint8_t   Ba = 0;
uint8_t   Ca = 0;
uint8_t   Da = 0;
uint8_t   Ea = 0;
uint8_t   Ha = 0;
uint8_t   La = 0;

//Z80 flags
bool Zf = false;                //Zero flag
bool Cf = false;                //Carry flag
bool Sf = false;                //Sign flag (MSBit of A Set)
bool Hf = false;                //Half Carry flag
bool Pf = false;                //Parity / Overflow flag
bool Nf = false;                //Add / Subtract flag

bool RUN = false;               //RUN flag
bool SingleStep = false;        //Single Step flag
bool intE = false;              //Interrupt enable
uint16_t BP = 0xffff;           //Breakpoint
uint8_t BPmode = 0;             //Breakpoint mode
bool bpOn = false;              //BP passed flag
uint8_t OC;                     //Opcode store
uint8_t JR;                     //Signed relative jump
uint8_t V8;                     //8 Bit operand temp storge
uint16_t V16;                   //16 Bit operand temp storge
uint16_t V16a;                  //16 Bit operand temp storge
uint32_t V32;                   //32 Bit operand temp storage used for 16 bit addition and subration
uint8_t v1;                     //Temporary storage
uint8_t v2;                     //Temporary storage
bool cfs;                       //Temp carry flag storage
bool dled;                      //Disk activity flag


uint8_t RAM[65536] = {};        //RAM
uint8_t pOut[256];              //Output port buffer
uint8_t pIn[256];               //Input port buffer
uint8_t rxBuf[1024];            //Serial receive buffer
uint16_t rxInPtr;               //Serial receive buffer input pointer
uint16_t rxOutPtr;              //Serial receive buffer output pointer
uint8_t txBuf[1024];            //Serial transmit buffer
uint16_t txInPtr;               //Serial transmit buffer input pointer
uint16_t txOutPtr;              //Serial transmit buffer output pointer

int vdrive;                     //Virtual drive number
char sdfile[50] = {};           //SD card filename
char sddir[50] = {"/download"}; //SD card path
bool sdfound = true;            //SD Card present flag

TaskHandle_t Task1, Task2, Task3, Task4, Task5, Task6;      //Task handles
SemaphoreHandle_t baton;        //Process Baton, currently not used

//Flags to indicate task startup sequences are complete, used to synchronise task startup order
bool ota_t = false;
bool cpu_t = false;
bool serial_t = false;
bool telnet_t = false;

uint32_t POP[256] ={};
uint32_t POPcb[256] ={};


