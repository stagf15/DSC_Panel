// DSC_18XX Arduino Interface - Simple Example
//
// - Demonstrates the use of the DSC library, prints the raw binary words and 
//   minimally formats and then prints the decoded panel message
//
// Sketch to decode the keybus protocol on DSC PowerSeries 1816, 1832 and 1864 panels
//   -- Use the schematic at https://github.com/emcniece/Arduino-Keybus to connect the
//      keybus lines to the arduino via voltage divider circuits.  Don't forget to 
//      connect the Keybus Ground to Arduino Ground (not depicted on the circuit)! You
//      can also power your arduino from the keybus (+12 VDC, positive), depending on the 
//      the type arduino board you have.
//
//

#include <DSC.h>

DSC dsc;            // Initialize DSC.h library as "dsc"

// --------------------------------------------------------------------------------------------------------
// -----------------------------------------------  SETUP  ------------------------------------------------
// --------------------------------------------------------------------------------------------------------

void setup()
{ 
  Serial.begin(115200);
  Serial.flush();
  Serial.println(F("DSC Powerseries 18XX"));
  Serial.println(F("Key Bus Interface"));
  Serial.println(F("Initializing"));
 
  dsc.setCLK(3);    // Sets the clock pin to 3 (example, this is also the default)
                    // setDTA_IN( ), setDTA_OUT( ) and setLED( ) can also be called
  dsc.begin();      // Start the dsc library (Sets the pin modes)
}

// --------------------------------------------------------------------------------------------------------
// ---------------------------------------------  MAIN LOOP  ----------------------------------------------
// --------------------------------------------------------------------------------------------------------

void loop()
{  
  // --------------- Print No Data Message -------------- (FOR DEBUG PURPOSES)
  if (dsc.timeout()) {
    // Print no data message if there is DSC library shows timout
    Serial.println(F("--- No data ---"));  
  }

  // ---------------- Get/process incoming data ----------------
  if (dsc.process() < 1) return;

  if (dsc.get_pCmd()) {
    // ------------ Print the Binary Panel Word ------------
    Serial.println(dsc.get_pnlRaw());

    // ------------ Print the Formatted Panel Word ------------
    Serial.println(dsc.get_pnlFormat());

    // ------------ Print the decoded Panel Message ------------
    Serial.print("---> ");
    if (String(dsc.get_pCmd(),HEX).length() == 1)
      Serial.print("0");                  // Write a leading zero to a single digit HEX
    Serial.print(String(dsc.get_pCmd(),HEX));
    Serial.print("(");
    Serial.print(dsc.get_pCmd());
    Serial.print("): ");
    Serial.println(dsc.get_pMsg());
  }

  if (dsc.get_kCmd()) {
    // ------------ Print the Binary Keypad Word ------------
    Serial.println(dsc.get_kpdRaw());    

    // ------------ Print the Formatted Keypad Word ------------
    Serial.println(dsc.get_kpdFormat());

    // ------------ Print the decoded Keypad Message ------------
    Serial.print("---> ");
    if (String(dsc.get_kCmd(),HEX).length() == 1)
      Serial.print("0");                  // Write a leading zero to a single digit HEX
    Serial.print(String(dsc.get_kCmd(),HEX));
    Serial.print("(");
    Serial.print(dsc.get_kCmd());
    Serial.print("): ");
    Serial.println(dsc.get_kMsg());
  }
}

// --------------------------------------------------------------------------------------------------------
// ---------------------------------------------  FUNCTIONS  ----------------------------------------------
// --------------------------------------------------------------------------------------------------------

// None

// --------------------------------------------------------------------------------------------------------
// ------------------------------------------------  END  -------------------------------------------------
// --------------------------------------------------------------------------------------------------------
