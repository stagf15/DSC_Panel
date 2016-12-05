#include "Arduino.h"
#include "DSC.h"
#include "DSC_Constants.h"
#include "DSC_Globals.h"
#include <TextBuffer.h>

/// ----- GLOBAL VARIABLES -----
/*
 * The existence of global variables are "declared" in DSC_Global.h, so that each 
 * source file that includes the header knows about them. The variables must then be
 * “defined” once in one of the source files (this one).

 * The following structures contains all of the global variables used by the ISR to 
 * communicate with the DSC.clkCalled() object. You cannot pass parameters to an 
 * ISR so these values must be global. The fields are defined in DSC_Globals.h
 */
timing_t  timing;
keybus_t  panel;
keybus_t  keypad;

// ----- Input/Output Pins (Global, defined in DSC_Globals.h) -----
byte CLK;         // Keybus Yellow (Clock Line)
byte DTA_IN;      // Keybus Green (Data Line via V divider)
byte DTA_OUT;     // Keybus Green Output (Data Line through driver)
byte LED;         // LED pin on the arduino

// Prototype for interrupt handler, called on clock line change
void clkCalled_Handler(); 

// Prototype for wordCpy, to copy an array to another array of equal length (len)
void wordCpy(byte *a, byte *b, byte len);

// Prototype for wordSet, to reset each element of an array of length (len) to int b
void wordSet(byte *a, int b, byte len);

//TextBuffer pTempByte(12);       // Initialize TextBuffer.h for temp panel byte buffer
//TextBuffer kTempByte(12);       // Initialize TextBuffer.h for temp keypad byte buffer

TextBuffer tempByte(12);          // Initialize TextBuffer.h for temp generic byte buffer 
TextBuffer pBuffer(WORD_BITS);    // Initialize TextBuffer.h for panel word
TextBuffer pMsg(MSG_BITS);        // Initialize TextBuffer.h for panel message
TextBuffer kBuffer(WORD_BITS);    // Initialize TextBuffer.h for keypad word
TextBuffer kMsg(MSG_BITS);        // Initialize TextBuffer.h for keypad message

/// --- END GLOBAL VARIABLES ---

DSC::DSC(void)
  {
    // ----- Time Variables -----
    // Volatile variables, modified within ISR, based on micros()
    timing.intervalTimer = 0;   
    timing.clockChange = 0;
    timing.lastChange = 0;      
    timing.lastRise = 0;         // NOT USED YET
    timing.lastFall = 0;         // NOT USED YET
    
    // Time variables, based on millis()
    timing.lastStatus = 0;
    timing.lastData = 0;

    // Class level variables to hold time elements
    int yy = 0, mm = 0, dd = 0, HH = 0, MM = 0, SS = 0;
    bool timeAvailable = false;     // Changes to true when kCmd == 0xa5 to 
                                    // indicate that the time elements are valid

    // ----- Input/Output Pins (DEFAULTS) ------
    //   These can be changed prior to DSC.begin() using functions below
    CLK      = 3;    // Keybus Yellow (Clock Line)
    DTA_IN   = 4;    // Keybus Green (Data Line via V divider)
    DTA_OUT  = 8;    // Keybus Green Output (Data Line through driver)
    LED      = 13;   // LED pin on the arduino Uno

    // ----- Keybus Word Byte Array Vars -----
    wordSet(panel.newArray, 0, ARR_SIZE);
    wordSet(panel.array, 0, ARR_SIZE);
    wordSet(panel.oldArray, 0, ARR_SIZE);
    panel.bit = 0, panel.elem = 0;
    
    wordSet(keypad.newArray, 0, ARR_SIZE);
    wordSet(keypad.array, 0, ARR_SIZE);
    wordSet(keypad.oldArray, 0, ARR_SIZE);
    keypad.bit = 0, keypad.elem = 0;

    // ----- Keybus Word Byte Lengths -----
    panel.newArrayLen = 0, panel.arrayLen = 0; //panel.oldLen = 0; 
    keypad.newArrayLen = 0, keypad.arrayLen = 0; //keypad.oldLen = 0; 

    panel.cmd = 0, keypad.cmd = 0;
    
    byteBuf1 = "";
    byteBuf2 = "";
    byteBuf3 = "";
  }

