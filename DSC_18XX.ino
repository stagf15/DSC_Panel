// DSC_18XX Arduino Interface ver. 1.5
//
// Sketch to decode the keybus protocol on DSC PowerSeries 1816, 1832 and 1864 panels
//   -- Use the schematic at https://github.com/emcniece/Arduino-Keybus to connect the
//      keybus lines to the arduino via voltage divider circuits.  Don't forget to 
//      connect the Keybus Ground to Arduino Ground (not depicted on the circuit)! You
//      can also power your arduino from the keybus (+12 VDC, positive), depending on the 
//      the type arduino board you have.
//   -- The initial code from the github website, while claiming to use the CRC32 Library
//      did not seem to reference that library.  This sketch [may] use it, so be sure to 
//      download the CRC32 library from the github above and include it in your sketch
//      (Though up till this point [29 Sep 16] I am not sure what it does...
//
// Versions:
// 1.0 - 3 Sep 16 
//      -- Original sketch obtained from: https://github.com/emcniece/Arduino-Keybus
//      -- Additional code segments added from: http://www.avrfreaks.net/forum/dsc-keybus-protocol
//      -- The arduino will process the lines of binary data from the DSC panel on the
//         keybus, while listening for connections via an Arduino ethernet shield
//      -- The connection acts as a TCP "Serial Monitor", streaming the output over TCP, and 
//         can be accessed by navigating to the IP address in the initialization message on the 
//         serial monitor, followed by /STREAM.  (Example:  http://192.168.1.45/STREAM)
// 1.1 - Added ability to read Keypad messages from the data line on the downward flank of 
//       the clock signal. (May still need work on timing, but data seems good)
// 1.2 - Changed timing and logic involved with reading and processing words from the keypad
//       and panel to make it more reliable
// 1.3 - 
//

#include <SPI.h>
#include <CRC32.h>
#include <Ethernet.h>
#include <TextBuffer.h>
#include <TimeLib.h>

#define CLK 3       // Keybus Yellow (Clock Line)
#define DTA_IN 4    // Keybus Green (Data Line via V divider)
#define DTA_OUT 8   // Keybus Green Output (Data Line through driver)
#define LED_PIN 13  // LED pin on the arduino

const char hex[] = "0123456789abcdef";
// ----- KEYPAD BUTTON VALUES -----
const byte kOut   = 0xff;   // 11111111 Usual 1st byte from keypad
const byte k_ff   = 0xff;   // 11111111 Keypad CRC checksum 1?
const byte k_7f   = 0x7f;   // 01111111 Keypad CRC checksum 2?
// The following buttons data are in the 2nd byte
const byte one    = 0x82;   // 10000010
const byte two    = 0x85;   // 10000101
const byte three  = 0x87;   // 10000111
const byte four   = 0x88;   // 10001000
const byte five   = 0x8B;   // 10001011
const byte six    = 0x8d;   // 10001101
const byte seven  = 0x8e;   // 10001110
const byte eight  = 0x91;   // 10010001
const byte nine   = 0x93;   // 10010011
const byte aster  = 0x94;   // 10010100
const byte zero   = 0x80;   // 10000000
const byte pound  = 0x96;   // 10010110
const byte stay   = 0xd7;   // 11010111
const byte away   = 0xd8;   // 11011000
const byte chime  = 0xdd;   // 11011101
const byte reset  = 0xed;   // 11101101
const byte kExit  = 0xf0;   // 11110000
const byte lArrow = 0xfb;   // 11111011
const byte rArrow = 0xf7;   // 11110111
// -------
// The following buttons data are in the 1st byte, and these 
//   seem to be sent twice, with a panel response in between:
const byte fire   = 0xbb;   // 10111011 
const byte aux    = 0xdd;   // 11011101 
const byte panic  = 0xef;   // 11101110 
// --------

const byte MAX_BITS = 200;        // Max of 254
const int NEW_WORD_INTV = 1500;   // New word indicator interval in us (Microseconds)

String pBuild="", pWord="", oldPWord="";
String kBuild="", kWord="", oldKWord="";

