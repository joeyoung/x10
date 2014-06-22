//testX10rx

// created: Feb 6, 2014  G. D. (Joe) Young <jyoung@islandnet.com>
//


// include the X10 library files:
#include <x10.h>

const char hCode[ ] = { 'M', 'E', 'C', 'K', 'O', 'G', 'A', 'I', 
                  'N', 'F', 'D', 'L', 'P', 'H', 'B', 'J' };

const char *kCode[ ] = { "13", "allUoff", "5", "allLon",  "3", "on",   "11", "off",
       "15", "dim", "7", "bright", "1", "allLoff", "9", "extC",
                  "14", "hail",    "6", "hailAck", "4", "psDim","12", "psDimH",
       "16", "extD", "8", "stOn",   "2", "stOff",   "10", "stReq" };

const int rxPin = 3;    // data receive pin
const int txPin = 4;    // data transmit pin
const int zcPin = 2;    // zero crossing pin

unsigned int cmd;

void print_cmd( unsigned int cmd ){
  if( (cmd & 0x8000) == 0x8000 ) {     // read error
    Serial.print( "read err: " );
    Serial.println( (cmd & 0x7fff), HEX );
  } else {
    Serial.print( hCode[(cmd&0b111100000)>>5] );
    Serial.print( kCode[cmd&0b11111] );
    Serial.println( " " );
//    Serial.println( cmd&0b11111, HEX );
  } // if error
} // print command


void setup( ) {
  Serial.begin( 9600 );
  x10.begin( rxPin, txPin, zcPin );
  cmd = x10.read( );
  Serial.println( cmd, HEX );
  print_cmd( cmd );
} 


void loop( ) {
  int cmd = x10.read( );
  print_cmd( cmd );
}