int DSC::addSerial(void)
  {
  // Not yet implemented
  }

void DSC::begin(void)
  {
    pinMode(CLK, INPUT);
    pinMode(DTA_IN, INPUT);
    pinMode(DTA_OUT, OUTPUT);
    pinMode(LED, OUTPUT);

    tempByte.begin();         // Begin the generic tempByte buffer, allocate memory
    pBuffer.begin();          // Begin the panel word buffer, allocate memory
    pMsg.begin();             // Begin the panel message buffer, allocate memory
    kBuffer.begin();          // Begin the keypad word buffer, allocate memory
    kMsg.begin();             // Begin the keypad message buffer, allocate memory

    // Set the interrupt pin
    intrNum = digitalPinToInterrupt(CLK);

    // Attach interrupt on the CLK pin
    attachInterrupt(intrNum, clkCalled_Handler, CHANGE);  
    //   Changed from RISING to CHANGE to read both panel and keypad data
  }

/* This is the interrupt handler used by this class. It is called every time the input
 * pin changes from high to low or from low to high.
 *
 * The function is not a member of the DSC class, it must be in the global scope in order 
 * to be called by attachInterrupt() from within the DSC class.
 */
void clkCalled_Handler() 
  { 
    timing.clockChange = micros();                  // Save the current clock change time 
    timing.intervalTimer =  
        (timing.clockChange - timing.lastChange);   // Determine interval since last clock change 
    
    if (timing.intervalTimer > (NEW_WORD_INTV - 200)) { 
      wordCpy(keypad.newArray, keypad.array, ARR_SIZE); // Save the complete keypad raw data bytes array 
      keypad.arrayLen = keypad.newArrayLen;             // Copy the word length
      
      wordSet(keypad.newArray, 0, ARR_SIZE);  // Reset the raw data bytes keypad array being built
      keypad.newArrayLen = 0;                 // Reset the new keypad word length to zero
      keypad.bit = 0;				                  // Reset the keypad bit counter to zero
      keypad.elem = 0; 				                // Reset the keypad byte counter to zero
    } 
    timing.lastChange = timing.clockChange; // Re-save the current change time as last change time 
    
    if (digitalRead(CLK)) {                 // If clock line is going HIGH, this is PANEL data 
      timing.lastRise = timing.lastChange;  // Set the lastRise time    
      
      if (panel.elem < ARR_SIZE) {      	  // Limit the array to X bytes
        //delayMicroseconds(120);           // Delay for 120 us to get a valid data line read 
        panel.newArray[panel.elem] <<= 1;
        if (digitalRead(DTA_IN)) panel.newArray[panel.elem] |= 1; 
        panel.newArrayLen++;
        // Increment the panel elem (byte) and bit counters as required
        if (panel.elem == 0 and panel.bit == 7) { 
          panel.elem = 1; panel.bit = 6; }  // Set the pByte/Bit counter for zero padding
        if (panel.bit < 7) 
          panel.bit++;
        else { 
          panel.elem++; panel.bit = 0; }    // Increment pByte counter if 8 bits
      } 
    } 
    
    else {                                  // Otherwise, it's going LOW, this is KEYPAD data 
      timing.lastFall = timing.lastChange;  // Set the lastFall time 

      if (keypad.elem < ARR_SIZE) {      	  // Limit the array to X bytes  
        //delayMicroseconds(200);           // Delay for 300 us to get a valid data line read 
        keypad.newArray[keypad.elem] <<= 1;
        if (digitalRead(DTA_IN)) keypad.newArray[keypad.elem] |= 1;  
        keypad.newArrayLen++;
        // Increment the keypad elem (byte) and bit counters as required
        if (keypad.bit < 7) 
          keypad.bit++;
        else { 
          keypad.elem++; keypad.bit = 0; }  // Increment kByte counter if 8 bits
      } 
    } 
  }

