include ../../makefile.in
include ../../EXTERNAL/makefile.in

FC = gfortran
FLAGS = -DUNIX -O3 -g

INCLUDES = -I../../INC -I../../EXTERNAL $(UMF_INC)

# Object files
OBJS_PN = LapPLanN.o functns.o genmat.o
OBJS_PR = LapPLanR.o functns.o genmat.o
OBJS_PN_MatFree = LapPLanN_MatFree.o functns.o genmat.o csr_module.o
OBJS_RN = LapRLanN.o functns.o genmat.o ../../EXTERNAL/evsl_suitesparse.o ../../EXTERNAL/evsl_suitesparse_f90.o
OBJS_RR = LapRLanR.o functns.o genmat.o ../../EXTERNAL/evsl_suitesparse.o ../../EXTERNAL/evsl_suitesparse_f90.o

LIB = -L../../ -levsl $(LIBLAPACK)

ALLEXE = LapPLanN.ex LapPLanR.ex LapPLanN_MatFree.ex LapRLanN.ex LapRLanR.ex

LIB_EXT = $(LIB_UMF) -fopenmp $(LIB0)

#Rules
default: LapPLanN.ex

LapPLanN.o: LapPLanN.f90
	$(FC) $(FLAGS) -o $@ -c $<

LapPLanR.o: LapPLanR.f90
	$(FC) $(FLAGS) -o $@ -c $<

LapPLanN_MatFree.o: LapPLanN_MatFree.f90 csr_module.o csrmatrix.mod
	$(FC) $(FLAGS) -o $@ -c $<

LapRLanN.o: LapRLanN.f90
	$(FC) $(FLAGS) -o $@ -c $<

LapRLanR.o: LapRLanR.f90
	$(FC) $(FLAGS) -o $@ -c $<

functns.o: functns.f90
	$(FC) $(FLAGS) -o $@ -c $<

csr_module.o: csr_module.f90
	$(FC) $(FLAGS) -o $@ -c $<

genmat.o: genmat.f
	$(FC) $(FLAGS) -o $@ -c $<

# Exe
# Polynomial
LapPLanN.ex: $(OBJS_PN)
	$(LINK) -o LapPLanN.ex $(OBJS_PN) $(LIB) $(LIB0)

LapPLanR.ex: $(OBJS_PR)
	$(LINK) -o LapPLanR.ex $(OBJS_PN) $(LIB) $(LIB0)

LapPLanN_MatFree.ex: $(OBJS_PN_MatFree)
	$(LINK) -o LapPLanN_MatFree.ex $(OBJS_PN_MatFree) $(LIB) $(LIB0)

LapRLanN.ex: $(OBJS_RN)
	$(LINK) -o LapRLanN.ex $(OBJS_RN) $(LIB_EXT) $(LIB)

LapRLanR.ex: $(OBJS_RR)
	$(LINK) -o LapRLanR.ex $(OBJS_RR) $(LIB_EXT) $(LIB)

all: $(ALLEXE)

clean:
	rm -f *.o *.mod *.ex
