/*============================================================================
 * Includes
 *==========================================================================*/

#include <osapi.h>
#include "ode_solvers.h"


/*==============================================================
  ODE Solver from 
  https://ww2.odu.edu/~agodunov/computing/programs/book2/Ch05/rk4n.cpp
  System of first order differential equations for the RK solver
  
  For a system of n first-order ODEs
  x [] array - x values
  dx[] array - dx/dt values
  
  For a system of n/2 second order ODEs follow the agreement
  In:  x[] array 
  # first n/2 elements are x
  # last  n/2 elements are dx/dt
  Out: dx[] array
  # first n/2 elements are first order derivatives  (or dx/dt)
  # last  n/2 elements are second order derivatives (or d2x/dt2)
  example: 2D projectile motion in (x,y) plane
  In           Out
  x[0] - x     dx[0] - x'
  x[1] - y     dx[1] - y'
  x[2] - x'    dx[2] - x"
  x[3] - y'    dx[3] - y"


==========================================================
 rk4_dn1.cpp: Solution of a system of n first-order ODE
 method:      Runge-Kutta 4th-order
 written by: Alex Godunov
 last revision: 7 October 2009
----------------------------------------------------------
 call ...
 dnx(t,x[],dx[],n)- functions dx/dt   (supplied by a user)
 input ...
 ti    - initial time
 tf    - solution time
 xi[]  - initial values 
 n     - number of first order equations
 output ...
 xf[]  - solutions
==========================================================*/
void ICACHE_FLASH_ATTR rk4_dn1(void(dnx)(FLOATING, FLOATING [], FLOATING [], int), 
               FLOATING ti, FLOATING h, FLOATING xi[], FLOATING xf[], int n)
{
      FLOATING t, x[n], dx[n];
      FLOATING k1[n],k2[n],k3[n],k4[n];
      int j;

      t = ti;
//k1
      dnx(t, xi, dx, n);
      for (j = 0; j<=n-1; j = j+1)
        {
          k1[j] = h*dx[j];
          x[j]  = xi[j] + k1[j]/2.0;  
        }      
//k2
      dnx(t+h/2.0, x, dx, n);
      for (j = 0; j<=n-1; j = j+1)
        {
          k2[j] = h*dx[j];
          x[j]  = xi[j] + k2[j]/2.0;  
        }
//k3
      dnx(t+h/2.0, x, dx, n);
      for (j = 0; j<=n-1; j = j+1)
        {
          k3[j] = h*dx[j];
          x[j]  = xi[j] + k3[j];  
        }      
//k4 and result      
      dnx(t+h, x, dx, n);
      for (j = 0; j<=n-1; j = j+1)
        {
          k4[j] = h*dx[j];
          xf[j] = xi[j] + k1[j]/6.0+k2[j]/3.0+k3[j]/3.0+k4[j]/6.0;
        }      
}

/* Eulers method for a system */
void ICACHE_FLASH_ATTR euler_dn1(void(dnx)(FLOATING, FLOATING [], FLOATING [], int), 
               FLOATING ti, FLOATING h, FLOATING xi[], FLOATING xf[], int n)
{
      FLOATING t, dx[n];
      int j;

      t = ti;
//result after 1 step
      dnx(t, xi, dx, n);
      for (j = 0; j<=n-1; j = j+1)
        {
          xf[j]  = xi[j] + h*dx[j];  
        }      
}