// ----- The following are DSC class level functions -----

int DSC::process(void)
  {
    // ------------ Get/process incoming data -------------
    panel.cmd = 0; 
    keypad.cmd = 0; 
    timeAvailable = false;      // Set the time element status to invalid
    
    // ----------------- Turn on/off LED ------------------
    if ((millis() - timing.lastStatus) > 500)
      digitalWrite(LED, 0);     // Turn LED OFF (no recent status command [0x05])
    else
      digitalWrite(LED, 1);     // Turn LED ON  (recent status command [0x05])
    
    /*
     * The normal clock frequency is 1 Hz or one cycle every ms (1000 us) 
     * The new word marker is clock high for about 15 ms (15000 us)
     * If the interval is longer than the required amount (NEW_WORD_INTV + 200 us), 
     * and the panel word in progress (pBuild) is more than 8 characters long,
     * process the panel and keypad words, otherwise return failure (0).
     */

    if (timing.intervalTimer < (NEW_WORD_INTV + 200)) return -1;  // Still building word
    if (panel.newArrayLen < 8) return -2;                         // Complete word too short

    wordCpy(panel.newArray, panel.array, ARR_SIZE); // Save the complete panel raw data bytes array 
    panel.arrayLen = panel.newArrayLen;             // Copy the word length
    
    wordSet(panel.newArray, 0, ARR_SIZE);   // Reset the raw data bytes panel array being built
    panel.newArrayLen = 0;                  // Reset the new panel word length to zero
    panel.bit = 0;				                  // Reset the panel bit counter to zero
    panel.elem = 0; 				                // Reset the panel byte counter to zero
    
    panel.cmd = decodePanel();        // Decode the panel binary, return command byte, or 0
    keypad.cmd = decodeKeypad();      // Decode the keypad binary, return command byte, or 0
    
    if (panel.cmd && keypad.cmd) return 3;  // Return 3 if both were decoded
    else if (keypad.cmd) return 2;          // Return 2 if keypad word was decoded
    else if (panel.cmd) return 1;           // Return 1 if panel word was decoded
    else return 0;                          // Return failure if none were decoded
  }

