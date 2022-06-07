#pragma once

bool diskRead(fs::FS & fs);
bool diskWrite(fs::FS & fs);
void SDfileOpen(fs::FS & fs);
bool SDfileRead(fs::FS & fs);
void SDprintDir(fs::FS & fs);
bool SDsetPath(fs::FS & fs);
void bootstrap(void);
void FileToRAM(char c[], uint16_t l, bool sd);



//*********************************************************************************************
//****                  Load bootstrap binaries from SD Card or SPIFFS                     ****
//*********************************************************************************************
void bootstrap(void) {
  File boot;
  //Try and boot from SD Card or SPIFFS
  if (sdfound == true) {
    Serial.println("\n\rBooting from SD Card");
    boot = SD.open("/boot.txt");
    if (boot == 0) Serial.println("boot.txt not found");
  } else {

    Serial.println("\n\rBooting from SPIFFS");
    if (!SPIFFS.begin(true)) {
      Serial.println("An Error has occurred while mounting SPIFFS");
      while (1);
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
          if (linebuf[l][0] == '/') l++;  //Ignore lines that don't start with /
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
    FileToRAM(Fs, Add, sdfound);  //sdfound: false means load from SPIFFS
  }
  boot.close();
}


//*********************************************************************************************
//****                             Print SD card directory                                 ****
//*********************************************************************************************
void SDprintDir(fs::FS & fs) {                                     // Show files in /download
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

//*********************************************************************************************
//****                                 Set SD card path                                    ****
//*********************************************************************************************
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
  //File dir = SD.open(d);                          // Check if path is valid
  File dir = fs.open(d, FILE_READ);                 // Check if path is valid
  if (!dir) {
    Serial.println("Invalid path");
    return (false);
  }
  dir.close();
  for (i = 0 ; i < 50; i++) sddir[i] = d[i];        // Copy path to global variable
  Serial.print("Setting SD Card path to: ");
  Serial.println(sddir);
  return (true);
}

//*********************************************************************************************
//****                              Open file on SD card                                   ****
//*********************************************************************************************
void SDfileOpen(fs::FS & fs) {
  uint16_t sdbuffer;
  sdbuffer = pOut[DPARM + 1] << 8 | pOut[DPARM];      // Get buffer address
  int n;
  int nn;
  for (nn = 0 ; nn < 50 ; nn++) {
    sdfile[nn] = sddir[nn];                           // Get current directory
    if (sddir[nn] == 0) break;                        // Null so end of string
  }
  sdfile[nn] = '/';                                   // append a /
  nn++;

  for (n = 0; n < 12; n++) sdfile[n + nn] = pOut[DPARM + n + 2];  // Append filename
  unsigned int blocks = 0;
  Serial.printf("open: %s ", sdfile);
  //File f = SD.open(sdfile, FILE_READ);
  File f = fs.open(sdfile, FILE_READ);
  if (!f) {
    Serial.println("file open failed");
  } else {
    blocks = f.size() / 128;                          // Calculate the number of 128 byte blocks
    if ((f.size() % 128) > 0) blocks++;               // Round up if not an exact number of 128 byte blocks
    Serial.printf("%7d Bytes %5d Block(s)", f.size(), blocks);
  }
  Serial.println();
  f.close();

  RAM[sdbuffer + 1] = blocks >> 8;
  RAM[sdbuffer]     = blocks & 0xff;                  // Write the number of blocks to the buffer
}

//*********************************************************************************************
//****           Read SD card file to Z80 memory for sdcopy.com                            ****
//*********************************************************************************************
bool SDfileRead(fs::FS & fs) {                            // Read block from file command
  int s = pOut[DPARM + 3] << 8 | pOut[DPARM + 2];         // Block Number
  int a = pOut[DPARM + 1] << 8 | pOut[DPARM];             // Get buffer address
  digitalWrite(LED_BUILTIN, HIGH);
  dled = true;
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
  digitalWrite(LED_BUILTIN, LOW);
  dled = false;
  return (true);
}

//*********************************************************************************************
//****                       Z80 Virtual disk write function                               ****
//*********************************************************************************************
bool diskWrite(fs::FS & fs) {
  digitalWrite(LED_BUILTIN, HIGH);
  dled = true;
  uint32_t s = pOut[DPARM + 4] << 16 | pOut[DPARM + 3] << 8 | pOut[DPARM + 2];
  uint16_t a = pOut[DPARM + 1] << 8 | pOut[DPARM];
  vdrive = s >> 14;
  s = s & 0x3fff;
  char dd[] = "/disks/A.dsk";
  dd[7] = vdrive + 65;
  File f = fs.open(dd, "r+w+");
  if (!f) {
    Serial.println("Virtual Disk file open failed");
    return (false);
  }
  f.seek(s * 512);                                          // Seek to first byte of sector
  //16 Disks with 512 tracks, 128 Sectors per track, 128 byte per sector
  for (int i = 0 ; i < 512 ; i++) {
    f.write(RAM[a + i]);
  }
  f.close();
  digitalWrite(LED_BUILTIN, LOW);
  dled = false;
  return (true);
}

//*********************************************************************************************
//****                       Z80 Virtual disk read function                               ****
//*********************************************************************************************
bool diskRead(fs::FS & fs) {
  digitalWrite(LED_BUILTIN, HIGH);
  dled = true;
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
    RAM[a + i] = f.read();
  }
  f.close();
  digitalWrite(LED_BUILTIN, LOW);
  dled = false;
  return (true);
}



//*********************************************************************************************
//****                load boot file from SD Card or SPIFFS into RAM                       ****
//*********************************************************************************************
//Read file from SD Card or SPIFFS into RAM
void FileToRAM(char c[], uint16_t l, bool sd) {
  digitalWrite(LED_BUILTIN, HIGH);
  dled = true;
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
  dled = false;
}
