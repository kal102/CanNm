# CanNm
**Can Network Management module implementation for AUTOSAR 4.4.0 Classic Platform.**

This is Can Network Management module I did as a student project, feel free to use CanNm.c and CanNm.h files in terms described in MIT license.
Features from 7.1 to 7.9 chapters in specifiation are implemented and labeled in code.
Unit tests using Acutest and fff libraries weren't written by me and they cover only basic functionalities of CanNm module.
Therefore some bugs may still exist in code and I don't guarantee that an unexptected behavior won't happen.
I advise you to develop your own unit tests in case you'd like to fork this project or use it on real hardware.

In Docs directory you can find auto-generated Doxygen documentation together with AUTOSAR module spec.

**Software**
* Compiler: GCC
* Static analysis: CPPCheck
* Unit tests: Acutest

**Commands**
* Compilation: *gcc -fprofile-arcs -ftest-coverage -g UT_CanNm.c -o UT_CanNm.exe*
* Execution: *./UT_CanNm.exe*
* Coverage: *gcov UT_CanNm.c*