byte DSC::decodePanel(void) 
  {
    pMsg.clear();                     // Initialize panel message for output
    // ------------- Process the Panel Data Word ---------------
    byte cmd = panel.array[0];        // Get the panel Cmd (data word type/command)

    if (wordCmp(panel.array, panel.oldArray, ARR_SIZE) || cmd == 0x00) {
      // Skip this word if the data hasn't changed, or pCmd is empty (0x00)
      return 0;     // Return failure
    }
    else {     
      // This seems to be a valid word, try to process it  
      timing.lastData = millis();                     // Record the time (last data word was received)
      wordCpy(panel.array, panel.oldArray, ARR_SIZE); // This is a new/good word, save it   
      //panel.oldLen = panel.arrayLen;                // Copy the word length
      
      // Interpret the data
      if (cmd == 0x05) 
      {
        timing.lastStatus = millis();        // Record the time for LED logic
        pMsg.print(F("[Status] "));
        if (byteToInt(panel.array,16,1,1))    pMsg.print(F("Ready"));
        else {
          if (!byteToInt(panel.array,15,1,1)) pMsg.print(F("Not Ready"));
          else                                pMsg.print(F("Armed")); }
                                           
        if (byteToInt(panel.array,12,1,1))    pMsg.print(F(", Error"));
        if (byteToInt(panel.array,13,1,1))    pMsg.print(F(", Bypass"));
        if (byteToInt(panel.array,14,1,1))    pMsg.print(F(", Memory"));
        
        if (byteToInt(panel.array,17,1,1))    pMsg.print(F(", Program"));
        if (byteToInt(panel.array,29,1,1))    pMsg.print(F(", Power Fail"));   // ??? - maybe 28 or 20?
      }
     
      if (cmd == 0xa5)
      {
        pMsg.print(F("[Info] "));
        
        int y3 = byteToInt(panel.array,9,4,1);
        int y4 = byteToInt(panel.array,13,4,1);
        yy = (String(y3) + String(y4)).toInt();
        mm = byteToInt(panel.array,19,4,1);
        dd = byteToInt(panel.array,23,5,1);
        HH = byteToInt(panel.array,28,5,1);
        MM = byteToInt(panel.array,33,6,1);     

        timeAvailable = true;         // Set the time element status to valid

        byte arm = byteToInt(panel.array,41,2,1);
        byte master = byteToInt(panel.array,43,1,1);
        byte user = byteToInt(panel.array,43,6,1); // 0-36
        if (arm == 0x02) {
          pMsg.print(F(", Armed"));
          user = user - 0x19;
        }
        if (arm == 0x03) {
          pMsg.print(F(", Disarmed"));
        }
        if (arm > 0) {
          if (master) pMsg.print(F(", Master Code")); 
          else        pMsg.print(F(", User Code"));
          user += 1;                  // shift to 1-32, 33, 34
          if (user > 34) user += 5;   // convert to system code 40, 41, 42
          pMsg.print(" "); pMsg.print(user);
        }
      }
      
      if (cmd == 0x27)
      {
        pMsg.print(F("[Zones A] "));
        int zones = byteToInt(panel.array,8+1+8+8+8+8,8,1);
        if (zones & 1) pMsg.print("1 ");
        if (zones & 2) pMsg.print("2 ");
        if (zones & 4) pMsg.print("3 ");
        if (zones & 8) pMsg.print("4 ");
        if (zones & 16) pMsg.print("5 ");
        if (zones & 32) pMsg.print("6 ");
        if (zones & 64) pMsg.print("7 ");
        if (zones & 128) pMsg.print("8 ");
        if (zones == 0) pMsg.print("Ready ");
      }
      
      if (cmd == 0x2d)
      {
        pMsg.print(F("[Zones B] "));
        int zones = byteToInt(panel.array,8+1+8+8+8+8,8,1);
        if (zones & 1) pMsg.print("9 ");
        if (zones & 2) pMsg.print("10 ");
        if (zones & 4) pMsg.print("11 ");
        if (zones & 8) pMsg.print("12 ");
        if (zones & 16) pMsg.print("13 ");
        if (zones & 32) pMsg.print("14 ");
        if (zones & 64) pMsg.print("15 ");
        if (zones & 128) pMsg.print("16 ");
        if (zones == 0) pMsg.print("Ready ");
      }
      
      if (cmd == 0x34)
      {
        pMsg.print(F("[Zones C] "));
        int zones = byteToInt(panel.array,8+1+8+8+8+8,8,1);
        if (zones & 1) pMsg.print("17 ");
        if (zones & 2) pMsg.print("18 ");
        if (zones & 4) pMsg.print("19 ");
        if (zones & 8) pMsg.print("20 ");
        if (zones & 16) pMsg.print("21 ");
        if (zones & 32) pMsg.print("22 ");
        if (zones & 64) pMsg.print("23 ");
        if (zones & 128) pMsg.print("24 ");
        if (zones == 0) pMsg.print("Ready ");
      }
      
      if (cmd == 0x3e)
      {
        pMsg.print(F("[Zones D] "));
        int zones = byteToInt(panel.array,8+1+8+8+8+8,8,1);
        if (zones & 1) pMsg.print("25 ");
        if (zones & 2) pMsg.print("26 ");
        if (zones & 4) pMsg.print("27 ");
        if (zones & 8) pMsg.print("28 ");
        if (zones & 16) pMsg.print("29 ");
        if (zones & 32) pMsg.print("30 ");
        if (zones & 64) pMsg.print("31 ");
        if (zones & 128) pMsg.print("32 ");
        if (zones == 0) pMsg.print("Ready ");
      }
      // --- The other 32 zones for a 1864 panel need to be added after this ---

      if (cmd == 0x11) 
        pMsg.print(F("[Keypad Query] "));
      if (cmd == 0x0a)
        pMsg.print(F("[Panel Program Mode] "));
      if (cmd == 0x5d)
        pMsg.print(F("[Alarm Memory Group 1] "));
      if (cmd == 0x63)
        pMsg.print(F("[Alarm Memory Group 2] "));
      if (cmd == 0x64)
        pMsg.print(F("[Beep Command Group 1] "));
      if (cmd == 0x69)
        pMsg.print(F("[Beep Command Group 2] "));
      if (cmd == 0x39)
        pMsg.print(F("[Undefined command from panel] "));
      if (cmd == 0xb1)
        pMsg.print(F("[Zone Configuration] "));
        
    return cmd;     // Return success
    }
  }

