- Modify the "CODEROOT" variable in makefile.inc to point to the root of your codebase. Usually, this is not necessary.
- Modify the "CODEROOT" variable in makefile.inc to point to the root of your code base if necessary.

- Implement the Record-based Files (RBF) Component:

   Go to folder "rbf" and type in:

    make clean
    make
    ./rbftest1


   The program should run. But it will generates an error. You are supposed to
   implement the API of the paged file manager defined in pfm.h and some
   of the methods in rbfm.h as explained in the project description.

- By default you should not change those functions of the PagedFileManager,
  FileHandle, and RecordBasedFileManager classes defined in rbf/pfm.h and rbf/rbfm.h.
  If you think some changes are really necessary, please contact us first.

- Copy your own implementation of RBF component to folder "rbf"

- Implement the Relation Manager (RM):

- By default you should not change those functions of the RM and RM_ScanIterator class defined in rm/rm.h. If you think some changes are really necessary, please contact us first.

- Copy your own implementation of rbf, ix, and rm to folder, "rbf", "ix", and "rm", respectively.
  Don't forget to include RM extension parts in the rm.h file after you copy your code into "rm" folder.
  
- Implement the extension of Relation Manager (RM) to coordinate data files and the associated indices of the data files.

- Also, implement Query Engine (QE)

   Go to folder "qe" and type in:

    make clean
    make
    ./qetest_01

   The program should work. But it does nothing until you implement the extension of RM and QE.

- If you want to try CLI:

   Go to folder "cli" and type in:
   
   make clean
   make
   ./cli_example_01
   
   or
   
   ./start
   
   The program should work. But you need to implement the extension of RM and QE to run this program properly. Note that examples in the cli directory are provided for your convenience. These examples are not the public test cases.

- By default you should not change those classes defined in rm/rm.h and qe/qe.h. If you think some changes are really necessary, please contact us first.