const byte ARR_SIZE = 14;         // Max of 254           // NOT USED
byte pBytes[ARR_SIZE] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};    // NOT USED
byte kBytes[ARR_SIZE] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};    // NOT USED

unsigned long lastStatus = 0;
unsigned long lastData = 0;
// Volatile variables, modified within ISR
volatile unsigned long intervalTimer = 0;
volatile unsigned long clockChange = 0;
volatile unsigned long lastChange = 0;
volatile unsigned long lastRise = 0;
volatile unsigned long lastFall = 0;
volatile bool newWord = false;

bool newClient = false;              // Whether the client is new or not
bool streamData = false;             // Was the request to stream data? (/STREAM)

time_t t = now();                    // Initialize the time variable

char buf[100];
TextBuffer message(128);             // Initialize TextBuffer.h for print/client message
CRC32 crc32;                         // Initialize CRC // NOT USED

// Enter a MAC address and IP address for the controller below.
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
// Set a manual IP address in case DHCP Fails
IPAddress ip(192, 168, 1, 169);

EthernetServer server(80);
EthernetClient client;

void setup()
{
  pinMode(CLK,INPUT);
  pinMode(DTA_IN,INPUT);
  pinMode(DTA_OUT, OUTPUT);
  Serial.begin(115200);
  Serial.flush();
  Serial.println(F("DSC Powerseries 18XX"));
  Serial.println(F("Key Bus Monitor"));
  Serial.println(F("Initializing"));

  message.begin();                   // Begin the message buffer, allocate memory
 
  // Start the Ethernet connection and the server (Try to use DHCP):
  Serial.println(F("Trying to get an IP address using DHCP..."));
  if (Ethernet.begin(mac) == 0) {
    Serial.println(F("Trying to manually set IP address..."));
    // Initialize the ethernet device using manual IP address:
    if (1 == 0) {                   // This logic is "shut off", doesn't work with Ethernet client
      Serial.println(F("No ethernet connection"));
    }
    else {
      Ethernet.begin(mac, ip);
      server.begin();
    }
  }
  Serial.print(F("Server is at: "));
  Serial.println(Ethernet.localIP());
  Serial.println();

  // Attach interrupt on the CLK pin
  attachInterrupt(digitalPinToInterrupt(CLK), clkCalled, CHANGE);  
      // Moved to the last line in setup, otherwise may interrupt init message
      // Changed from RISING to CHANGE to read both panel and keypad data
}

// -----------------------------------------------------------------------------------------------------------------------
// -------------------------------------------------------  MAIN LOOP  ---------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

