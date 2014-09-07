/*
Robot version 2
Simpler. ATtiny85, 2 motors, LED, two sensors

Sensor stuff: 
	both white	:	no prior	:	random walk
				:	prior		:	turn in direction of last black
	
	one white	:	ideal situation, move straight
	
	both black	:	no prior	:	random walk
				:	prior		:	turn in direction of last white

This code was written by Eric Heisler (shlonkin)
It is in the public domain, so do what you will with it.
*/
#include "Arduino.h"

// Here are some parameters that you need to adjust for your setup
// Base speeds are the motor pwm values. Adjust them for desired motor speeds.
#define baseLeftSpeed 17
#define baseRightSpeed 17

// This determines sensitivity in detecting black and white
// measurment is considered white if it is > whiteValue*n/4
// where n is the value below. n should satisfy 0<n<4
// A reasonable value is 3. Fractions are acceptable.
#define leftSensitivity 3
#define rightSensitivity 3

// the pin definitions
#define lmotorpin 1 // PB1 pin 6
#define rmotorpin 0 // PB0 pin 5
#define lsensepin 3 //ADC3 pin 2
#define rsensepin 1 //ADC1 pin 7
#define ledpin 4 //PB4 pin 3

// some behavioral numbers
// these are milisecond values
#define steplength 300
#define smallturn 200
#define bigturn 500
#define memtime 1000

// variables
uint8_t lspd, rspd; // motor speeds
uint16_t lsenseval, rsenseval, lwhiteval, rwhiteval; // sensor values

void followEdge();
void move(uint8_t lspeed, uint8_t rspeed);
void stop();
void senseInit();
void flashLED(uint8_t flashes);

// just for convenience and simplicity (HIGH is off)
#define ledoff PORTB |= 0b00010000
#define ledon PORTB &= 0b11101111

void setup(){
	// setup pins
	pinMode(lmotorpin, OUTPUT);
	pinMode(rmotorpin, OUTPUT);
	pinMode(2, INPUT); // these are the sensor pins, but the analog
	pinMode(3, INPUT); // and digital functions use different numbers
	ledoff;
	pinMode(ledpin, OUTPUT);
	analogWrite(lmotorpin, 0);
	analogWrite(rmotorpin, 0);
	
	lspd = baseLeftSpeed;
	rspd = baseRightSpeed;
	
	// give a few second pause to set the thing on a white surface
	// the LED will flash during this time
	lsenseval = 6;
	while(lsenseval){
		lsenseval--;
		flashLED(1);
		delay(989);
	}
	flashLED(4);
	delay(500);
	senseInit();
}

void loop(){
	// followEdge() contains an infinite loop, so this loop really isn't necessary
	followEdge();
}

void followEdge(){
	// now look for an edge
	uint8_t lastMove = 1; //0=straight, 1=left, 2=right
	unsigned long moveEndTime = 0; // the millis at which to stop
	unsigned long randomBits = micros(); // for a random walk
	
	unsigned long prior = 0; // after edge encounter set to millis + memtime
	uint8_t priorDir = 0; //0=left, 1=right, 2=both
	uint8_t lastSense = 1; //0=edge, 1=both white, 2=both black
	uint8_t i = 0; // iterator
	
	while(true){
		// only read sensors about once every 20ms
		// it can be done faster, but this makes motion smoother
		delay(18);
		// read the value 4 times and average
		ledon;
		delay(1);
		lsenseval = 0;
		rsenseval = 0;
		for(i=0; i<4; i++){
			lsenseval += analogRead(lsensepin);
			rsenseval += analogRead(rsensepin);
		}
		// don't divide by 4 because it is used below
		ledoff;
		
		// refill the random bits if needed
		if(randomBits == 0){ randomBits = micros(); }
		
		// Here is the important part
		// There are four possible states: both white, both black, one of each
		// The behavior will depend on current and previous states
		if((lsenseval > lwhiteval*leftSensitivity) && (rsenseval > rwhiteval*rightSensitivity)){
			// both white - if prior turn toward black, else random walk
			if(lastSense == 2 || millis() < prior){
				// turn toward last black or left
				if(priorDir == 0){
					moveEndTime = millis()+smallturn;
					move(0, rspd); // turn left
					lastMove = 1;
				}else if(priorDir == 1){
					moveEndTime = millis()+smallturn;
					move(lspd, 0); // turn right
					lastMove = 2;
				}else{
					moveEndTime = millis()+bigturn;
					move(0, rspd); // turn left a lot
					lastMove = 1;
				}
			}else{
				// random walk
				if(millis() < moveEndTime){
					// just continue moving
				}else{
					if(lastMove){
						moveEndTime = millis()+steplength;
						move(lspd, rspd); // go straight
						lastMove = 0;
					}else{
						if(randomBits & 1){
							moveEndTime = millis()+smallturn;
							move(0, rspd); // turn left
							lastMove = 1;
						}else{
							moveEndTime = millis()+smallturn;
							move(lspd, 0); // turn right
							lastMove = 2;
						}
						randomBits >>= 1;
					}
				}
			}
			lastSense = 1;
			
		}else if((lsenseval > lwhiteval*leftSensitivity) || (rsenseval > rwhiteval*rightSensitivity)){
			// one white, one black - this is the edge
			// just go straight
			moveEndTime = millis()+steplength;
			move(lspd, rspd); // go straight
			lastMove = 0;
			lastSense = 0;
			prior = millis()+memtime;
			if(lsenseval > lwhiteval*leftSensitivity){
				// the right one is black
				priorDir = 1;
			}else{
				// the left one is black
				priorDir = 0;
			}
			
		}else{
			// both black - if prior turn toward white, else random walk
			if(lastSense == 1 || millis() < prior){
				// turn toward last white or left
				if(priorDir == 0){
					moveEndTime = millis()+smallturn;
					move(lspd, 0); // turn right
					lastMove = 2;
				}else if(priorDir == 1){
					moveEndTime = millis()+smallturn;
					move(0, rspd); // turn left
					lastMove = 1;
				}else{
					moveEndTime = millis()+bigturn;
					move(lspd, 0); // turn right a lot
					lastMove = 2;
				}
			}else{
				// random walk
				if(millis() < moveEndTime){
					// just continue moving
				}else{
					if(lastMove){
						moveEndTime = millis()+steplength;
						move(lspd, rspd); // go straight
						lastMove = 0;
					}else{
						if(randomBits & 1){
							moveEndTime = millis()+smallturn;
							move(0, rspd); // turn left
							lastMove = 1;
						}else{
							moveEndTime = millis()+smallturn;
							move(lspd, 0); // turn right
							lastMove = 2;
						}
						randomBits >>= 1;
					}
				}
			}
			lastSense = 2;
		}
	}
}

void move(uint8_t lspeed, uint8_t rspeed){
	analogWrite(lmotorpin, lspeed);
	analogWrite(rmotorpin, rspeed);
}

void stop(){
	analogWrite(lmotorpin, 0);
	analogWrite(rmotorpin, 0);
}

// stores the average of 16 readings as a white value
void senseInit(){
	lwhiteval = 0;
	rwhiteval = 0;
	ledon;
	delay(1);
	for(uint8_t i=0; i<16; i++){
		lwhiteval += analogRead(lsensepin);
		delay(1);
		rwhiteval += analogRead(rsensepin);
		delay(9);
	}
	lwhiteval >>= 4;
	rwhiteval >>= 4;
	ledoff;
}

void flashLED(uint8_t flashes){
	while(flashes){
		flashes--;
		ledon;
		delay(200);
		ledoff;
		if(flashes){ delay(500); }
	}
}
