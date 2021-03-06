/* routines.c
 * 
 * Module contains the logic of the
 * various operation modes of the robot
 *
 * MAE3780 Mechatronics Final Project 2013
 * Cornell University
 * Derek Faust, Nicole Panega, Abdullah Sayeem
 */

// Definitions
#ifndef F_CPU
#define F_CPU 16000000UL
#endif

//Include standard headers
#include <stdint.h>			// For data types
#include <util/delay.h>		// For delays in victory dance

//Include Local Headers
#include "routines.h"		// Header for this file
#include "motor.h"			// For motor control
#include "sonar.h"			// For sonar sensing
#include "infrared.h"		// For infrared sensing
#include "qti.h"			// For border sensing
#include "indicator.h"		// For LED and buzzer control

// Constants
#define MOMENTUM_SWITCH_DIST 8	// Distance required to reverse momentum
#define MAX_TRACKING_MISSES 8	// Number of times the object isn't seen before
								// We give up and go back to searching
#define MAX_TRACKING_BOUNDS 2	// Number of times the bounds are seen before
								// we know it's not a glitch.
#define SPOTTED_THRESHOLD 3		// Number of times something must be spotted
								// before attacking
#define SPINOFF_COUNT 5			// Number of counts to execute a spinoff
#define EVADE_COUNT 5			// Number of counts to execute an evade maneuver
#define BACKUP_COUNT 5			// Number of counts to back away from edge

/* Begin Global Functions here
 * ==========================
 *
 */

// Operate the robot in search mode
void routines_search(void){

	// Initialize object detection variable
	int8_t objDetected = 0;
	int8_t oldobjDetected = 0;

	// Copy travel direction
	int8_t direction = motor_dirTravelA;

	// Determine which direction to move
	if(motor_dirTurnA == -1){
		// If turning left, keep turning left
		motor_setSpeed(-3,3);
	}else{
		// If turning right, keep turning right
		motor_setSpeed(3,-3);
	}

	// Reset spotting counter
	uint8_t spotted = 0;
	
	while (spotted<SPOTTED_THRESHOLD){
		// While no object is detected,
		// Keep searching and switch directions when necessary

		while(!qti_touchingBounds && spotted<SPOTTED_THRESHOLD){
			// While no bounds or objects are detected

			// Save last measurement
			oldobjDetected = objDetected;
			// Get new sonar measurement
			objDetected = sonar_getRegion();
			
			if(sonar_isNewDist(objDetected)){
				// If there is new sonar data for the correct direction
				if((objDetected==oldobjDetected) && objDetected){
					// Check if the object is in the direction we're charging
					// If it's not, count a sighting.
					spotted++;
				}else{
					// If the object isn't there
					// reset the sighting counter
					spotted = 0;
				}
			}

		}
		
		if(!objDetected){
			// If a bound was hit,
			// Then switch spin direction
			motor_setSpeed(-motor_currentSpeed[0],-motor_currentSpeed[1]);
		}
	}

	// The object is found, go get it
	routines_attack(objDetected);

}

// Operate the robot in attack mode
void routines_attack(int8_t direction){

	// Charge the opponent
	motor_setSpeed(3*direction,3*direction);

	// Initialize variables to determine when to give up
	uint8_t missCounter = 0;
	uint8_t boundCounter = 0;
	
	while((missCounter<MAX_TRACKING_MISSES) && (boundCounter<MAX_TRACKING_BOUNDS)){
		// While we haven't lost sight more than ## consecutive times
	
		// Poll the sonar
		sonar_getRegion();

		// Look for consecutive misses
		if (sonar_isNewDist(direction)){
			// If there is a new distance measurement
			if(sonar_getRegion()!=direction){
				// Check if the object is in the direction we're charging
				// If it's not, count a miss.
				missCounter++;
			}else{
				// If it is,reset the miss counter.
				missCounter = 0;
			}
		}

		// Looking for consecutive touchingBounds
		if(qti_touchingBounds){
			// Increment the bound counter if we see a bound
			// This is meant to stray signals
			boundCounter++;
		}else{
			// Reset the bound counter if no bound is found
			boundCounter = 0;
		}
			
	}
	if(qti_touchingBounds){
		// If a bound was touched
		if(qti_touchingBounds==direction){
			// If a bound was touched on the pushing side
			// Opponent should be out of the ring
			routines_victoryBack(-direction);
		}else if(qti_touchingBounds==-direction){
			// If a bound was touched on the back side
			// We're being pushed out, try to spin out.
			routines_spin(qti_touchingBounds);
		}
	}
}

// Torero move to avoid being pushed out
void routines_spin(int8_t direction){

	// Determine how to execute the spin
	if(motor_dirTurn == -1){
		// If turning left, set left motor back slightly
		// and set right to max
		motor_setSpeed(-direction,3*direction);
	}else{
		// If turning right, set right motor back slightly
		// and set left to max
		motor_setSpeed(3*direction,-direction);
	}

	// Wait the correct amount of time.
	uint16_t spinCounter = 0;

	for(spinCounter=0; spinCounter<SPINOFF_COUNT; spinCounter++){
		// For the specified number of counts, complete the turn.

		//Keep polling sensors so we have current data when we're done
		sonar_getDistance(0);

		// Add a delay so the loop doesn't execute ridiculously fast
		_delay_us(5);
	}
}

// Back off and continue to search, in case of false alarm
void routines_victoryBack(int8_t direction){
	
	// Backup from the edge of the circle
	motor_setSpeed(3*direction, 3*direction);

	// Keep backing up until the count is done
	uint16_t i_backup;		// Initialize iterator
	
	// Turn on the green LED
	indicator_greenSet(1);

	// For the specified time, back up
	for(i_backup=0; i_backup<BACKUP_COUNT; i_backup++){
		_delay_us(5);				// Slow down the loop
		indicator_beep(); 			// Beep while backing up
		sonar_getRegion();			// Refresh sonar measurement
	}

	// Turn off the green LED
	indicator_greenSet(0);
}
