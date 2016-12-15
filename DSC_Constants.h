/* DSC_Constants.h
 * Part of DSC Library 
 * See COPYRIGHT.txt and LICENSE.txt for more information.
 *
 * It contains all of the constants used by the DSC class.
 * They have to be declared global in scope because they may be accessed by
 * the ISR and you cannot pass parameters nor objects to an ISR routine.
 * 
 * In general, applications would not include this file. 
 */
 
#ifndef DSC_Constants_h
#define DSC_Constants_h

// ----- Word Size Constants -----
  /*
   * The following constants may be adjusted however the memory capability of
   * the specific board being used must be taken into account.
  */
const byte MAX_BITS = 164;          // The length at which to overflow (max 255)
const byte WORD_BITS = 108;         // The expected length of a word (max 255)
const byte MSG_BITS = 80;           // The expected length of a message (max 255)
const byte ARR_SIZE = 12;           // (max 255)   // NOT USED

// ----- Word Timing Constants -----
const int NEW_WORD_INTV = 5200;     // New word indicator interval in us (Micros)
const int NO_DATA_TIMEOUT = 20000;  // Time to flag indicating no data (Millis)

// ------ HEX LOOK-UP ARRAY ------
const char hex[] = "0123456789abcdef";  // HEX alphanumerics look-up array

// ----- KEYPAD BUTTON VALUES -----
const byte kOut   = 0xff;   // 11111111 (dec: 255) Usual 1st byte from keypad
const byte k_ff   = 0xff;   // 11111111 (dec: 255) Keypad CRC checksum 1?
const byte k_7f   = 0x7f;   // 01111111 (dec: 127) Keypad CRC checksum 2?
// The following buttons data are in the 2nd byte:
const byte one    = 0x82;   // 10000010 (dec: 130) 
const byte two    = 0x85;   // 10000101 (dec: 133) 
const byte three  = 0x87;   // 10000111 (dec: 135) 
const byte four   = 0x88;   // 10001000 (dec: 136) 
const byte five   = 0x8b;   // 10001011 (dec: 139) 
const byte six    = 0x8d;   // 10001101 (dec: 141) 
const byte seven  = 0x8e;   // 10001110 (dec: 142) 
const byte eight  = 0x91;   // 10010001 (dec: 145) 
const byte nine   = 0x93;   // 10010011 (dec: 147) 
const byte aster  = 0x94;   // 10010100 (dec: 148) 
const byte zero   = 0x80;   // 10000000 (dec: 128) 
const byte pound  = 0x96;   // 10010110 (dec: 150) 
const byte stay   = 0xd7;   // 11010111 (dec: 215) 
const byte away   = 0xd8;   // 11011000 (dec: 216) 
const byte chime  = 0xdd;   // 11011101 (dec: 221) 
const byte reset  = 0xed;   // 11101101 (dec: 237) 
const byte kExit  = 0xf0;   // 11110000 (dec: 270) 
const byte lArrow = 0xfb;   // 11111011 (dec: 251) 
const byte rArrow = 0xf7;   // 11110111 (dec: 247) 
// The following button's data are in the 1st byte, and these 
//   seem to be sent twice, with a panel response in between:
const byte fire   = 0xbb;   // 10111011 (dec: 187) 
const byte aux    = 0xdd;   // 11011101 (dec: 221) 
const byte panic  = 0xee;   // 11101110 (dec: 238) 

#endif
