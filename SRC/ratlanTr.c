#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <float.h>
#include <complex.h>
#include "def.h"
#include "blaslapack.h"
#include "struct.h"
#include "internal_proto.h"

/**
 * @file ratlanTr.c
 * @brief Rational filtered thick restart Lanczos
 */
/**
 * if filter the initial vector
 */ 
#define FILTER_VINIT 1
/**

 * @brief RatLanTR  filtering Lanczos process [Thick restart version]
 *
 * @param lanm      Dimension of Krylov subspace [restart dimension]
 * @param nev       Estimate of number of eigenvalues in the interval --
 *         ideally nev == exact number or a little larger.  This is not used
 *         for testing convergence but it helps make decisions as to when to
 *         test convergence RatLanTr attempts to compute *all* eigenvalues in
 *         the interval and stops only when no more eigenvalyes are left. The
 *         convergenve test is a very simple one based on the residual norm for
 *         the filtered matrix 
 *
 * @param intv   an array of length 4  \n
 *         [intv[0], intv[1]] is the interval of desired eigenvalues \n
 *         [intv[2], intv[3]] is the global interval of all eigenvalues \n
 *         it must contain all eigenvalues of A
 * 
 * @param maxit  max Num of outer Lanczos iterations (restarts) allowed -- 
 *         Each restart may or use the full lanm lanczos steps or fewer.
 * 
 * @param tol       tolerance for convergence. stop when ||res||< tol
 * @param vinit     initial  vector for Lanczos -- [optional]
 * @param rat       a struct containing the parameters of the rational.
 *
 * @warning RatLanTr()  Modifies the following variables
 * @param[out] nev2     Number of eigenvalues/vectors computed
 * @param[out] W        A set of eigenvectors  [n x nev2 matrix]
 *                      of unit 2-norm for standard eig prob
 *                      of unit B-norm for generalized eig prob
 * @param[out] vals     Associated eigenvalues [nev2 x 1 vector]
 * @param[out] resW     Associated residual norms [nev x 1 vector]
 *                      2-norm for standard eig prob
 *                      B-norm for generalized eig prob
 * @param[out] fstats   File stream which stats are printed to
 *
 * @return Returns 0 on success (or if check_intv() is non-positive), and 2 if there are no eigenvalues found.
 *
 *
 * @warning memory allocation for W/vals/resW within this function 
 *
 **/
