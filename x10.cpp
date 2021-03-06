/*
  x10.cpp - X10 transmission library for Arduino version 0.4
  
 Copyright (c) 2007 by Tom Igoe (tom.igoe@gmail.com)

This file is free software; you can redistribute it and/or modify
it under the terms of either the GNU General Public License version 2
or the GNU Lesser General Public License version 2.1, both as
published by the Free Software Foundation.

  
  Original library								(0.1) 
  Timing bug fixes								(0.2)
  #include bug fixes for 0012					(0.3)
  Refactored version following Wire lib API		(0.4)
    
  Zero crossing algorithms borrowed from David Mellis' shiftOut command
  for Arduino.
  
  The circuits can be found at 
 
http://www.arduino.cc/en/Tutorial/x10

  Feb 12, 2014
  simple read( ) implementation - G. D. (Joe) Young <jyoung@islandnet.com>

  Feb 18/14 - coordinate read( ) and write( ) sharing of waitForZeroCross( )
 
 
 */

#include <Arduino.h>
#include "x10.h"
#include "x10constants.h"

uint8_t x10Class::zeroCrossingPin;// = 2;	// AC zero crossing pin
uint8_t x10Class::rxPin;// = 3;			// data in pin
uint8_t x10Class::txPin;// = 4;			// data out pin

uint8_t x10Class::houseCode = 0;		// house code
uint8_t x10Class::transmitting = 0;		// whether you're transmitting or not
uint8_t x10Class::receiving = 0;		// true if receiving underway


/*
 Constructor.
 
 Sets the pins and sets their I/O modes.
 
 */
 
x10Class::x10Class() {}


void x10Class::begin(int _rxPin, int _txPin, int _zcPin)
{
  // initialize pin numbers:
  txPin = _txPin;        		
  rxPin = _rxPin;        	
  zeroCrossingPin = _zcPin;			
  houseCode = 0;				  
  transmitting = 0;
  receiving = 0;			

  // Set I/O modes:
  pinMode(rxPin, INPUT);
  pinMode(txPin, OUTPUT);
  pinMode (zeroCrossingPin, INPUT);
//  atzc = false;
}

void x10Class::beginTransmission(uint8_t data)
{
	houseCode = data;
	transmitting = 1;
}

void x10Class::beginTransmission(int data)
{
	 beginTransmission((uint8_t)data);

}

void x10Class::endTransmission(void)
{
   // indicate that we are done transmitting
  transmitting = 0;
}

    
size_t x10Class::write(uint8_t data)
{
	if (transmitting) {
		sendCommand(houseCode, data);
	}
}
size_t x10Class::write(const char * data)
{
 write((uint8_t*)data, strlen(data));
}

size_t x10Class::write(const uint8_t *data, size_t quantity)
{
    for(size_t i = 0; i < quantity; ++i){
      write(data[i]);
    }
}

/*
	methods inherited from Stream but not implemented yet.
*/

int x10Class::available(void)
{
}

int x10Class::read(void)
{
	if( transmitting ) return( (int)WAIT_TX );	//back out if Tx underway
	receiving = true;
	return( (int)receiveCommand( ) );
}

int x10Class::peek(void)
{
}

void x10Class::flush(void)
{
}

/*
	Writes an X10 command out to the X10 modem
*/
void x10Class::sendCommand(byte houseCode, byte numberCode) {
 
   byte startCode = 0b1110; 		// every X10 command starts with this
   
	// send the three parts of the command:
	sendBits(startCode, 4, true);	
	sendBits(houseCode, 4, false);
	sendBits(numberCode, 5, false);

  	// if this isn't a bright or dim command, it should be followed by
  	// a delay of 3 power cycles (or 6 zero crossings):
  	if ((numberCode != BRIGHT) && (numberCode != DIM)) {
  		waitForZeroCross(zeroCrossingPin, 6);
   	}
}


/*
	Writes a sequence of bits out.  If the sequence is not a start code,
	it repeats the bits, inverting them.
*/