byte DSC::decodeKeypad(void) 
  {
    kMsg.clear();                       // Initialize keypad message for output 
    // ------------- Process the Keypad Data Word ---------------
    byte cmd = keypad.array[0];         // Get the keypad Cmd (data word type/command)
    String btnStr = F("[Button] ");
    
    if ((keypad.array[0] == 255 && keypad.array[1] == 255 &&
         keypad.array[2] == 255 && keypad.array[3] == 255) || 
        (keypad.array[0] == 0x00)) {  
      // Skip this word if kArray is all 1's or kCmd is empty (0x00)
      return 0;     // Return failure
    }

    else { 
      // This seems to be a valid word, try to process it
      timing.lastData = millis();                       // Record the time (last data word was received)
      wordCpy(keypad.array, keypad.oldArray, ARR_SIZE); // This is a new/good word, save it   
      //keypad.oldLen = keypad.arrayLen;                // Copy the word length

      byte kByte2 = keypad.array[1]; //byteToInt(keypad.array,8,8,0); 
      byte kByte3 = keypad.array[2];
      byte kByte4 = keypad.array[3];
     
      // Interpret the data
      if (cmd == kOut) {
        if (kByte2 == one) {
          kMsg.print(btnStr); kMsg.print("1"); }
        else if (kByte2 == two) {
          kMsg.print(btnStr); kMsg.print("2"); }
        else if (kByte2 == three) {
          kMsg.print(btnStr); kMsg.print("3"); }
        else if (kByte2 == four) {
          kMsg.print(btnStr); kMsg.print("4"); }
        else if (kByte2 == five) {
          kMsg.print(btnStr); kMsg.print("5"); }
        else if (kByte2 == six) {
          kMsg.print(btnStr); kMsg.print("6"); }
        else if (kByte2 == seven) {
          kMsg.print(btnStr); kMsg.print("7"); }
        else if (kByte2 == eight) {
          kMsg.print(btnStr); kMsg.print("8"); }
        else if (kByte2 == nine) {
          kMsg.print(btnStr); kMsg.print("9"); }
        else if (kByte2 == aster) {
          kMsg.print(btnStr); kMsg.print("*"); }
        else if (kByte2 == zero) {
          kMsg.print(btnStr); kMsg.print("0"); }
        else if (kByte2 == pound) {
          kMsg.print(btnStr); kMsg.print("#"); }
        else if (kByte2 == stay) {
          kMsg.print(btnStr); kMsg.print(F("Stay")); }
        else if (kByte2 == away) {
          kMsg.print(btnStr); kMsg.print(F("Away")); }
        else if (kByte2 == chime) {
          kMsg.print(btnStr); kMsg.print(F("Chime")); }
        else if (kByte2 == reset) {
          kMsg.print(btnStr); kMsg.print(F("Reset")); }
        else if (kByte2 == kExit) {
          kMsg.print(btnStr); kMsg.print(F("Exit")); }
        else if (kByte2 == lArrow) {  // These arrow commands don't work every time
          kMsg.print(btnStr); kMsg.print(F("<")); }
        else if (kByte2 == rArrow) {  // They are often reverse for unknown reasons
          kMsg.print(btnStr); kMsg.print(F(">")); }
        else if (kByte2 == kOut)
          kMsg.print(F("[Keypad Response]"));
        else {
          kMsg.print("[Keypad] 0x"); 
          kMsg.print(String(kByte2, HEX)); kMsg.print(" (Unknown)");
        }
      }

      if (cmd == fire) {
        kMsg.print(btnStr); kMsg.print(F("Fire")); }
      if (cmd == aux) {
        kMsg.print(btnStr); kMsg.print(F("Auxillary")); }
      if (cmd == panic) {
        kMsg.print(btnStr); kMsg.print(F("Panic")); }
      
      return cmd;                         // Return success
    }
  }