void loop()
{  
  // -------------- Check for a new connection --------------
  if (!client.connected()) {
    client = server.available();
    newClient = true;
    streamData = false;
  }
  if (client and newClient) {
    float connTimeout = millis() + 500;    // Set the timeout 500 ms from now
    while(true) {
      // If there is available data
      if (client.available()) {
        // Read the first two words of the request
        String request_type = client.readStringUntil(' ');  // Read till the first space
        String request = client.readStringUntil(' ');       // Read till the next space
        Serial.print(F("Request: "));
        Serial.print(request_type);
        Serial.print(" ");
        Serial.println(request);
        //client.flush();
        if (request == "/STREAM") {
          streamData = true;
          newClient = false;
          for (int i=0; i <= 30; i++){
            client.println(F("Empty --------------------------------------------------"));
          } 
        }
        else {
          streamData = false;
          client.stop();
          newClient = true;
        }
        break;
      }
      // If not, wait up to [connTimeout] ms for data
      if (millis() >= connTimeout) {
        Serial.println(F("Connection Timeout"));
        streamData = false;
        client.stop();
        newClient = true;
      }
      delay(5);
    }
  }
 
  // ----------------- Turn on/off LED ------------------
  if ((millis() - lastStatus) > 500)
    digitalWrite(LED_PIN,0);               // Turn LED OFF (no recent status command [0x05])
  else
    digitalWrite(LED_PIN,1);               // Turn LED ON  (recent status command [0x05])

  // --------------- Print No Data Message -------------- (FOR DEBUG PURPOSES)
  if ((millis() - lastData) > 20000) {
    // Print no data message if there is no new data in XX time (ms)
    Serial.println("--- No data for 20 seconds ---");  
    if (client.connected() and streamData) {
      client.println("--- No data for 20 seconds ---"); 
    }
    lastData = millis();          // Reset the timer
  }

  // ---------------- Get/process incoming data ----------------
  
  // If the clock (CLK) line takes less than 1500 us (1.5 ms) to change (1 clock period, 
  // high then low, is 1 ms), restart the loop and wait to process data until it remains
  // unchanged for > 1.5 ms, which indicates that a new word marker has begun.
  
  if (micros() < (lastChange + NEW_WORD_INTV)) return;
  
  // If it takes more than 1500 us (1.5 ms), the data word is complete, and it is now time 
  // to process the words.  The new word marker is about 15 ms long, which means we have 
  // about 13.5 ms left to process and output the panel and keypad words before the next 
  // words begin. 

  pWord = pBuild;                       // Save the complete panel raw data bytes sentence
  kWord = kBuild;                       // Save the complete keypad raw data bytes sentence
  pBuild = "";                          // Reset the raw data panel word being built
  kBuild = "";                          // Reset the raw data bytes keypad word being built
  int pCmd = decodePanel();
  int kCmd = decodeKeypad();

  if (pCmd) {
    // ------------ Write the panel message to buffer ------------
    message.clear();                      // Clear the message Buffer (this sets first byte to 0)
    message.write("[Panel]  ");
    pnlBinary(pWord);                     // Writes formatted raw data to the message buffer
    Serial.print(message.getBuffer());
    //printHex(pWord);                    // Print the raw data in Hex (WORKS!)

    message.clear();                      // Clear the message Buffer (this sets first byte to 0)
    message.write(formatTime(now()));     // Add the time stamp
    message.write(" ");
    if (String(pCmd,HEX).length() == 1)
      message.write("0");                 // Write a leading zero to a single digit HEX
    message.write(String(pCmd,HEX));
    message.write("(");
    message.write(pCmd);
    message.write("): ");
    message.writeln(msg);
  
    // ------------ Print the message ------------
    Serial.print(message.getBuffer());
    if (client.connected() and streamData) {
      client.write(message.getBuffer(), message.getSize());
    }
  }

  if (kCmd) {
    // ------------ Write the keypad message to buffer ------------
    message.clear();                      // Clear the message Buffer (this sets first byte to 0)
    message.write("[Keypad] ");
    kpdBinary(kWord);                     // Writes formatted raw data to the message buffer
    Serial.print(message.getBuffer());
  
    message.clear();                      // Clear the message Buffer (this sets first byte to 0)
    message.write(formatTime(now()));     // Add the time stamp
    message.write(" ");
    message.writeln(kMsg);

    // ------------ Print the message ------------
    Serial.print(message.getBuffer());
    if (client.connected() and streamData) {
      client.write(message.getBuffer(), message.getSize());
    }
  }
}

// -----------------------------------------------------------------------------------------------------------------------
// -------------------------------------------------------  FUNCTIONS  ---------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

void clkCalled()
{
  clockChange = micros();                 // Save the current clock change time
  intervalTimer = (clockChange - lastChange); // Determine interval since last clock change
  //if (intervalTimer > NEW_WORD_INTV) newWord = true;
  //else newWord = false;
  lastChange = clockChange;               // Re-save the current change time as last change time

  if (digitalRead(CLK)) {                 // If clock line is going HIGH, this is PANEL data
    lastRise = lastChange;                // Set the lastRise time
    if (pBuild.length() <= MAX_BITS) {    // Limit the string size to something manageable
      //delayMicroseconds(120);           // Delay for 120 us to get a valid data line read
      if (digitalRead(DTA_IN)) pBuild += "1"; else pBuild += "0";
    }
  }
  else {                                  // Otherwise, it's going LOW, this is KEYPAD data
    lastFall = lastChange;                // Set the lastFall time
    if (kBuild.length() <= MAX_BITS) {    // Limit the string size to something manageable 
      //delayMicroseconds(200);           // Delay for 300 us to get a valid data line read
      if (digitalRead(DTA_IN)) kBuild += "1"; else kBuild += "0";
    }
  }
}

