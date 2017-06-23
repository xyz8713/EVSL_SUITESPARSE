#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "blaslapack.h"
#include "def.h" 
#include "string.h"  //for memset

/*-------------------- Protos */
void linspace(double a, double b, int num, double *arr);

int exDOS(double *vals, int n, int npts, 
	  double *x, double *y, double *intv) {
  /* vals = eigenvalues 
     n    = number of eigenvalues
     npts = number of points for dos curve
     
   return
      x   = x-coordinates for dos plot
      y   = y-coordinates for dos plot 
            both x and y are allocated are assumed to have been 
	    pre-allocated. 
  */
// sigma = coefficient for gaussian
// we select sigma so that at end of subinterval
// we have exp[-0.5 [H/sigma]^2] = 1/K.
  int i,j=0, one=1;
  double h, xi, t, scaling;
  const double lm = intv[0];
  const double lM = intv[1];
  const double kappa = 1.25;
  const int M = min(n, 30);
  const double H = (lM - lm) / (M - 1);
  const double sigma = H / sqrt(8 * log(kappa));
  const double a = intv[2];
  const double b = intv[3];
  double sigma2 = 2 * sigma * sigma;
//-------------------- sigma related..
//-------------------- if gaussian smaller than tol ignore point.
  const double tol = 1e-08;
  double width = sigma * sqrt(-2.0 * log(tol));
  // ------------------- define width of sub-intervals for DOS
  linspace(a, b, npts, x);
  h = x[1]-x[0];
  memset(y, 0.0, npts*sizeof(double));
//-------------------- scan all relevant eigenvalues and add to its
//                     interval [if any]
  for (i=0; i<n; i++){
    t = vals[i];
    if (t<a  || t>b)
      continue;
    //-------------------- first point to consider 
    j = max((int) ((t-a-width)/h),0);
    //xi = a+j*h;
    //!! check! 
    for  (xi = a+j*h; xi <= min(t+width,b) ; xi+=h)
      y[j++] +=  exp(-(xi-t)*(xi-t)/sigma2);
  }
  if (j == 0)
    return 1;
//-------------------- scale dos - [n eigenvalues in all]
//  scaling 2 -- for plots due to the missing 1/sqrt[2*pi*sigma^2] factor.
//  However, note that this does not guarantee that 
//  sum[y] * h = the correct number of eigenvalues
//  y = y / sqrt[pi*sigma2] ;
  scaling  = 1.0 / (n*sqrt(sigma2*PI));
  DSCAL(&npts, &scaling, y, &one);
  return 0;
}