const char* DSC::get_pnlFormat(void)
  {
    if (!panel.cmd) return NULL;          // return failure
    // Formats the panel word array into bytes of binary data in the form:
    // 8 1 8 8 8 8 8 etc, and returns a pointer to the buffer 
    pBuffer.clear();
    pBuffer.print("[Panel]  ");
    int bitsRem = panel.arrayLen;

    if (panel.arrayLen > 8) {
      pBuffer.print(byteToBin(panel.array[0], 8)); bitsRem -= 8;
      pBuffer.print(" ");
      pBuffer.print(panel.array[1]); bitsRem -= 1;
      pBuffer.print(" ");
      int grps = (panel.arrayLen - 2) / 8;
      for(int i=0;i<grps;i++) {
        if (bitsRem > 7) {
          pBuffer.print(byteToBin(panel.array[i + 2], 8));
          pBuffer.print(" "); }
        else pBuffer.print(byteToBin(panel.array[i + 2], bitsRem));
        bitsRem -= 8;
      }
    }
    else
      pBuffer.print(byteToBin(panel.array[0], bitsRem));

    if (pnlChkSum()) pBuffer.print(" (OK)");

    return pBuffer.getBuffer();           // return the pointer
  }

const char* DSC::get_kpdFormat(void)
  {
    if (!keypad.cmd) return NULL;         // return failure
    // Formats the keypad word array into bytes of binary data in the form:
    // 8 8 8 8 8 8 etc, and returns a pointer to the buffer 
    kBuffer.clear();
    kBuffer.print("[Keypad] ");
    int bitsRem = keypad.arrayLen;
    
    if (keypad.arrayLen > 8) {
      int grps = keypad.arrayLen / 8;
      for(int i=0;i<grps;i++) {
        if (bitsRem > 7) {
          kBuffer.print(byteToBin(keypad.array[i], 8));
          kBuffer.print(" "); }
        else kBuffer.print(byteToBin(keypad.array[i], bitsRem));
        bitsRem -= 8;
      }
    }
    else
      kBuffer.print(byteToBin(keypad.array[0], bitsRem));

    return kBuffer.getBuffer();           // return the pointer
  }

const char* DSC::get_pnlArray(void)
  {
    if (!panel.cmd) return NULL;          // return failure
    // Formats the panel word array into bytes in the form:
    // 8 1 8 8 8 8 8 etc, and returns a pointer to the buffer 
    pBuffer.clear();
    pBuffer.print("[Panel]  ");

    if (panel.arrayLen > 8) {
      pBuffer.print(panel.array[0]);
      pBuffer.print(" ");
      pBuffer.print(panel.array[1]);
      pBuffer.print(" ");
      int grps = (panel.arrayLen - 2) / 8;
      for(int i=0;i<grps;i++) {
        pBuffer.print(panel.array[i + 2]);
        if (i<(grps-1)) pBuffer.print(" ");
      }
    }
    else
      pBuffer.print(panel.array[0]);

    if (pnlChkSum()) pBuffer.print(" (OK)");

    return pBuffer.getBuffer();           // return the pointer
  }

