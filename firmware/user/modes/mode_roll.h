/*
 * mode_roll.h
 *
 *  Created on: 4 Aug 2019
 *      Author: bbkiwi
 */

#ifndef MODES_MODE_ROLL_H_
#define MODES_MODE_ROLL_H_
#include "ode_solvers.h"

// specify struct naming the needed parameters must all be FLOATING to match passed parameters array
typedef struct pendP
{
    FLOATING yAccel;
    FLOATING xAccel;
    FLOATING lenPendulum;
    FLOATING damping;
    FLOATING gravity;
    FLOATING force;
} pendParam;

typedef struct springP
{
    FLOATING yAccel;
    FLOATING xAccel;
    FLOATING springConstant;
    FLOATING damping;
    FLOATING force;
} springParam;


typedef struct velP
{
    FLOATING yAccel;
    FLOATING xAccel;
    FLOATING gmult;
    FLOATING force;
} velParam;

void ICACHE_FLASH_ATTR rollEnterMode(void);
led_t* ICACHE_FLASH_ATTR roll_updateDisplayComputations(int16_t xAccel, int16_t yAccel, int16_t zAccel);

void dnxdampedpendulum(FLOATING, FLOATING [], FLOATING [], int, FLOATING [] );
void dnx2dvelocity(FLOATING, FLOATING [], FLOATING [], int, FLOATING []);
void dnxdampedspring(FLOATING, FLOATING [], FLOATING [], int, FLOATING []);


#include "ode_solvers.h"
extern const FLOATING gravity; //= 9.81;               // free fall acceleration in m/s^2
extern const FLOATING mass;                // mass of a projectile in kg
extern const FLOATING radconversion; // = 3.1415926/180.0;  // radians
extern uint8_t numberoffirstordereqn;                          // number of first-order equations


extern swadgeMode rollMode;

#endif /* MODES_MODE_ROLL_H_ */
