/*

DSC.h (
  Sketch to decode the keybus protocol on DSC PowerSeries 1816, 1832 and 1864 panels
   -- Use the schematic at https://github.com/emcniece/Arduino-Keybus to connect the
      keybus lines to the arduino via voltage divider circuits.  Don't forget to 
      connect the Keybus Ground to Arduino Ground (not depicted on the circuit)! You
      can also power your arduino from the keybus (+12 VDC, positive), depending on the 
      the type arduino board you have.
   -- This library does not yet use the CRC32 checksum operation

  Created by stagf15, 19 Jul 2016.
  Released into the public domain.
  
*/

#ifndef DSC_h
#define DSC_h
#include "DSC_Globals.h"
#include "DSC_Constants.h"

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

class DSC : public Print  // Initialize DSC as an extension of the print class
{
  public:
    // Class to call to initialize the DSC Class
    // for example...  DSC dsc;
    DSC(void);
    
    // Used to add the serial instance to the DSC Class
    int addSerial(void);
    
    // Included in the setup function of the user's sketch
    // Begins the the class, sets the pin modes, attaches the interrupt
    void begin(void);
    
    // Included in the main loop of user's sketch, checks and processes 
    // the current panel and keypad words if able
    // Returns:   0   (No 
    int process(void);
    
    // Decodes the panel and keypad words, returns 0 for failure and the command
    // byte for success
    byte decodePanel(void);
    byte decodeKeypad(void);
    
    // Ends, or de-constructs the class - NOT USED  
    //int end();
    
    // Returns the panel and keypad word in formatted binary (returns NULL if failure)
    const char* get_pnlFormat(void);
    const char* get_kpdFormat(void);

    // Returns the panel and keypad word array as Integers (returns NULL if failure)
    const char* get_pnlArray(void);
    const char* get_kpdArray(void);

    // Returns the panel and keypad word in raw binary (returns NULL if failure)
    const char* get_pnlRaw(void);
    const char* get_kpdRaw(void);
    
    // Returns the panel and keypad messages (returns NULL if failure)
    const char* get_pMsg(void);
    const char* get_kMsg(void);
    
    // Returns the panel and keypad command byte 
    byte get_pCmd(void);
    byte get_kCmd(void);
    
    // Sends a keypad key code of four data bytes
    bool send_key(byte aa, byte bb, byte cc, byte dd);
    
    // Returns whether the time is available or not (T or F)
    bool get_time(void);
    
    // Returns whether it's been greater than NO_DATA_TIMEOUT millis 
    // since the last processed word (Generally for DEBUG purposes)
    bool timeout(void);
    
    // Returns 1 if there is a valid checksum, 0 if not
    int pnlChkSum(void);
    
    // Conversion operation functions
    unsigned int binToInt(String &dataStr, int offset, int dataLen);
    //const char* binToChar(String &dataStr, int offset, int endData);  // not needed
    const String byteToBin(byte b, byte digits);
    unsigned int byteToInt(byte* dataArr, int offset, int dataLen, bool padding);
    
    // Used to set the pins to values other than the default
    void setCLK(int p);
    void setDTA_IN(int p);
    void setDTA_OUT(int p);
    void setLED(int p);
    
    // Used to compare two word arrays of equal length (len)
    bool wordCmp(byte *a, byte *b, byte len);

    // Used to copy a byte array to another array of equal length (len)
    // void wordCpy(byte *a, byte *b, byte len);    // Prototype global in DSC.cpp
    
    // Used to reset each element of an array of length (len) to int b
    // void wordSet(byte *a, int b, byte len);      // Prototype global in DSC.cpp

    // ----- Print class extension variables -----
    virtual size_t write(uint8_t);
    virtual size_t write(const char *str);
    virtual size_t write(const uint8_t *buffer, size_t size);

    // Class level variables to hold time elements
    int yy, mm, dd, HH, MM, SS;
    bool timeAvailable;
    bool testFlag;
    
  private:
    uint8_t intrNum;
};

#endif
