#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "def.h"
#include "struct.h"
#include "internal_proto.h"
#include "evsl_suitesparse.h"
/** 
 * @file evsl_suitesparse_f90.c
 * @brief Fortran interface definitions
 ***/

/** @brief Fortran interface for SetupBSolSuiteSparse and
 * also SetBsol and SetLTSol
 * @param[in] Bf90: CSR matrix of B
 * @param[out] Bsoldataf90: data pointer for Bsol and LTsol
 */
void EVSLFORT(setup_bsol_suitesparse)(uintptr_t *Bf90, 
                                      uintptr_t *Bsoldataf90) {
  /* cast csr pointer of B */
  csrMat *B = (csrMat *) (*Bf90);
  BSolDataSuiteSparse *Bsol;
  Malloc(Bsol, 1, BSolDataSuiteSparse);
  /* setup B sol and LT sol*/
  SetupBSolSuiteSparse(B, Bsol);
  SetBSol(BSolSuiteSparse, (void *) Bsol);
  SetLTSol(LTSolSuiteSparse, (void *) Bsol); 
  /* cast pointer for output */
  *Bsoldataf90 = (uintptr_t) Bsol;
}

/** @brief Fortran interface for FreeBSolSuiteSparseData */
void EVSLFORT(free_bsol_suitesparse)(uintptr_t *Bsolf90) {
  /* cast pointer */
  BSolDataSuiteSparse *Bsol = (BSolDataSuiteSparse *) (*Bsolf90);
  FreeBSolSuiteSparseData(Bsol);
  free(Bsol);
}

/** @brief Fortran interface for SetupASIGMABSolSuiteSparse 
 * @param[in] Af90: CSR matrix of A
 * @param[in] flagB: if B is present (gen. eig. problem)
 * @param[in] Bf90: CSR matrix of B
 * @param[in] ratf90: rational filter
 * @param[out] solshiftf90: pointer of solshift array
 */
void EVSLFORT(setup_asigmabsol_suitesparse)(uintptr_t *Af90,
                                            int *flagB,
                                            uintptr_t *Bf90,
                                            uintptr_t *ratf90, 
                                            uintptr_t *solshiftf90) {
  /* cast csr pointer of A */
  csrMat *A = (csrMat *) (*Af90);
  /* cast csr pointer of B */
  csrMat *B = (*flagB) ? (csrMat *) (*Bf90) : NULL;
  /* cast pointer */
  ratparams *rat = (ratparams *) (*ratf90);
  /* allocate and setup solshiftdata */
  void **solshiftdata = (void **) malloc(rat->num*sizeof(void *));
  SetupASIGMABSolSuiteSparse(A, B, rat->num, rat->zk, solshiftdata);
  /* cast pointer for output */
  *solshiftf90 = (uintptr_t) solshiftdata;
}

/** @brief Fortran interface for SetASigmaBSol with SuiteSparse solve
 * @param[in,out] ratf90: pointer of rational filter
 * @param[out] solshiftf90: pointer of solshift array
 */
void EVSLFORT(set_asigmabsol_suitesparse)(uintptr_t *ratf90, 
                                          uintptr_t *solshiftf90) {
  /* cast pointers */
  ratparams *rat = (ratparams *) (*ratf90);
  void **solshiftdata = (void **) (*solshiftf90);

  SetASigmaBSol(rat, NULL, ASIGMABSolSuiteSparse, solshiftdata);
}

/** @brief Fortran interface for FreeASIGMABSolSuiteSparse
 * @param ratf90: pointer of rational filter [in/out] 
 * @param solshiftf90: pointer of solshift array [in/out] 
 */
void EVSLFORT(free_asigmabsol_suitesparse)(uintptr_t *ratf90, 
                                           uintptr_t *solshiftf90) {
  /* cast pointers */
  ratparams *rat = (ratparams *) (*ratf90);
  void **solshiftdata = (void **) (*solshiftf90);
  FreeASIGMABSolSuiteSparse(rat->num, solshiftdata);
  free(solshiftdata);
}
