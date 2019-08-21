#ifndef _ODE_SOLVERS_H_
#define _ODE_SOLVERS_H_

/*============================================================================
 * Defines
 *==========================================================================*/
#define FLOATING float

/* global variables for ODE */
//extern const FLOATING gravity;               // free fall acceleration in m/s^2
//extern const FLOATING mass;                // mass of a projectile in kg
//extern const FLOATING radconversion; // = 3.1415926/180.0;  // radians
//extern uint8_t numberoffirstordereqn;                          // number of first-order equations


/*============================================================================
 * Function Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR rk4_dn1(void(dnx)(FLOATING, FLOATING [], FLOATING [], int), 
               FLOATING ti, FLOATING h, FLOATING xi[], FLOATING xf[], int n);
void ICACHE_FLASH_ATTR euler_dn1(void(dnx)(FLOATING, FLOATING [], FLOATING [], int), 
               FLOATING ti, FLOATING h, FLOATING xi[], FLOATING xf[], int n);

#endif /* _ODE_SOLVERS_H_ */