int RatLanTr(int lanm, int nev, double *intv, int maxit, 
             double tol, double *vinit, ratparams *rat, int *nev2, 
             double **vals, double **W, double **resW, FILE *fstats) {
  const int ifGenEv = evsldata.ifGenEv;
  /*-------------------- for stats */
  double tall=0.0;
  //double tolP = tol;
  double tr, last_tr;
  tall = cheblan_timer();
  int do_print = 1;
  /* handle case where fstats is NULL. Then no output. Needed for openMP. */
  if (fstats == NULL) {
    do_print = 0;
  }
  /* size of the matrix */
  int n = evsldata.n;
  /*--------------------- adjust lanm and maxit */
  lanm = min(lanm, n);
  int lanm1=lanm+1;
  /*  if use full lanczos, should not do more than n iterations */
  if (lanm == n) {
    maxit = min(maxit, n);
  }
  /*-------------------- this is needed to increment the space when we
                         discover more than nev eigenvalues in interval */
  double nevInc = 0.2;   /* add 1  + 20% each time it is needed */
  /*-------------------- if we have at least nev/ev_frac good candidate 
                         eigenvalues from p(A) == then we restart to lock them in */
  //int evFrac = 2;
  /*--------------------   some constants frequently used */
  /* char cT='T'; */
  char cN = 'N';
  int one = 1;
  double done=1.0, dmone=-1.0, dzero=0.0;
  /*-------------------- Ntest = when to start testing convergence */
  int Ntest = min(lanm, nev+50);
  /*--------------------   how often to test */
  int cycle = 30; 
  int i, ll, /*count, last_count,*/ jl, last_jl;
  /*-----------------------------------------------------------------------
    -----------------------------------------------------------------------*/
  /* check if the given interval is valid */
  if (check_intv(intv, fstats) < 0) {
    *nev2 = 0;
    *vals = NULL; *W = NULL; *resW = NULL;
    return 0;
  }
  double aa = intv[0];
  double bb = intv[1];
  //int deg = rat->pow;
  double bar = 0.5; // for the scaled rational filter
  /*-----------------------------------------------------------------------* 
   * *thick restarted* Lanczos step 
   *-----------------------------------------------------------------------*/
  if (do_print) {
    fprintf(fstats, " Rational-LanTR, dim %d  cycle %d  \n",lanm, cycle);
  }
  /*--------------------- the min number of steps to be performed for 
   *                      each innter loop, must be >= 1 - if not, 
   *                      it means that the Krylov dim is too small */
  int min_inner_step = 5;
  /*-------------------- it = number of Lanczos steps */
  int it = 0;
  /*-------------------- Lanczos vectors V_m and tridiagonal matrix T_m */
  double *V, *T;
  Malloc(V, n*lanm1, double);
  /*-------------------- for gen eig prob, storage for V = B * Z */
  double *Z;
  if (ifGenEv) {
    Malloc(Z, n*lanm1, double);
  } else {
    Z = V;
  }
  /*-------------------- T must be zeroed out initially */
  Calloc(T, lanm1*lanm1, double);
  /*-------------------- Lam, Y: the converged (locked) Ritz values/vectors 
                         res: related residual norms */
  double *Y, *Q, *Lam, *res;
  Malloc(Y, n*nev, double);
  Malloc(Lam, nev, double);
  Malloc(res, nev, double);
  /*-------------------- for gen eig prob, storage for B*Y */
  if (ifGenEv) {
    Malloc(Q, n*nev, double);
  } else {
    Q = Y;
  }
  /*-------------------- lock =  number of locked vectors */
  int lock = 0;
  /*-------------------- trlen = dim. of thick restart set */
  int trlen = 0, prtrlen=-1;
  /*-------------------- Ritz values and vectors of p(A) */
  double *Rval, *Rvec, *resi, *Bvec = NULL;
  Malloc(Rval, lanm, double);
  Malloc(resi, lanm, double);
  Malloc(Rvec, n*lanm, double);
  if (ifGenEv) {
    Malloc(Bvec, n*lanm, double);
  } else {
    Bvec = Rvec;
  }
  /*-------------------- Eigen vectors of T */
  double *EvecT;
  Malloc(EvecT, lanm1*lanm1, double);
  /*-------------------- s used by TR (the ``spike'' of 1st block in Tm)*/
  double *s;
  Malloc(s, lanm, double);
  /*-------------------- alloc some work space */
  double *work, *vrand = NULL;
  int work_size = ifGenEv ? 6*n : 4*n;
  Malloc(work, work_size, double);  
#if FILTER_VINIT  
  RatFiltApply(n, rat, vinit, V, work);
  Malloc(vrand, n, double);
  /*-------------------- copy initial vector to Z(:,1)   */
  if(ifGenEv){
    DCOPY(&n, V, &one, Z, &one);
  }  
#else
  /*-------------------- copy initial vector to Z(:,1)   */
    DCOPY(&n, vinit, &one, Z, &one);
#endif
  /*-------------------- normalize it */
  double t;
  if (ifGenEv) {
    /* v = B*z */
    matvec_B(Z, V);
    /* B norm of z*/
    t = 1.0 / sqrt(DDOT(&n, V, &one, Z, &one));
    DSCAL(&n, &t, Z, &one);
  } else {
    /* 2-norm */
    t = 1.0 / DNRM2(&n, V, &one);
  }
  /* unit B^{-1}-norm or 2-norm */
  DSCAL(&n, &t, V, &one);
  /*-------------------- main (restarted Lan) outer loop */
  while (it < maxit) {
    /*-------------------- for ortho test */
    double wn = 0.0;
    int nwn = 0;
    /*  beta */
    double beta = 0.0, r, res0; 
    /*  start with V(:,k) */
    int k = trlen > 0 ? trlen +1 : 0;
    /* ! add a test if dimension exceeds (m+1) 
     * (trlen + 1) + min_inner_step <= lanm + 1 */
    if (k+min_inner_step > lanm1) {
      fprintf(fstats, "Krylov dim too small for this problem. Try a larger dim\n");
      exit(1);
    }
    /*-------------------- thick restart special step */
    if (trlen > 0) {
       int k1 = k-1;
      /*------------------ a quick reference to V(:,k) */
      double *v = &V[k1*n];
      double *z = &Z[k1*n];
      /*------------------ next Lanczos vector */
      double *vnew = v + n;
      double *znew = z + n;
      /*------------------ znew = R(A) * v */
      RatFiltApply(n, rat, v, znew, work);
      /*------------------ deflation */
      if (lock > 0) {
        if (ifGenEv) {
          /* orthgonlize against locked vectors first, w = w - B*Y*Y'*w */
          CGS_DGKS2(n, lock, NGS_MAX, Q, Y, znew, work);
        } else {
          /* orthgonlize against locked vectors first, w = w - Y*Y'*w */
          CGS_DGKS(n, lock, NGS_MAX, Y, vnew, NULL, work);
        }
      }
      /*-------------------- restart with 'trlen' Ritz values/vectors
                             T = diag(Rval(1:trlen)) */
      for (i=0; i<trlen; i++) {
        T[i*lanm1+i] = Rval[i];
        wn += fabs(Rval[i]);
      }
      /*--------------------- s(k) = V(:,k)'* znew */
      s[k1] = DDOT(&n, v, &one, znew, &one);
      /*--------------------- znew = znew - Z(:,1:k)*s(1:k) */
      DGEMV(&cN, &n, &k, &dmone, Z, &n, s, &one, &done, znew, &one);
      /*-------------------- expand T matrix to k-by-k, arrow-head shape
                             T = [T, s(1:k-1)] then T = [T; s(1:k)'] */
      for (i=0; i<k1; i++) {
        T[trlen*lanm1+i] = s[i];
        T[i*lanm1+trlen] = s[i];
        wn += 2.0 * fabs(s[i]);
      }
      T[trlen*lanm1+trlen] = s[k1];
      wn += fabs(s[k1]);
      if (ifGenEv) {
        /*-------------------- vnew = B * znew */
        matvec_B(znew, vnew);
        /*-------------------- beta = (vnew, znew)^{1/2} */
        beta = sqrt(DDOT(&n, vnew, &one, znew, &one));
      } else {
        /*-------------------- beta = norm(w) */
        beta = DNRM2(&n, vnew, &one);
      }
      wn += 2.0 * beta;
      nwn += 3*k;
      /*   beta ~ 0 */
      if (beta*nwn < orthTol*wn) {
        if (do_print) {
          fprintf(fstats, "it %4d: Lucky breakdown, beta = %.15e\n", k, beta);
        }    
# if FILTER_VINIT      
      /*------------------ generate a new init vector in znew */
      rand_double(n, vrand);
      /* Filter the initial vector */
      RatFiltApply(n, rat, vrand, znew, work);
#else 
      rand_double(n, znew);
#endif                       
        if (ifGenEv) {
	  /* orthgonlize against locked vectors first, w = w - B*Y*Y'*w */
          CGS_DGKS2(n, lock, NGS_MAX, Q, Y, znew, work);
	  /* znew = znew - Z(:,1:k)*V(:,1:k)'*znew */
          CGS_DGKS2(n, k, NGS_MAX, Z, V, znew, work);    
          matvec_B(znew, vnew);
          beta = sqrt(DDOT(&n, vnew, &one, znew, &one));
          double ibeta = 1.0/beta;
          DSCAL(&n, &ibeta, vnew, &one);          
          DSCAL(&n, &ibeta, znew, &one);
          beta = 0.0;         
        } else {
	  /* orthgonlize against locked vectors first, w = w - Y*Y'*w */
          CGS_DGKS(n, lock, NGS_MAX, Y, vnew, NULL, work);
	  /*   vnew = vnew - V(:,1:k)*V(:,1:k)'*vnew */
	  /*   beta = norm(w) */
          CGS_DGKS(n, k, NGS_MAX, V, vnew, &beta, work);
          double ibeta = 1.0/beta;
          DSCAL(&n, &ibeta, vnew, &one);
          beta = 0.0;
        }
      } else {
	/*------------------- w = w / beta */
	double ibeta = 1.0 / beta;
	DSCAL(&n, &ibeta, vnew, &one);
	if (ifGenEv) {
	  DSCAL(&n, &ibeta, znew, &one);
	}
      }
      /*------------------- T(k+1,k) = beta; T(k,k+1) = beta; */
      T[k1*lanm1+k] = beta;
      T[k*lanm1+k1] = beta;
    } /* if (trlen > 0) */
    /*-------------------- Done with TR step. Rest of Lanczos step */
    /*-------------------- reset Ntest at each restart. */
    Ntest = max(20,nev-lock+10);
    /*last_count = 0;*/  last_jl = 0; last_tr = 0.0;
    /*-------------------- regardless of trlen, *(k+1)* is the current 
     *                     number of Lanczos vectors in V */
    /*-------------------- pointer to the previous Lanczos vector */
    double *zold = k > 0 ? Z+(k-1)*n : NULL;
    /*------------------------------------------------------*/
    /*------------------ Lanczos inner loop ----------------*/
    /*------------------------------------------------------*/
    while (k < lanm && it < maxit) {
      k++;
      /*---------------- a quick reference to V(:,k) */
      double *v = &V[(k-1)*n];
      double *z = &Z[(k-1)*n];
      /*---------------- next Lanczos vector */
      double *vnew = v + n;
      double *znew = z + n;
      /*------------------ znew = R(A) * v */
      RatFiltApply(n, rat, v, znew, work);
      it++;
      /*-------------------- deflation: orthgonalize vs locked ones first */
      if (lock > 0) {
        if (ifGenEv) {
          /* orthgonlize against locked vectors first, znew = znew - B*Y*Y'*znew */
          CGS_DGKS2(n, lock, NGS_MAX, Q, Y, znew, work);
        } else {
          /*--------------------   vnew = vnew - Y*Y'*vnew */
          CGS_DGKS(n, lock, NGS_MAX, Y, vnew, NULL, work);
        }
      }
      /*-------------------- znew = znew - beta*zold */
      if (zold) {
        double nbeta = -beta;
        DAXPY(&n, &nbeta, zold, &one, znew, &one);
      }
      /*-------------------- alpha = znew'*v */
      double alpha = DDOT(&n, v, &one, znew, &one);
      /*-------------------- T(k,k) = alpha */
      T[(k-1)*lanm1+(k-1)] = alpha;
      wn += fabs(alpha);
      /*-------------------- znew = znew - alpha*z */
      double nalpha = -alpha;
      DAXPY(&n, &nalpha, z, &one, znew, &one);
      /*-------------------- FULL reortho to all previous Lan vectors */
      if (ifGenEv) {
        /* znew = znew - Z(:,1:k)*V(:,1:k)'*znew */
        CGS_DGKS2(n, k, NGS_MAX, Z, V, znew, work);
        /*-------------------- vnew = B * znew */
        matvec_B(znew, vnew);
        /*-------------------- beta = (vnew, znew)^{1/2} */
        beta = sqrt(DDOT(&n, vnew, &one, znew, &one));
      } else {
        /*   vnew = vnew - V(:,1:k)*V(:,1:k)'*vnew */
        /*   beta = norm(w) */
        CGS_DGKS(n, k, NGS_MAX, V, vnew, &beta, work);
      }
      wn += 2.0 * beta;
      nwn += 3;
      /*-------------------- zold = z */
      zold = z;
      /*-------------------- lucky breakdown test */
      if (beta*nwn < orthTol*wn) {
        if (do_print) {
          fprintf(fstats, "it %4d: Lucky breakdown, beta = %.15e\n", it, beta);
        }
# if FILTER_VINIT      
      /*------------------ generate a new init vector in znew */
      rand_double(n, vrand);
      /* Filter the initial vector */
      RatFiltApply(n, rat, vrand, znew, work);
#else 
      rand_double(n, znew);
#endif                
        if (ifGenEv) {
	  /* orthgonlize against locked vectors first, w = w - B*Y*Y'*w */
          CGS_DGKS2(n, lock, NGS_MAX, Q, Y, znew, work);
	  /* znew = znew - Z(:,1:k)*V(:,1:k)'*znew */
          CGS_DGKS2(n, k, NGS_MAX, Z, V, znew, work);    
          matvec_B(znew, vnew);
          beta = sqrt(DDOT(&n, vnew, &one, znew, &one));
          double ibeta = 1.0 / beta;
          DSCAL(&n, &ibeta, vnew, &one);          
          DSCAL(&n, &ibeta, znew, &one);
          beta = 0.0;            
        } else {
	  /* orthgonlize against locked vectors first, w = w - Y*Y'*w */
          CGS_DGKS(n, lock, NGS_MAX, Y, vnew, NULL, work);
	  /*   vnew = vnew - V(:,1:k)*V(:,1:k)'*vnew */
	  /*   beta = norm(w) */
          CGS_DGKS(n, k, NGS_MAX, V, vnew, &beta, work);
          double ibeta = 1.0 / beta;
          DSCAL(&n, &ibeta, vnew, &one);
          beta = 0.0;
        }
      } else {  
	/*---------------------- vnew = vnew / beta */
	double ibeta = 1.0 / beta;
	DSCAL(&n, &ibeta, vnew, &one);
	if (ifGenEv) {
	  /*-------------------- znew = znew / beta */
	  DSCAL(&n, &ibeta, znew, &one);
	}
      }
      /*-------------------- T(k,k+1) = T(k+1,k) = beta */
      T[k*lanm1+(k-1)] = beta;
      T[(k-1)*lanm1+k] = beta;
      /*---------------------- Restarting test */
      int k1 = k-trlen-Ntest;
      if ( ((k1>=0) && (k1 % cycle == 0)) || (k == lanm) || it == maxit) {
        /*-------------------- solve eigen-problem for T(1:k,1:k)
                               vals in Rval, vecs in EvecT */
        /* printf("k %d, trlen %d, Ntest %d, its %d\n", k, trlen, Ntest, it); */
        SymEigenSolver(k, T, lanm1, EvecT, lanm1, Rval);
        /*-------------------- max dim reached-break from the inner loop */
        if (k == lanm || it == maxit) {
          break;
        }
#if 1
        jl = 0;
        tr = 0.0;
        for (i=0; i<k; i++) {
          if (Rval[i] +DBL_EPSILON >= bar) {
            jl++;
            tr += Rval[i];
          }
        }
        if (fabs(tr-last_tr) <= tol * fabs(tr) && jl == last_jl) {
          fprintf(fstats,"break: [it %d, k %d]: last_tr %.15e, tr %.15e, last_jl %d  jl %d\n",
                  it, k, last_tr, tr, last_jl, jl);
          break;
        }
        fprintf(fstats,"[it %d, k %d] testing: last_tr %.15e, tr %.15e, last_jl %d  jl %d\n",
                it, k, last_tr, tr, last_jl, jl);
        last_tr = tr;
        last_jl = jl;        
#else         
        count = 0;
        /*-------------------- get residual norms and check acceptance of
                               Ritz values for R(A). */
        /*-------------------- count e.vals in interval + those that convergeed */
        jl = 0;
        for (i=0; i<k; i++) {
          if (Rval[i]>= bar) {
            jl++;
            /* for standard e.v prob, this is the 2-norm of A*y-lam*y 
             * for gen e.v prob, this is the B-norm of B^{-1}*A*y-lam*y */
            r = fabs(beta*EvecT[i*lanm1+(k-1)]);
            resi[i] = r;
            if (r < tolP) {
              count++;
            }
          }
        }
        /*-------------------- testing (partial) convergence for restart */
        if (do_print) {
          fprintf(fstats,"  --> testing conv k %4d, it %4d, count %3d  jl %3d trlen %3d\n",
                  k, it, count, jl, trlen);
        }
        /*-------------------- enough good candidates 1st time -> break */
        /* if ((count*evFrac >= nev-lock) && (prtrlen==-1)) */
        if (count*evFrac >= nev-lock) {
          break;
        }
        /*-------------------- count & jl unchanged since last test --> break */
        if ((count<=last_count) && (jl<=last_jl)) {
          break;
        }
        last_count = count;  
        last_jl = jl;
#endif        
      } /* if [Restarting test] block */
      /*-------------------- end of inner (Lanczos) loop - Next: restart*/        
    } /* while (k<mlan) loop */

    /*--------------------   TWO passes to select good candidates */
    /*                       Pass-1: based on if ``p(Ritzvalue) > bar'' */	    
    jl = 0;
    for (i=0; i<k; i++) {
      /*--------------------   if this Ritz value is higher than ``bar'' */
      if (Rval[i] >= bar) {
        /* move good eigenvectors/vals to front */
        if (i != jl) {
          DCOPY(&k, EvecT+i*lanm1, &one, EvecT+jl*lanm1, &one);
          Rval[jl] = Rval[i];
          resi[jl] = resi[i];
        }
        jl++;
      }
    }
    /*---------------------- Compute the Ritz vectors: 
     *                       Rvec(:,1:jl) = V(:,1:k) * EvecT(:,1:jl) */
    DGEMM(&cN, &cN, &n, &jl, &k, &done, V, &n, EvecT, &lanm1, &dzero, Rvec, &n);
    if (ifGenEv) {
      DGEMM(&cN, &cN, &n, &jl, &k, &done, Z, &n, EvecT, &lanm1, &dzero, Bvec, &n);
    }
    /*-------------------- Pass-2: check if Ritz vals of A are in [a,b] */
    /*                     number of Ritz values in [a,b] */
    ll = 0;
    /*-------------------- trlen = # Ritz vals that will go to TR set */
    prtrlen = trlen; 
    trlen = 0;
    for (i=0; i<jl; i++) {
      double *q = Rvec + i*n;
      double *y = Bvec + i*n;
      double *w = work;
      double *w2 = w + n;
      /*------------------ normalize just in case. */
      if (ifGenEv) {
        /* B-norm, w2 = B*y */
        matvec_B(y, w2);
        t = sqrt(DDOT(&n, y, &one, w2, &one));
      } else {
        /* 2-norm */
        t = DNRM2(&n, y, &one);
      }
      /*-------------------- return code 2 --> zero eigenvector found */
      if (t == 0.0) {
        return 2;
      }
      /*-------------------- scal y */
      t = 1.0 / t;
      DSCAL(&n, &t, y, &one);
      if (ifGenEv) {
        DSCAL(&n, &t, w2, &one);
      }
      /*-------------------- w = A*y */
      matvec_A(y, w);
      /*-------------------- Ritzval: t3 = (y'*w)/(y'*y) or
       *                              t3 = (y'*w)/(y'*B*y) */
      /*-------------------- Rayleigh quotient */
      double t3 = DDOT(&n, y, &one, w, &one);
      /*--------------------  if lambda (==t3) is in [a,b] */
      if (t3 >= aa - DBL_EPSILON && t3 <= bb + DBL_EPSILON) {
        ll++;
        /*-------------------- compute residual wrt A for this pair */
        double nt3 = -t3;
        if (ifGenEv) {
          /* w = w - t3*w2, w2 = B*y,  (w=A*y-t3*B*y) */
          DAXPY(&n, &nt3, w2, &one, w, &one);
          /* res0 = 2-norm of w */
          res0 = DNRM2(&n, w, &one);
        } else {
          /*-------------------- w = w - t3*y, (w=A*y-t3*y) */
          DAXPY(&n, &nt3, y, &one, w, &one);
          /*-------------------- res0 = norm(w) */
          res0 = DNRM2(&n, w, &one);
        }
        /*-------------------- test res. of this Ritz pair against tol */
        /* r = resi[i];*/
        r = res0;
        if (r < tol) {
          //-------------------- check if need to realloc
          if (lock >= nev) {
            nev += 1 + (int) (nev*nevInc);
            if (do_print) {
              fprintf(fstats, "-- More eigval found: realloc space for %d evs\n", nev);
            }
            Realloc(Y, nev*n, double);
            if (ifGenEv) {
              Realloc(Q, nev*n, double);
            }
            Realloc(Lam, nev, double);
            Realloc(res, nev, double);
          }
          /*--------------------   accept (t3, y) */
          DCOPY(&n, y, &one, Q+lock*n, &one);
          if (ifGenEv) {
            DCOPY(&n, q, &one, Y+lock*n, &one);
          }
          Lam[lock] = t3;
          res[lock] = res0;
          lock++;
        } else {
          /*-------------------- restart; move Ritz pair for TR to front */
          Rval[trlen] = Rval[i];
          DCOPY(&n, y, &one, Z+trlen*n, &one);
          if (ifGenEv) {
            DCOPY(&n, q, &one, V+trlen*n, &one);
          }
          /* special vector for TR that is the bottom row of 
           * eigenvectors of Tm */
          s[trlen] = beta * EvecT[i*lanm1+(k-1)];
          trlen ++;
        }
      }
    }/* end of 2nd pass */

    /*-------Note:   jl = #evs that passed the 1st test,
     *       ll = #evs that passed the 1st and the 2nd tests.
     *       These ll evs are either locked (accepted) 
     *       or put into trlen (candidates for next restarts)
     *       step when trlen = 0 last restart and ll=0 this time.
     *       this is a sort of confirmation that nothing is left. 
     *       another test may be added later to make it more rigorous.
     */       
    if (do_print) {
      fprintf(fstats,"it %4d:  k %3d, jl %3d, ll %3d, lock %3d, trlen %3d\n",
              it, k, jl, ll, lock, trlen);
    }
    /*-------------------- TESTs for stopping */
    if ((prtrlen == 0) && (ll==0)) {
      /*--------------------  It looks like: Nothing left to compute  */
      if (do_print) {
        fprintf(fstats, "--------------------------------------------------\n");
        fprintf(fstats, " --> No ev.s left to be computed\n");
      }
      break;
    }
    if (it == maxit){
      if (do_print) {
        fprintf(fstats, "--------------------------------------------------\n");
        fprintf(fstats, " --> Max its reached without convergence\n");
      }
      break;
    }
    /*-------------------- prepare to restart.  First zero out all T */
    memset(T, 0, lanm1*lanm1*sizeof(double));
    /*-------------------- move starting vector vector V(:,k+1);  V(:,trlen+1) = V(:,k+1) */
    DCOPY(&n, V+k*n, &one, V+trlen*n, &one);
    if (ifGenEv) {
      DCOPY(&n, Z+k*n, &one, Z+trlen*n, &one);
    }
  } /* outer loop (it) */

  if (do_print) {
    fprintf(fstats, "     Number of evals found = %d\n", lock);
    fprintf(fstats, "--------------------------------------------------\n");
  }

  /*-------------------- Done.  output : */
  *nev2 = lock;
  *vals = Lam;
  *W = Q;
  *resW = res;
  /*-------------------- free arrays */
  free(V);
  free(T);
  free(Rval);
  free(resi);
  free(EvecT);
  free(Rvec);
  free(s);
  free(work);
  if (vrand) {
    free(vrand);
  }  
  if (ifGenEv) {
    free(Z);
    free(Y);
    free(Bvec);
  }
  /*-------------------- record stats */
  tall = cheblan_timer() - tall;
  evslstat.t_iter = tall;
 
  return 0;
}