const char* DSC::get_kpdArray(void)
  {
    if (!keypad.cmd) return NULL;         // return failure
    // Formats the keypad word array into bytes in the form:
    // 8 8 8 8 8 8 etc, and returns a pointer to the buffer 
    kBuffer.clear();
    kBuffer.print("[Keypad] ");
    
    if (keypad.arrayLen > 8) {
      int grps = keypad.arrayLen / 8;
      for(int i=0;i<grps;i++) {
        kBuffer.print(keypad.array[i]);
        if (i<(grps-1)) kBuffer.print(" ");
      }
    }
    else
      kBuffer.print(keypad.array[0]);

    return kBuffer.getBuffer();           // return the pointer
  }

const char* DSC::get_pnlRaw(void)
  {
    if (!panel.cmd) return NULL;          // return failure
    // Puts the raw binary word into a buffer and returns a pointer to the buffer
    pBuffer.clear();
    pBuffer.print("[Panel]  ");
    int bitsRem = panel.arrayLen;
    
    if (panel.arrayLen > 8) {
      pBuffer.print(byteToBin(panel.array[0], 8)); bitsRem -= 8;
      pBuffer.print(panel.array[1]); bitsRem -= 1;
      int grps = (panel.arrayLen - 2) / 8;
      for(int i=0;i<grps;i++) {
        if (bitsRem > 7) pBuffer.print(byteToBin(panel.array[i + 2], 8)); 
        else pBuffer.print(byteToBin(panel.array[i + 2], bitsRem));
        bitsRem -= 8;
      }
    }
    else
      pBuffer.print(byteToBin(panel.array[0], bitsRem));

    if (pnlChkSum()) pBuffer.print(" (OK)");

    return pBuffer.getBuffer();           // return the pointer
  }

const char* DSC::get_kpdRaw(void)
  {
    if (!keypad.cmd) return NULL;         // return failure
    // Puts the raw binary word into a buffer and returns a pointer to the buffer
    kBuffer.clear();
    kBuffer.print("[Keypad] ");
    int bitsRem = keypad.arrayLen;
    
    if (keypad.arrayLen > 8) {
      int grps = keypad.arrayLen / 8;
      for(int i=0;i<grps;i++) {
        if (bitsRem > 7) kBuffer.print(byteToBin(keypad.array[i], 8));
        else kBuffer.print(byteToBin(keypad.array[i], bitsRem));
        bitsRem -= 8;
      }
    }
    else
      kBuffer.print(byteToBin(keypad.array[0], bitsRem));

    return kBuffer.getBuffer();           // return the pointer
  }

int DSC::pnlChkSum(void)
  {
    // Sums all but the last full byte (minus padding) and compares to last byte
    // returns 0 if not valid, and the checksum if it's valid
    int cSum = 0;
    if (panel.arrayLen >= 17) {
      cSum += panel.array[0];
      int grps = (panel.arrayLen - 9) / 8; 
      byteBuf2 = panel.arrayLen;
      for(int i=0;i<grps;i++) {
        if (i<(grps-1)) 
          cSum += panel.array[i + 2];
        else {
          byte cSumMod = cSum % 256;
          byte lastByte = panel.array[i + 2];
          if (cSumMod == lastByte) return cSumMod;
        }
      }
    }
    return 0;
  }

const char* DSC::get_pMsg(void)
  {
    if (!panel.cmd) return NULL;          // return failure
    return pMsg.getBuffer();              // return the pointer
  }

const char* DSC::get_kMsg(void)
  {
    if (!keypad.cmd) return NULL;         // return failure
    return kMsg.getBuffer();              // return the pointer
  }

byte DSC::get_pCmd(void)
  {
    return panel.cmd;                     // return pCmd
  }

byte DSC::get_kCmd(void)
  {
    return keypad.cmd;                    // return kCmd
  }

bool DSC::get_time(void)
  {
    return timeAvailable;                 // return kCmd
  }

bool DSC::timeout(void)
  {
    bool s = false;
    if ((millis() - timing.lastData) > NO_DATA_TIMEOUT) s = true;
    timing.lastData = millis();           // Reset the timer
    return s;                             // return timeout status
  }