void x10Class::sendBits(byte cmd, byte numBits, byte isStartCode) {
  	byte thisBit;	// byte for shifting bits
  
	// iterate the number of bits to be shifted:
	for(int i=1; i<=numBits; i++) {
		// wait for a zero crossing change
		waitForZeroCross(zeroCrossingPin, 1);
		// shift off the last bit of the command:
		thisBit = cmd & (1 << (numBits - i));
		
		// repeat once for each phase:
		for (int phase = 0; phase < 3; phase++) {
			// set the data Pin:
			digitalWrite(txPin, thisBit);
			delayMicroseconds(BIT_LENGTH);
			// clear the data pin:
			digitalWrite(txPin, LOW);
			delayMicroseconds(BIT_DELAY);
		}
		
		// if this command is a start code, don't
		// send its complement.  Otherwise do:
		if(!isStartCode) {
			// wait for zero crossing:
			waitForZeroCross(zeroCrossingPin, 1);
			for (int phase = 0; phase < 3; phase++) {
				// set the data pin:
				digitalWrite(txPin, !thisBit);
				delayMicroseconds(BIT_LENGTH);
				// clear the data pin:
				digitalWrite(txPin, LOW);
				delayMicroseconds(BIT_DELAY);
			}
		}
	}
}

/*
	TO DO: receiveBits and receiveCommand to parallel the above
	Started Feb 6, 2014  G. D. (Joe) Young <jyoung@islandnet.com>
	Feb 9/14 - received bits are complements of transmited; need to detect
	    beginning of start sequence, then gather 4 bits and check validity
	Feb 12/14 - implemented beginning detect with waitForZeroCross, read pin.
	Feb 19/14 - add non-blocking return if no start detected
*/

#if 0 
void x10Class::setatzc( bool _atzc ) {
	atzc = _atzc;
} // setatzc( )
#endif

unsigned int x10Class::receiveCommand( ) {

	unsigned int command = 0;
	byte bitsleft = 3;

	waitForZeroCross( zeroCrossingPin, 1 );	// sync with zero crossing
	delayMicroseconds( BIT_LENGTH/2 );		// sample near bit centre
	if( digitalRead( rxPin ) == HIGH ) {
		receiving = false;
		return( NO_CMD );					// give tx a chance if no start bit
	} else {
		byte startCode = receiveBits( bitsleft, true ); 	// exit wait after rx first start bit
		if( startCode == 0b110 ) {		// good start
			byte house = receiveBits( 4, false );
			if( house < 0x80 ) {		// good house code
				command = house<<5;
			} else {
				command = 0x8001;		// setup to return error
				return command;
			} // if good house code
			byte number = receiveBits( 5, false );
			if( number < 0x80 ) {		// good number code
				command |= number;
			} else {
				command = 0x8002;
				return command;			// error abort
			} // if good number code
		} else {						// bad start
//		command = 0x8000;
			command = startCode | 0x8000;
		} // if good start code
	} // if no start bit found on entry
	return command;

} /* receiveCommand( ) */


byte x10Class::receiveBits( byte numBits, byte isStartCode ) {
	byte input = 0;
	byte thisBit;
	for( byte i=0; i<numBits; i++ ) {
		waitForZeroCross( zeroCrossingPin, 1 );
		delayMicroseconds( BIT_LENGTH/2 );		// sample near bit centre
		thisBit = (~digitalRead( rxPin )) & 0x01;
		if( !isStartCode ) {				// check for complement
			waitForZeroCross( zeroCrossingPin, 1 );
			delayMicroseconds( BIT_LENGTH/2 );		// sample near bit centre
			if( thisBit == digitalRead( rxPin ) & 0x01 ) {
				input = (input<<1) | thisBit;
			} else {
				input |= 0x80;
				return input;		// decoding error abort
			} // if complement matches
		} else { 
			input = (input<<1) | thisBit;
		} // if not startCode
	} // for numBits

	return input;

} /* receiveBits( ) */


#if 0
void x10Class::waitzc( ) {
	waitForZeroCross( zeroCrossingPin, 1 );
} // waitzc() - access function for sketch waiting for zc (not needed)
#endif

/*
  waits for a the zero crossing pin to cross zero

*/
void x10Class::waitForZeroCross(int pin, int howManyTimes) {
	unsigned long cycleTime = 0;
	
  	// cache the port and bit of the pin in order to speed up the
  	// pulse width measuring loop and achieve finer resolution.  calling
  	// digitalRead() instead yields much coarser resolution.
  	uint8_t bit = digitalPinToBitMask(pin);
  	uint8_t port = digitalPinToPort(pin);

  	for (int i = 0; i < howManyTimes; i++) {
		// wait for pin to change:
    	if((*portInputRegister(port) & bit))
    	 	while((*portInputRegister(port) & bit)) 
        		cycleTime++;
    	else
      		while(!(*portInputRegister(port) & bit)) 
        		cycleTime++;
  		}
}

// pre-instantiate class:
x10Class x10 = x10Class();
