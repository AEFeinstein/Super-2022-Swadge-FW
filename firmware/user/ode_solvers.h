#ifndef _ODE_SOLVERS_H_
#define _ODE_SOLVERS_H_

/*============================================================================
 * Defines
 *==========================================================================*/
#define FLOATING float

/*============================================================================
 * Function Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR rk4_dn1(void(dnx)(FLOATING, FLOATING [], FLOATING [], int), 
               FLOATING ti, FLOATING h, FLOATING xi[], FLOATING xf[], int n);
void ICACHE_FLASH_ATTR euler_dn1(void(dnx)(FLOATING, FLOATING [], FLOATING [], int), 
               FLOATING ti, FLOATING h, FLOATING xi[], FLOATING xf[], int n);

#endif /* _ODE_SOLVERS_H_ */
