// clock display with x10 monitoring

// created: Oct 12, 2013 G. D. (Joe) Young <jyoung@islandnet.com>

// revised: Oct 15 - sync read to clock chip's seconds
//          Oct 17 - calendar display
//          Oct 18 - put date each minute, temp display
//          Oct 20 - rearrange display
//          Jan 23/14 - shrPort I2C library and modified LiquidCrystal_I2C
//                      (LiquidCrystal_shr) library. Called clockDispshr
//          Feb 14 - Add X10 receive functions - using loop zero-cross
//                   detection of first start bit
//          Feb 20 - use non-blocking read. x10clock2

#include <Wire.h> 
#include <LiquidCrystal_shr.h>
#include <shrPort.h>
#include <x10.h>

// x10 house code definitions - numerical order
const char hCode[ ] = { 
  'M', 'E', 'C', 'K', 'O', 'G', 'A', 'I', 
  'N', 'F', 'D', 'L', 'P', 'H', 'B', 'J' };

// x10 key code definitions - numerical order
const char *kCode[ ] = { 
  "13", "allUoff", "5", "allLon",  "3", "on",   "11", "off",
  "15", "dim", "7", "bright", "1", "allLoff", "9", "extC",
  "14", "hail",    "6", "hailAck", "4", "psDim","12", "psDimH",
  "16", "extD", "8", "stOn",   "2", "stOff",   "10", "stReq" };


const int rxPin = 3;    // data receive pin
const int txPin = 4;    // data transmit pin
const int zcPin = 2;    // zero crossing pin

unsigned int cmd;


#define LCD_ADR 0x20  // set the LCD address to 0x20
#define LCD_CHRS 16   // for a 16 chars ...
#define LCD_LINS 2    //   and 2 line display

shrPort port0( LCD_ADR, PCF8574 );
LiquidCrystal_shr lcd( port0, LCD_CHRS, LCD_LINS );