bool pnlBinary(String &dataStr)
{
  int chkSum = 0x00;
  byte lastByte = 0x00;
  bool chkSumOk = false;
  
  if (dataStr.length() > 8) {
    // Formats the referenced string into bytes of binary data in the form:
    //  8 1 8 8 8 8 8 etc, and then prints each segment
    message.write(dataStr.substring(0,8));
    chkSum += binToInt(dataStr,0,8);
    message.write(" ");
    message.write(dataStr.substring(8,9));
    message.write(" ");
    int grps = (dataStr.length() - 9) / 8;
    for(int i=0;i<grps;i++) {
      message.write(dataStr.substring(9+(i*8),9+(i+1)*8));
      message.write(" ");
      if (i<(grps-1)) 
        chkSum += binToInt(dataStr,9+(i*8),8);
      else {
        String fullCS = String(chkSum, HEX);
        lastByte = binToInt(dataStr,9+(i*8),8);
        if (fullCS.substring((fullCS.length()-2),fullCS.length()) == 
            String(lastByte, HEX)) chkSumOk = true;
      }
    }
    if (dataStr.length() > ((grps*8)+9))
      message.write(dataStr.substring((grps*8)+9,dataStr.length()));
  }
  else {
    message.write(dataStr);
  }
  if (chkSumOk) message.write(" (OK)");
  message.writeln();
  return chkSumOk;
}

bool kpdBinary(String &dataStr)
{
  int chkSum = 0x00;
  byte lastByte = 0x00;
  bool chkSumOk = false;
  
  if (dataStr.length() > 8) {
    // Formats the referenced string into bytes of binary data in the form:
    //  8 8 8 8 8 8 etc, and then prints each segment
    int grps = dataStr.length() / 8;
    for(int i=0;i<grps;i++) {
      message.write(dataStr.substring(i*8,(i+1)*8));
      message.write(" ");
      /*
      if (i=1) 
        chkSum += binToInt(dataStr,i*8,8);
      else if (i=2) {
        String fullCS = String(chkSum, HEX);
        lastByte = binToInt(dataStr,i*8,8);
        if (fullCS.substring((fullCS.length()-2),fullCS.length()) == 
            String(lastByte, HEX)) chkSumOk = true;
      }
      */
    }
    if (dataStr.length() > (grps*8))
      message.write(dataStr.substring((grps*8),dataStr.length()));
  }
  else {
    message.write(dataStr);
  }
  if (chkSumOk) message.write(" (OK)");
  message.writeln();
  return chkSumOk;
}

unsigned int binToInt(String &dataStr, int offset, int dataLen)
{
  // Returns the value of the binary data in the String from "offset" to "dataLen" as an int
  int buf = 0;
  for(int j=0;j<dataLen;j++) {
    buf <<= 1;
    if (dataStr[offset+j] == '1') buf |= 1;
  }
  return buf;
}

String byteToBin(byte b)
{
  // Returns the 8 bit binary representation of byte "b" with leading zeros as a String
  int zeros = 8-String(b, BIN).length();
  String zStr = "";
  for (int i=0;i<zeros;i++) zStr += "0";
  return zStr + String(b, BIN);
}

void zeroArr(byte byteArr[])
{
  // Zeros an array of ARR_SIZE  bytes
  for (int i=0;i<ARR_SIZE;i++) byteArr[i] = 0;
}

String formatTime(time_t cTime)
{
  return digits(hour(cTime)) + ":" + digits(minute(cTime)) + ":" + digits(second(cTime)) + ", " +
         String(month(cTime)) + "/" + String(day(cTime)) + "/" + String(year(cTime));  //+ "/20" 
}

String digits(unsigned int val)
{
  // Take an unsigned int and convert to a string with appropriate leading zeros
  if (val < 10) return "0" + String(val);
  else return String(val);
}

