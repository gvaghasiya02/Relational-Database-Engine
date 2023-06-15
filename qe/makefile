
include ../makefile.inc

all: libqe.a qetest_01 qetest_02 qetest_03 qetest_04 qetest_05 qetest_06 qetest_07 qetest_08 qetest_09 qetest_10 qetest_11 qetest_12 qetest_13 qetest_14 qetest_15 qetest_16

# lib file dependencies
libqe.a: libqe.a(qe.o)  # and possibly other .o files

# c file dependencies
qe.o: qe.h

qetest.o: qe.h

# binary dependencies
qetest_01: qetest_01.o libqe.a $(CODEROOT)/ix/libix.a $(CODEROOT)/rm/librm.a $(CODEROOT)/rbf/librbf.a
qetest_02: qetest_02.o libqe.a $(CODEROOT)/ix/libix.a $(CODEROOT)/rm/librm.a $(CODEROOT)/rbf/librbf.a
qetest_03: qetest_03.o libqe.a $(CODEROOT)/ix/libix.a $(CODEROOT)/rm/librm.a $(CODEROOT)/rbf/librbf.a
qetest_04: qetest_04.o libqe.a $(CODEROOT)/ix/libix.a $(CODEROOT)/rm/librm.a $(CODEROOT)/rbf/librbf.a
qetest_05: qetest_05.o libqe.a $(CODEROOT)/ix/libix.a $(CODEROOT)/rm/librm.a $(CODEROOT)/rbf/librbf.a
qetest_06: qetest_06.o libqe.a $(CODEROOT)/ix/libix.a $(CODEROOT)/rm/librm.a $(CODEROOT)/rbf/librbf.a
qetest_07: qetest_07.o libqe.a $(CODEROOT)/ix/libix.a $(CODEROOT)/rm/librm.a $(CODEROOT)/rbf/librbf.a
qetest_08: qetest_08.o libqe.a $(CODEROOT)/ix/libix.a $(CODEROOT)/rm/librm.a $(CODEROOT)/rbf/librbf.a
qetest_09: qetest_09.o libqe.a $(CODEROOT)/ix/libix.a $(CODEROOT)/rm/librm.a $(CODEROOT)/rbf/librbf.a
qetest_10: qetest_10.o libqe.a $(CODEROOT)/ix/libix.a $(CODEROOT)/rm/librm.a $(CODEROOT)/rbf/librbf.a
qetest_11: qetest_11.o libqe.a $(CODEROOT)/ix/libix.a $(CODEROOT)/rm/librm.a $(CODEROOT)/rbf/librbf.a
qetest_12: qetest_12.o libqe.a $(CODEROOT)/ix/libix.a $(CODEROOT)/rm/librm.a $(CODEROOT)/rbf/librbf.a
qetest_13: qetest_13.o libqe.a $(CODEROOT)/ix/libix.a $(CODEROOT)/rm/librm.a $(CODEROOT)/rbf/librbf.a
qetest_14: qetest_14.o libqe.a $(CODEROOT)/ix/libix.a $(CODEROOT)/rm/librm.a $(CODEROOT)/rbf/librbf.a
qetest_15: qetest_15.o libqe.a $(CODEROOT)/ix/libix.a $(CODEROOT)/rm/librm.a $(CODEROOT)/rbf/librbf.a
qetest_16: qetest_16.o libqe.a $(CODEROOT)/ix/libix.a $(CODEROOT)/rm/librm.a $(CODEROOT)/rbf/librbf.a

# dependencies to compile used libraries
.PHONY: $(CODEROOT)/rbf/librbf.a
$(CODEROOT)/rbf/librbf.a:
	$(MAKE) -C $(CODEROOT)/rbf librbf.a

.PHONY: $(CODEROOT)/rm/librm.a
$(CODEROOT)/rm/librm.a:
	$(MAKE) -C $(CODEROOT)/rm librm.a

.PHONY: $(CODEROOT)/ix/libix.a
$(CODEROOT)/ix/libix.a:
	$(MAKE) -C $(CODEROOT)/ix libix.a

.PHONY: clean
clean:
	-rm qetest_01 qetest_02 qetest_03 qetest_04 qetest_05 qetest_06 qetest_07 qetest_08 qetest_09 qetest_10 qetest_11 qetest_12 qetest_13 qetest_14 qetest_15 qetest_16 *.a *.o *~ Tables* Columns* left* right* large*
	$(MAKE) -C $(CODEROOT)/rm clean
	$(MAKE) -C $(CODEROOT)/ix clean 
