#pragma once
#ifdef T2
//*********************************************************************************************
//****                            OLED display task                                        ****
//*********************************************************************************************
void OLEDTask(void *parameter) {
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
      oled1 = pOut[0];
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
      oled2 = pOut[02];
    }

    if (dled != dledO) {
      dledO = dled;
      if (dled == true) {
        display.fillCircle(6, 39, 4, BLUE);
      } else {
        display.fillCircle(6, 39, 4, BLACK);
      }
    }

    vTaskDelay(1);
  }
}
#endif