unsigned int DSC::byteToInt(byte* dataArr, int offset, int dataLen, bool padding)
  {
    // Returns the value of the binary data in the byte from "offset" to "dataLen" as an int
    int byteNum = 0;                      // Int automatically rounds down
    // If padding is true, then the second byte should only be one bit long (panel data)
    if (padding && offset > 7) {
      if (offset == 8) {
        byteNum = 1;
        offset = 0;
      }
      if (offset > 8) {
        byteNum = (offset + 7) / 8;       
        offset = offset - ((byteNum - 1) * 8 + 1);
      }
    }
    // If padding is false, there is no zero padding bit (keypad data), or if there 
    // is padding and the offset is less <= 7, this is the first byte
    else {
      byteNum = offset / 8;               
      offset = offset - (byteNum * 8);
    }
    
    // Convert the byte, and the one following, to 16 binary digits
    String bothBytes = byteToBin(dataArr[byteNum], 8) + byteToBin(dataArr[byteNum + 1], 8);
    int iBuf = 0;
    for(int j=0;j<dataLen;j++) {
      iBuf <<= 1;
      if (bothBytes[offset+j] == '1') iBuf |= 1;
    }
    return iBuf;
  }

const char* DSC::binToChar(String &dataStr, int offset, int endData)
  {   
    tempByte.clear();
    // Returns a char array of the binary data in the String from "offset" to "endData"
    tempByte.print(dataStr[offset]);
    for(int j=1;j<(endData-offset);j++) {
      tempByte.print(dataStr[offset+j]);
    }
    return tempByte.getBuffer();
  }

const String DSC::byteToBin(byte b, byte digits)
  {
    // Returns the X bit binary representation of byte "b" with leading zeros
    // where X is the number of binary digits up to 8
    if (digits > 8) digits = 8;
    int zeros = abs(digits)-String(b, BIN).length();
    String zStr = "";
    for (int i=0;i<zeros;i++) zStr += "0";
    return zStr + String(b, BIN);
  }

void DSC::setCLK(int p)
  {
    // Sets the clock pin, must be called prior to begin()
    CLK = p;
  }

void DSC::setDTA_IN(int p)
  {
    // Sets the data in pin, must be called prior to begin()
    DTA_IN = p;
  }
  
void DSC::setDTA_OUT(int p)
  {
    // Sets the data out pin, must be called prior to begin()
    DTA_OUT = p;
  }
void DSC::setLED(int p)
  {
    // Sets the LED pin, must be called prior to begin()
    LED = p;
  }

size_t DSC::write(uint8_t character) 
  { 
    // Code to display letter when given the ASCII code for it
    // Not yet implemented
  }

size_t DSC::write(const char *str) 
  { 
    // Code to display string when given a pointer to the beginning -- 
    // remember, the last character will be null, so you can use a while(*str). 
    // You can increment str (str++) to get the next letter
    // Not yet implemented
  }
  
size_t DSC::write(const uint8_t *buffer, size_t size) 
  { 
    // Code to display array of chars when given a pointer to the beginning 
    // of the array and a size -- this will not end with the null character
    // Not yet implemented
  }

bool DSC::wordCmp(byte *a, byte *b, byte len)
  {
    // test each element to be the same. if not, return false
    for (byte n=0;n<len;n++) if (a[n]!=b[n]) return 0;

    // if it has not returned yet, they are equal
    return 1;
  }

/*
 / These last two functions are also not members of the DSC class.  They are in the 
 / global scope so they can be called by the interrupt handler
*/

void wordCpy(byte *a, byte *b, byte len)
  {
    // copy each element in byte array a of length len to byte array b
    for (byte n=0;n<len;n++) b[n]=a[n];
  }
  
void wordSet(byte *a, int b, byte len)
  {
    // set each element in byte array a of length len to int b
    for (byte n=0;n<len;n++) a[n]=b;
  }
///////// END //////////