static int decodePanel() 
{
  // ------------- Process the Panel Data Word ---------------
  int cmd = binToInt(pWord,0,8);        // Get the panel pCmd (data word type/command)
  
  if (pWord == oldPWord || cmd == 0x00) {
    // Skip this word if the data hasn't changed, or pCmd is empty (0x00)
    return 0;     // Return failure
  }
  else {     
    // This seems to be a valid word, try to process it  
    lastData = millis();                // Record the time (last data word was received)
    oldPWord = pWord;                   // This is a new/good word, save it
   
    String msg = "";
   
    // Interpret the data
    if (cmd == 0x05) 
    {
      lastStatus = millis();            // Record the time for LED logic
      msg += F("[Status] ");
      if (binToInt(pWord,16,1)) {
        msg += F("Ready");
      }
      else {
        msg += F("Not Ready");
      }
      if (binToInt(pWord,12,1)) msg += F(", Error");
      if (binToInt(pWord,13,1)) msg += F(", Bypass");
      if (binToInt(pWord,14,1)) msg += F(", Memory");
      if (binToInt(pWord,15,1)) msg += F(", Armed");
      if (binToInt(pWord,17,1)) msg += F(", Program");
      if (binToInt(pWord,29,1)) msg += F(", Power Fail");   // ??? - maybe 28 or 20?
    }
   
    if (cmd == 0xa5)
    {
      msg += F("[Info] ");
      int y3 = binToInt(pWord,9,4);
      int y4 = binToInt(pWord,13,4);
      int yy = (String(y3) + String(y4)).toInt();
      int mm = binToInt(pWord,19,4);
      int dd = binToInt(pWord,23,5);
      int HH = binToInt(pWord,28,5);
      int MM = binToInt(pWord,33,6);
      
      setTime(HH,MM,0,dd,mm,yy);
      if (timeStatus() == timeSet) {
        Serial.println("Time Synchronized");
        msg += "Time Sync ";
      }
      else {
        Serial.println("Time Sync Error");
        msg += "Time Sync Error ";
      }      

      int arm = binToInt(pWord,41,2);
      int master = binToInt(pWord,43,1);
      int user = binToInt(pWord,43,6); // 0-36
      if (arm == 0x02) {
        msg += F(", Armed");
        user = user - 0x19;
      }
      if (arm == 0x03) {
        msg += F(", Disarmed");
      }
      if (arm > 0) {
        if (master) msg += F(", Master Code"); 
        else msg += F(", User Code");
        user += 1; // shift to 1-32, 33, 34
        if (user > 34) user += 5; // convert to system code 40, 41, 42
        msg += " " + String(user);
      }
    }
    
    if (cmd == 0x27)
    {
      msg += F("[Zones A] ");
      int zones = binToInt(pWord,8+1+8+8+8+8,8);
      if (zones & 1) msg += "1";
      if (zones & 2) msg += "2";
      if (zones & 4) msg += "3";
      if (zones & 8) msg += "4";
      if (zones & 16) msg += "5";
      if (zones & 32) msg += "6";
      if (zones & 64) msg += "7";
      if (zones & 128) msg += "8";
      if (zones == 0) msg += "Ready ";
    }
    
    if (cmd == 0x2d)
    {
      msg += F("[Zones B] ");
      int zones = binToInt(pWord,8+1+8+8+8+8,8);
      if (zones & 1) msg += "9";
      if (zones & 2) msg += "10";
      if (zones & 4) msg += "11";
      if (zones & 8) msg += "12";
      if (zones & 16) msg += "13";
      if (zones & 32) msg += "14";
      if (zones & 64) msg += "15";
      if (zones & 128) msg += "16";
      if (zones == 0) msg += "Ready ";
    }
    
    if (cmd == 0x34)
    {
      msg += F("[Zones C] ");
      int zones = binToInt(pWord,8+1+8+8+8+8,8);
      if (zones & 1) msg += "17";
      if (zones & 2) msg += "18";
      if (zones & 4) msg += "19";
      if (zones & 8) msg += "20";
      if (zones & 16) msg += "21";
      if (zones & 32) msg += "22";
      if (zones & 64) msg += "23";
      if (zones & 128) msg += "24";
      if (zones == 0) msg += "Ready ";
    }
    
    if (cmd == 0x3e)
    {
      msg += F("[Zones D] ");
      int zones = binToInt(pWord,8+1+8+8+8+8,8);
      if (zones & 1) msg += "25";
      if (zones & 2) msg += "26";
      if (zones & 4) msg += "27";
      if (zones & 8) msg += "28";
      if (zones & 16) msg += "29";
      if (zones & 32) msg += "30";
      if (zones & 64) msg += "31";
      if (zones & 128) msg += "32";
      if (zones == 0) msg += "Ready ";
    }
    // --- The other 32 zones for a 1864 panel need to be added after this ---

    if (cmd == 0x11) {
      msg += F("[Keypad Query] ");
    }
    if (cmd == 0x0a) {
      msg += F("[Panel Program Mode] ");
    } 
    if (cmd == 0x5d) {
      msg += F("[Alarm Memory Group 1] ");
    } 
    if (cmd == 0x63) {
      msg += F("[Alarm Memory Group 2] ");
    } 
    if (cmd == 0x64) {
      msg += F("[Beep Command Group 1] ");
    } 
    if (cmd == 0x69) {
      msg += F("[Beep Command Group 2] ");
    } 
    if (cmd == 0x39) {
      msg += F("[Undefined command from panel] ");
    } 
    if (cmd == 0xb1) {
      msg += F("[Zone Configuration] ");
    }
  return cmd;     // Return success
  }
}