const char *ver_str = "Clk 0.8";
const byte ixyr = 0x06;
const byte ixmo = 0x05;
const byte ixdom = 0x04;
const byte ixdow =0x03;
const char days[7][4] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
const char months[12][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                             "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

unsigned long sectick;
byte seconds = 0;
byte clockData[19];    // array to hold info from clock chip
byte lastdow;          // for logging change of date
boolean lstate = false;

void setup()
{
//  Wire.begin( );
  Serial.begin( 9600 );
  x10.begin( rxPin, txPin, zcPin );
  lcd.init();        // initialize the lcd, wire, and shrPort libs
  lcd.backlight();
  for( int i = 0; i<19; i++ ) clockData[i] = 0;
  byte errCode = getClock( clockData );
  if( errCode == 0 ) {
    seconds = clockData[0];          // sync up with clock chip
    getTime( clockData );
    while( seconds == clockData[0] ) {
      getTime( clockData );
    } // wait for clock chip to change
  } // if clock read is ok
  putTime( clockData );
  putDate( clockData );
  sectick = millis( ) + 1000L;
  Serial.print( "Start at " );
  serTime( clockData );
  serDate( clockData );
  Serial.println( "" );
  lastdow = clockData[ixdow];
}

void loop()
{

  cmd = x10.read( );
//  if( (cmd & 0x8000) != 0x8000 ) {
  if( cmd != NO_CMD ) {
    serTime( clockData );
    print_cmd( cmd );
  }

//#if 0
  if( !lstate && ((clockData[1]&0x01) == 0) ) {
    lstate = true;
    x10.beginTransmission( G );
    x10.write( UNIT_2 );
    x10.write( ON );
    x10.endTransmission( );
    print_tx( G, UNIT_2, ON );
  }
  if( lstate && ((clockData[1]&0x01) == 1) ) {
    lstate = false;
    x10.beginTransmission( G );
    x10.write( UNIT_2 );
    x10.write( OFF );
    x10.endTransmission( );
    print_tx( G, UNIT_2, OFF );
  }
//#endif  

  if( sectick < millis( ) ) {
    sectick += 1000L;
    clockData[0]++;
    if( ( clockData[0] & 0x0f ) > 9 ) {
      clockData[0] &= 0xf0;
      clockData[0] += 0x10;
    }
    if( clockData[0] == 0x60 ) {
      clockData[0] = 0;
      getTime( clockData );
      putDate( clockData );
      getTemp( clockData+0x11 );
      putTemp( clockData+0x11 );
    } // each minute read clock
    if( clockData[ixdow] != lastdow ) {
      serDate( clockData );      // note day change in serial log
      Serial.println( "" );
      lastdow = clockData[ixdow]; //reset to current day
    } // new day check
    putTime( clockData );
  } // counting seconds

} // loop


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


void print_tx( byte hc, byte kc, byte kc2 ) {
  serTime( clockData );
  Serial.print( "Tx:" );
  Serial.print( hCode[hc] );
  Serial.print( kCode[kc] );
  Serial.println( kCode[kc2] );  
} // print_tx


#define RTC_ADR 0x68
#define RTC_RD_ERR 1
#define RTC_TM_RD_ERR 2

byte getClock( byte *regs ) {
  Wire.beginTransmission( RTC_ADR ); // start write to slave
  Wire.write( (uint8_t) 0x00 );    // set adr pointer to seconds
  Wire.endTransmission();
  byte errcode = Wire.requestFrom( RTC_ADR, 19 ); // request all
  if( errcode == 19 ) {
    for( byte ix=0; ix<19; ix++ ) {
      *(regs+ix) = Wire.read( );
    } // for received bytes
    return 0;
  } else {
    return RTC_RD_ERR;
  } // if RTC responded with 19 bytes
} // getClock( )

byte getTime( byte *regs ) {
  Wire.beginTransmission( RTC_ADR ); // start write to slave
  Wire.write( (uint8_t)0x00 );     // set adr pointer to seconds
  Wire.endTransmission();
  byte errcode = Wire.requestFrom( RTC_ADR, 7 ); // request time
  if( errcode == 7 ) {
    for( byte ix=0; ix<7; ix++ ) {
      *(regs+ix) = Wire.read( );
    } // for received bytes
    return 0;
  } else {
    return RTC_TM_RD_ERR;
  } // if RTC responded with 7 bytes
} // getTime( )

byte getTemp( byte *tregs ) {
 Wire.beginTransmission( RTC_ADR );
 Wire.write( (uint8_t)0x11 );       // set adr pointer to temp regs
 Wire.endTransmission( );
 byte errcode = Wire.requestFrom( RTC_ADR, 2 ); // request temp pair
 if( errcode == 2 ) {
   for( byte ix=0; ix<2; ix++ ) {
     *(tregs+ix) = Wire.read( );
   } // for received bytes
 } else {
   return RTC_RD_ERR;
 } // if RTC responded with 2 bytes
} // getTemp( ) 

const byte ixsec = 0x00;
const byte ixmin = 0x01;
const byte ixhr = 0x02;

void putTime( byte *regs ) {
  lcd.setCursor( 0, 1 );
  lcd.write( ( ( regs[ixhr] & 0xf0 )>>4 ) + 0x30 );
  lcd.write( ( regs[ixhr] & 0x0f ) + 0x30 );
  lcd.write( ':' );
  lcd.write( ( ( regs[ixmin] & 0xf0 )>>4 ) + 0x30 );
  lcd.write( ( regs[ixmin] & 0x0f ) + 0x30 );
  lcd.write( ':' );
  lcd.write( ( ( regs[ixsec] & 0xf0 )>>4 ) + 0x30 );
  lcd.write( ( regs[ixsec] & 0x0f ) + 0x30 );
} // putTime


// put time to serial output
void serTime( byte *regs ) {
  Serial.print( ( ( regs[ixhr] & 0xf0 )>>4 ), HEX );
  Serial.print( ( regs[ixhr] & 0x0f ), HEX );
  Serial.print( ':' );
  Serial.print( ( ( regs[ixmin] & 0xf0 )>>4 ), HEX );
  Serial.print( ( regs[ixmin] & 0x0f ), HEX );
  Serial.print( ':' );
  Serial.print( ( ( regs[ixsec] & 0xf0 )>>4 ), HEX );
  Serial.print( ( regs[ixsec] & 0x0f ), HEX );
  Serial.print( " " );
} // putTime


void putDate( byte *regs ) {
  lcd.setCursor( 0, 0 );
  lcd.print( days[regs[ixdow]-1] );
  lcd.write( ' ' );
  byte bixmo = ( ((regs[ixmo]&0x10)>>4)*10 ) + ( regs[ixmo]&0x07 );
  lcd.print( months[bixmo-1] );
  lcd.write( ' ' );
  lcd.write( ( ( regs[ixdom]&0x30 )>>4 ) + 0x30 );
  lcd.write( ( ( regs[ixdom]&0x0f ) + 0x30 ) );
  lcd.write( ' ' );
  if( regs[ixyr] < 0x80 ) {
    lcd.print( "20" );
  } else {
    lcd.print( "19" );      // century kludge
  }
  lcd.write( ( ( regs[ixyr] & 0xf0 )>>4 ) + 0x30 );
  lcd.write( ( ( regs[ixyr] & 0x0f ) + 0x30 ) );
  lcd.setCursor( 9, 1 );
  lcd.print( ver_str );
} // putDate

// put date to serial output
void serDate( byte *regs ) {
  Serial.print( " " );
  Serial.print( days[regs[ixdow]-1] );
  Serial.print( " " );
  byte bixmo = ( ((regs[ixmo]&0x10)>>4)*10 ) + ( regs[ixmo]&0x07 );
  Serial.print( months[bixmo-1] );
  Serial.print( " " );
//  Serial.print( ( ( regs[ixdom]&0x30 )>>4 ), DEC  );
  Serial.print( regs[ixdom], HEX );
  Serial.print( " " );
  if( regs[ixyr] < 0x80 ) {
    Serial.print( "20" );
  } else {
    Serial.print( "19" );      // century kludge
  }
//  Serial.print( ( ( regs[ixyr] & 0xf0 )>>4 ), DEC  );
  Serial.print( regs[ixyr], HEX );
} // serDate


void putTemp( byte *tregs ) {
  lcd.setCursor( 9, 1 );
  byte chrs = lcd.print( tregs[0], DEC );      //msb of temp
  chrs += lcd.write( '.' );
  chrs += lcd.print( 25*(tregs[1]>>6), DEC );  //fractional temp, .25 degree resolutn
  chrs += lcd.write( 0xdf );
  while( chrs < 7 ) {
	lcd.write( ' ' );
	chrs++;
  } // fill out with spaces
} // putTemp( )


