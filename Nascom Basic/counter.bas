1 REM ***          Binary Counter for ESP32 Z80 Emulator             ***
2 REM *** binary counter on Port A LEDs and leds on port B alternate ***
5 OUT 1, 255
6 OUT 3, 255
10 A = 0
11 C = 0
20 OUT 0, A
22 FOR B = 1 TO 1000
23 NEXT B
24 IF C = 0 THEN OUT 2, 1
25 IF C = 1 THEN OUT 2,2
26 C = C +1
27 IF C = 2 THEN C = 0
30 A = INP(0)
40 A = A + 1
50 IF A  = 256 THEN A = 0
60 GOTO 20