static int decodeKeypad() 
{
  // ------------- Process the Keypad Data Word ---------------
  int cmd = binToInt(pWord,0,8);      // Get the keypad pCmd (data word type/command)
  
  if (kWord.indexOf("0") == -1) {  
    // Skip this word if kWord is all 1's
    return 0;     // Return failure
  }
  else { 
    // This seems to be a valid word, try to process it
    lastData = millis();              // Record the time (last data word was received)
    oldKWord = kWord;                 // This is a new/good word, save it

    String kMsg = "";
    int kByte2 = binToInt(kWord,8,8); 
   
    // Interpret the data
    if (cmd == kOut) {
       
      if (kByte2 != 0xff)
        kMsg += "[Button] ";
      if (kByte2 == one)
        kMsg += "1";
      else if (kByte2 == two)
        kMsg += "2";
      else if (kByte2 == three)
        kMsg += "3";
      else if (kByte2 == four)
        kMsg += "4";
      else if (kByte2 == five)
        kMsg += "5";
      else if (kByte2 == six)
        kMsg += "6";
      else if (kByte2 == seven)
        kMsg += "7";
      else if (kByte2 == eight)
        kMsg += "8";
      else if (kByte2 == nine)
        kMsg += "9";
      else if (kByte2 == aster)
        kMsg += "*";
      else if (kByte2 == zero)
        kMsg += "0";
      else if (kByte2 == pound)
        kMsg += "#";
      else if (kByte2 == stay)
        kMsg += F("Stay");
      else if (kByte2 == away)
        kMsg += F("Away");
      else if (kByte2 == chime)
        kMsg += F("Chime");
      else if (kByte2 == reset)
        kMsg += F("Reset");
      else if (kByte2 == kExit)
        kMsg += F("Exit");
      else if (kByte2 == lArrow)  // These arrow commands don't work every time
        kMsg += F("<");
      else if (kByte2 == rArrow)  // They often reverse for unknown reasons
        kMsg += F(">");
      else if (kByte2 == 0xff)
        kMsg += F("[Keypad Response]");
      else {
        kMsg += "0x" + String(kByte2, HEX) + " (Unknown)";
      }
    }

    if (cmd == fire)
      kMsg += F("[Button] Fire");
    if (cmd == aux)
      kMsg += F("[Button] Auxillary");
    if (cmd == panic)
      kMsg += F("[Button] Panic");
    
    return cmd;     // Return success
  }
}
// -----------------------------------------------------------------------------------------------------------------------
// ----------------------------------------------------------  END  ------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------
