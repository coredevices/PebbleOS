
Title: README

D.U.M.A. - Detect Unintended Memory Access - A Red-Zone memory allocator:


DESCRIPTION:


DUMA helps you detect two common programming bugs:
software that overruns the boundaries of a malloc() memory allocation, and
software that touches a memory allocation that has been released by free().
Unlike other malloc() debuggers, DUMA will detect read accesses as well as
writes, and it will pinpoint the exact instruction that causes an error.
It has been in use at Pixar since 1987, and at many other sites for years.

DUMA uses the virtual memory hardware of your computer to place an inaccessible
memory page immediately after (or before, at the user's option) each memory
allocation. When software reads or writes this inaccessible page, the hardware
issues a segmentation fault, stopping the program at the offending instruction.
It is then trivial to find the erroneous statement using your favorite
debugger. In a similar manner, memory that has been released by free() is made
inaccessible, and any code that touches it will get a segmentation fault.

Simply linking your application with libduma.a will allow you to detect most,
but not all, malloc buffer overruns and accesses of free memory. If you want
to be reasonably sure that you've found all bugs of this type, you'll have to
read and understand the rest of this man page.


USAGE:


Link your program with the library libduma.a. Make sure you are not linking
with '-lmalloc', '-lmallocdebug', or with other malloc-debugger or
malloc-enhancer libraries. You can only use one at a time.

If your system administrator has installed DUMA for public use, you'll be able
to use the '-lduma' argument to the linker, otherwise you'll have to put the
path-name for libduma.a in the linker's command line.
You can also use dynamic linking. If you're using a Bourne shell, the statement
'export LD_PRELOAD=libduma.so.0.0' will cause DUMA to be loaded to run all
dynamic executables.

The command 'duma.sh <command>' runs a single command under DUMA.

Some systems will require special arguments to the linker to assure that you
are using the DUMA malloc() and not the one from your C library.

Run your program using a debugger. It's easier to work this way than to create
a core file and post-mortem debug it. DUMA can create huge core files, and some
operating systems will thus take minutes simply to dump core! Some operating
systems will not create usable core files from programs that are linked with
DUMA.

If your program has one of the errors detected by DUMA, it will get a
segmentation fault (SIGSEGV) at the offending instruction. Use the debugger to
locate the erroneous statement, and repair it.


GLOBAL AND ENVIRONMENT VARIABLES:


DUMA has several configuration switches that can be enabled via
the shell environment. These switches change what bugs DUMA will detect,
so it's important that you know how to use them.

In older versions of DUMA you could set the value of
global integer variables (using a debugger). In actual DUMA versions most
of the global variables don't exist any more: they changed to thread local
variables defined in structures. Instead you can call macro function to set
some variables - but not from debugger!

You can use the gdb command 'set environment variable value' to set shell
environment variables only for the program you are going to debug. This is
useful especially if you are using the shared DUMA library.

DUMA_ALIGNMENT - This is an integer that specifies the alignment for any memory
  allocations that will be returned by malloc(), calloc(), and realloc().
  The value is specified in bytes, thus a value of 4 will cause memory to be
  aligned to 32-bit boundaries unless your system doesn't have a 8-bit
  characters. DUMA_ALIGNMENT is set to the minimum required alignment specific
  to your environment by default. The minimum required alignment is detected by
  createconf and stored in the file duma_config.h.
  If your program requires that allocations be aligned to 64-bit boundaries
  you'll have to set this value to 8. This is the case when compiling with the
  '-mips2' flag on MIPS-based systems such as those from SGI. For some
  architectures the default is defined to even more - x86_64 uses alignment to
  16 bytes by default.
  DUMA internally uses a smaller value if the requested memory size is smaller
  than the alignment value: the next smaller power of 2 is used.
  Thus allocating blocks smaller than DUMA_ALIGNMENT may result into smaller
  alignments - for example when allocating 3 bytes, they would be aligned to 2
  byte boundary. This allows better detection of overrun.
  For this reason, you will sometimes want to set DUMA_ALIGNMENT to 1 (no
  alignment), so that you can detect overruns of less than your CPU's word
  size. Be sure to read the section 'WORD-ALIGNMENT AND OVERRUN DETECTION' in
  this manual page before you try this.
  To change this value, set DUMA_ALIGNMENT in the shell environment to an
  integer value, or call the macro function DUMA_SET_ALIGNMENT() from your
  code.
  You don't need to change this setting, if you just need bigger alignment for
  some special buffers. In this case you may use the function
  memalign(alignment, userSize).

DUMA_PROTECT_BELOW - DUMA usually places an inaccessible page immediately after
  each memory allocation, so that software that runs past the end of the
  allocation will be detected. Setting DUMA_PROTECT_BELOW to 1 causes DUMA to
  place the inaccessible page before the allocation in the address space, so
  that under-runs will be detected instead of over-runs.
  To change this value, set DUMA_PROTECT_BELOW in the shell environment to an
  integer value, or call the macro function DUMA_SET_PROTECT_BELOW() from your
  code.

DUMA_SKIPCOUNT_INIT - DUMA usually does its initialization with the first
  memory allocation. On some systems this may collide with initialization of
  pthreads or other libaries and produce a hang. To get DUMA work even in these
  situations you can control (with this environment variable) after how many
  allocations the full internal initialization of DUMA is done. Default is 0.

DUMA_REPORT_ALL_LEAKS - DUMA usually reports only memory leaks where the source
  filename with line number of the allocating instruction is known. Setting this
  variable to 1 in shell environment reports all memory leaks.
  The default is 0 to avoid reporting of irrelevant memory leaks from
  system/compiler environment: there are many standard libraries leaking memory,
  which by default is no real problem as the system frees up all memory on
  program exit.

DUMA_FILL - When set to a value between 0 and 255, every byte of allocated
  memory is initialized to that value. This can help detect reads of
  uninitialized memory. When set to -1, DUMA does not initialise memory on
  allocation. But some memory is filled with zeroes (the operating system
  default on most systems) and some memory will retain the values written to
  it during its last use.
  Per default DUMA will initialise all allocated bytes to 255 (=0xFF).
  To change this value, set DUMA_FILL in the shell environment to an
  integer value, or call the macro function DUMA_SET_FILL() from your
  code.

DUMA_SLACKFILL - As DUMA internally allocates memory in whole pages, there
  retains an unused and unprotectable piece of memory: the slack or no mans
  land. Per default DUMA will initialise this area to 170 (=0xAA), which
  is 10101010 in binary representation.
  To change this value, set DUMA_SLACKFILL in the shell environment to an
  integer value.
  DUMA automatically checks this area, the no mans land, at deallocation.
  You can manually induce a check with the macro function DUMA_CHECK() for
  one memory block. With the macro function DUMA_CHECKALL() all memory blocks
  get checked.

DUMA_CHECK_FREQ - First see DUMA_SLACKFILL abover for definition of no mans
  land. Checking the integrity of the no mans land costs performance. This is
  why this is usually done only at deallocation of a memory block. Set this
  variable to let DUMA check all memory blocks no mans land every <value>.th
  allocation or deallocation. Set this variable to 1, to let DUMA check at
  each allocation and deallocation.
  Per default the value 0 is used, which means to check only at deallocation.

DUMA_ALLOW_MALLOC_0 - Memory allocation of size zero is ANSI conform.  But
  often this is the result of a software bug. For this reason DUMA may trap
  such calls to malloc() with size zero. I leave this option disabled by
  default, but you are free to trap these calls setting the DUMA_ALLOC_MALLOC_0
  in the shell environment to an integer value.

DUMA_MALLOC_0_STRATEGY - This environment variable controls DUMA's behaviour
  on malloc(0):
    0 - like having former ALLOW_MALLOC_0 = 0  ==> abort program with segfault
    1 - return NULL pointer
    2 - return always the same pointer to some protected page
    3 - return mid address of a unique protected page (=default)
  ATTENTION: only 1 and 3 are ANSI conform. But value 1 will break most
  programs, cause value 3 strategy most system libraries use/implement.
  All returned pointers can be passed to free().

DUMA_NEW_0_STRATEGY - This environment variable controls DUMA's behaviour
  on C++ operator new with size zero:
    2 - return always the same pointer to some protected page
    3 - return mid address of a unique protected page (=default)
  ATTENTION: only 3 is standard conform. Value 2 may break some but will
  work for most programs. With value 2 you may reduce the memory consumption.

DUMA_MALLOC_FAILEXIT - Many programs do not check for allocation failure. This
  often leads to delayed errors, no more understandable. Set this variable to a
  positive integer in the shell environment to exit the program immediately
  when memory allocation fails. This option is set by default.

DUMA_PROTECT_FREE - DUMA usually returns free memory to a pool from which it
  may be re-allocated. If you suspect that a program may be touching free
  memory, set DUMA_PROTECT_FREE shell environment to -1. This is the default
  and will cause DUMA not to re-allocate any memory.
  For programs with many allocations and deallocations this may lead to the
  consumption of the full address space and thus to the failure of malloc().
  To avoid such failures you may limit the amount of protected deallocated
  memory by setting DUMA_PROTECT_FREE to a positive value. This value in kB
  will be the limit for such protected free memory.
  A value of 0 will disable protection of freed memory.

DUMA_MAX_ALLOC - This shell environment variable limits the total memory print
  of a program. This is another way to indirectly limit the sum of freed
  protected memory (see DUMA_PROTECT_FREE). By default there is no limit (=-1).
  A positive value is interpreted in kB, which stands for the sum of allocated
  and freed protected memory.

DUMA_FREE_ACCESS - This is a debugging enhancer to catch deallocation of a
  memory block using watch expressions. DUMA does a write access to the first
  byte, which may lead a debugger to stop on a watch expression. You have to
  enable this by setting the shell environment variable to non zero.
  Default is disabled.

DUMA_SHOW_ALLOC - Set this shell environment variable to non-zero to let DUMA
  print all allocations and deallocations to the console. Although this
  generates a lot of messages, this option can be useful to detect inefficient
  code containing many (de)allocations. This is switched off by default.

DUMA_SUPPRESS_ATEXIT - Set this shell environment variable to non-zero when
  DUMA should skip the installation of its exit handler. The exit handler is
  called at the end of the main program and checks for memory leaks, so the
  handler's installation should *usually* not be suppressed. One reason for
  doing so regardless are some buggy environments, where calls to the standard
  C library's atexit()-function hangs.

DUMA_DISABLE_BANNER - Set this shell environment variable to non-zero to
  suppress the usual startup message on console. Default is 0.

DUMA_OUTPUT_DEBUG - Set this shell environment variable to non-zero to output
  all DUMA messages to the debugging console. This option is only available
  on Windows and is off by default.

DUMA_OUTPUT_STDOUT - Set this shell environment variable to non-zero to output
  all DUMA messages to STDOUT. This option is off by default.

DUMA_OUTPUT_STDERR - Set this shell environment variable to non-zero to output
  all DUMA messages to STDERR. This option is on by default.

DUMA_OUTPUT_FILE - Set this shell environment variable to a filename where all
  DUMA messages should be written to. This option is off by default.

DUMA_OUTPUT_STACKTRACE - Set this shell environment variable to non-zero to
  output a stacktrace of the allocation that is not free'd. This option is
  available only on Windows and is off by default. This option also requires a map
  file generated by the linker.

DUMA_OUTPUT_STACKTRACE_MAPFILE - Set this shell environment variable to the map
  file, when it isn't found. This is very usefull when using detours version of
  DUMA. This option is available only on Windows.



WORD-ALIGNMENT AND OVERRUN DETECTION:


There is a conflict between the alignment restrictions that malloc() operates
under and the debugging strategy used by DUMA. When detecting overruns, DUMA
malloc() allocates two or more virtual memory pages for each allocation. The
last page is made inaccessible in such a way that any read, write, or execute
access will cause a segmentation fault. Then, DUMA malloc() will return an
address such that the first byte after the end of the allocation is on the
inaccessible page. Thus, any overrun of the allocation will cause a
segmentation fault.

It follows that the address returned by malloc() is the address of the
inaccessible page minus the size of the memory allocation. Unfortunately,
malloc() is required to return word-aligned allocations, since many CPUs can
only access a word when its address is aligned. The conflict happens when
software makes a memory allocation using a size that is not a multiple of the
word size, and expects to do word accesses to that allocation. The location of
the inaccessible page is fixed by hardware at a word-aligned address. If DUMA
malloc() is to return an aligned address, it must increase the size of the
allocation to a multiple of the word size.

In addition, the functions memalign() and valloc() must honor explicit
specifications on the alignment of the memory allocation, and this, as well can
only be implemented by increasing the size of the allocation. Thus, there will
be situations in which the end of a memory allocation contains some padding
space, and accesses of that padding space will not be detected, even if they
are overruns.

DUMA provides the variable DUMA_ALIGNMENT so that the user can control the
default alignment used by malloc(), calloc(), and realloc(). To debug overruns
as small as a single byte, you can set DUMA_ALIGNMENT to one. This will result
in DUMA malloc() returning unaligned addresses for allocations with sizes that
are not a multiple of the word size. This is not a problem in most cases,
because compilers must pad the size of objects so that alignment restrictions
are honored when storing those objects in arrays. The problem surfaces when
software allocates odd-sized buffers for objects that must be word-aligned. One
case of this is software that allocates a buffer to contain a structure and a
string, and the string has an odd size (this example was in a popular TIFF
library). If word references are made to un-aligned buffers, you will see a bus
error (SIGBUS) instead of a segmentation fault. The only way to fix this is to
re-write the offending code to make byte references or not make odd-sized
allocations, or to set DUMA_ALIGNMENT to the word size.

Another example of software incompatible with DUMA_ALIGNMENT < word-size
is the strcmp() function and other string functions on SunOS (and probably
Solaris), which make word-sized accesses to character strings, and may attempt
to access up to three bytes beyond the end of a string. These result in a
segmentation fault (SIGSEGV). The only way around this is to use versions of
the string functions that perform byte references instead of word references.


CATCHING THE ERRONEOUS LINE:


To get the line in your sources, where an error occurs, go as following:
1. Compile your program with debugging information
   and statically linked to DUMA.
2. Start your program from debugger f.e. with 'gdb <program>'
3. Set program environment variables like 'set environment DUMA_PROTECT_BELOW 1'
4. set your program arguments with 'set args ..'
5. Run and wait for the segmentation fault

alternatively

1. Compile your program (with debugging information) without DUMA.
2. Set 'ulimit -c unlimited' to get core files
3. Start your program, choose one of following options
   a) Start your program (linked statically with DUMA)
   b) Start your program with duma.sh <your_program>
4. Wait for a segmentation fault. this should have created a core[.<pid>]
   file. You can get into a debugger f.e. with 'gdb <program> -c <core file>'


INSTRUCTIONS FOR DEBUGGING YOUR PROGRAM:


1. Link with libduma.a as explained above.
2. Run your program in a debugger and fix any overruns or accesses
   to free memory.
3. Quit the debugger.
4. Set DUMA_PROTECT_BELOW = 1 in the shell environment.
5. Repeat step 2, this time repairing underruns if they occur.
6. Quit the debugger.
7. Read the restrictions in the section on

alternatively

read and install gdbinit.rc as ~/.gdbinit
if you are using a GNU gdb based debugger


WORD-ALIGNMENT AND OVERRUN DETECTION:


See if you can set DUMA_ALIGNMENT to 1 and repeat step 2. Sometimes this will
be too much work, or there will be problems with library routines for which you
don't have the source, that will prevent you from doing this.


MEMORY USAGE AND EXECUTION SPEED:


Since DUMA uses at least two virtual memory pages for each of its allocations,
it's a terrible memory hog. I've sometimes found it necessary to add a swap
file using swapon(8) so that the system would have enough virtual memory to
debug my program. Also, the way we manipulate memory results in various cache
and translation buffer entries being flushed with each call to malloc or free.
The end result is that your program will be much slower and use more resources
while you are debugging it with DUMA.

The Linux kernel also limits the number of different page mappings per process.
Have a look for
/proc/sys/vm/max_map_count
f.e. under
http://www.redhat.com/docs/manuals/enterprise/RHEL-4-Manual/en-US/Reference_Guide/s3-proc-sys-vm.html
You may have to increase this value to allow debugging with DUMA with a
command like:
"sudo sysctl -w vm.max_map_count=1000000"

Don't leave libduma.a linked into production software! Use it only for
debugging. See section 'COMPILATION NOTES FOR RELEASE/PRODUCTION' below.


MEMORY LEAK DETECTION:


All memory allocation is protocoled from DUMA together with the filename and
linenumber of the calling function. The atexit() function checks if each
allocated memory block was freed. To disable leak detection add the
preprocessor definition 'DUMA_SO_NO_LEAKDETECTION' or
'DUMA_LIB_NO_LEAKDETECTION' to DUMA_OPTIONS in Makefile.
If a leak is reported without source filename and line number but is
reproducible with the same pointer, set a conditional breakpoint on the
function 'void * duma_alloc_return( void * address)'
f.e. with gdb command 'break duma_alloc_return if address==0x123'


C++ MEMORY OPERATORS AND LEAK DETECTION:


Macros for "new" and "delete" are defined in dumapp.h. These macros give
filename and linenumber of the calling functions to DUMA, thus allowing the
same leak detection reports as for malloc and free. 'dumapp.h' needs to be
included from your source file(s).
For disabling the C++ new/delete/new[] and delete[] operators, add the
preprocessor definition 'DUMA_NO_CPP_SUPPORT' to DUMA_OPTIONS in Makefile.


DEFINITION OF OWN MEMBER NEW/DELETE OPERATORS:


Definition of own member new/delete operators for a class will fail cause the
new/delete keywords are defined as macros from DUMA. You will have to undefine
DUMA's macros with following line:

:#include "noduma.h"

Then you have to call DUMA's operators directly inside your own definition.

For using DUMA's C++ operators without having the preprocessor macros defined,
following syntax can be used:

(start code)
// const char * file  or  __FILE__ macro
// int          line  or  __LINE__ macro

ptr = new(file,line) type;          // scalar new throwing bad_alloc() on error
ptr = new(std::nothrow,file,line) type; // scalar new returning 0 on error
operator delete(ptr,file,line);     // scalar delete

ptr = new(file,line) type[n];       // vector new throwing bad_alloc() on error
ptr = new(std::nothrow,file,line) type[n];  // vector new returning 0 on error
operator delete[](ptr, file,line);  // vector delete
(end code)

The default syntax without file/line info can be used, too.


PREPACKAGED RPM FILES FOR REDHAT & CO:


You can download prepackaged .rpm files for RedHat, Fedora Core and similar
systems from
http://dries.ulyssis.org/apt/packages/duma/info.html

Dries Verachtert <dries@ulyssis.org> wrote the .spec file.


COMPILATION NOTES FOR VISUAL C++:


Here some Compilation Notes for your Application in Debug Mode
for the Microsoft Visual C++ (v6) Compiler:

1) Don't use Program Database for "Edit and Continue":
   Project-Options -> C++ -> General -> Debug-Info
2) Don't use the "multithreaded-dll" runtime library:
   Project-Options -> C++ -> Code Generation -> Runtime library
3) Switch off incremental linking
   Project-Options -> Linker -> General
4) Switch off precompiled headers:
   Project-Options -> C++ -> Precompiled Headers
5) Add following Linker option "/FORCE:MULTIPLE"
   Project-Options -> Linker -> General

The labels may differ a bit cause i'm using the german version:

Options in german language:
1) Projekteinstellungen -> C++ -> Kategorie =Allgemein
   -> Debug-Info =Programmdatenbank
2) Projekteinstellungen -> C++ -> Kategorie =Codegeneration
   -> Laufzeitbibliothek anpassen (Release/Debug),
   nicht die DLL-Variante verwenden
3) Projekteinstellungen -> Linker -> Kategorie =Allgemein
   - Inkrementelles Binden =Aus
4) Projekteinstellungen -> Linker -> Projekt Optionen
   "/FORCE:MULTIPLE" unten eingeben

Now everything you have to do is to set a dependency to "duma" from your
application.


COMPILATION NOTES FOR RELEASE/PRODUCTION:


Set the preprocessor definition

#define DUMA_NO_DUMA

in your Makefiles to disable DUMA usage and don't link with DUMA library.
With DUMA_NO_DUMA-definition all DUMA macro functions get defined but do
nothing. This way you don't have to change your code for release compilation
even when using special DUMA macros.


WARNINGS:


I have tried to do as good a job as I can on this software, but I doubt that it
is even theoretically possible to make it bug-free. This software has no
warranty. It will not detect some bugs that you might expect it to detect, and
will indicate that some non-bugs are bugs.


FILES:


/dev/zero: Source of memory pages (via mmap(2)).


SEE ALSO:


malloc(3), mmap(2), mprotect(2), swapon(8)


DIAGNOSTICS:


Segmentation Fault: Examine the offending statement for violation of the
boundaries of a memory allocation.

Bus Error: See the section on WORD-ALIGNMENT AND OVERRUN DETECTION in this
manual page.


BUGS:


My explanation of the alignment issue could be improved.
Some Sun systems running SunOS 4.1 were reported to signal an access to a
protected page with 'SIGBUS' rather than 'SIGSEGV', I suspect this is an
undocumented feature of a particular Sun hardware version, not just the
operating system. On these systems, dumatest will fail with a bus error until
you modify the Makefile to define PAGE_PROTECTION_VIOLATED_SIGNAL as SIGBUS.

There are, without doubt, other bugs and porting issues. Please contact me via
e-mail if you have any bug reports, ideas, etc.


OTHER ALTERNATIVE/ADDITIONAL DEBUGGING SOFTWARE/TOOLS:


GCC
option -Warray-bounds up from gcc 4.3
options -fmudflap -fmudflapth -fmudflapir up from gcc 4.0
See http://gcc.gnu.org/
See http://gcc.gnu.org/wiki/Mudflap_Pointer_Debugging

IBM Stack Smashing Protector aka Pro Police
it is a GCC (Gnu Compiler Collection) extension for protecting applications
from stack-smashing attacks. Applications written in C will be protected by
the method that automatically inserts protection code into an application at
compilation time. The protection is realized by buffer overflow detection and
the variable reordering feature to avoid the corruption of pointers. The basic
idea of buffer overflow detection comes from StackGuard system.
See http://www.trl.ibm.com/projects/security/ssp/

Checkergcc
a modified version of the GNU C Compiler that instruments all memory
references, is available on Linux systems and where GCC is used. It performs
some of the same tasks as Purify, but only on code that it has compiled.

Valgrind
Valgrind is an award-winning suite of tools for debugging and profiling Linux
programs. With the tools that come with Valgrind, you can automatically detect
many memory management and threading bugs, avoiding hours of frustrating
bug-hunting, making your programs more stable. You can also perform detailed
profiling, to speed up and reduce memory use of your programs.
The Valgrind distribution currently includes four tools: a memory error
detector, a cache (time) profiler, a call-graph profiler, and a heap (space)
profiler. It runs on the following platforms: X86/Linux, AMD64/Linux,
PPC32/Linux, PPC64/Linux.
See http://valgrind.org/

Pageheap.exe
Another alternative to using duma on Windows (XP, 2000, Server 2003) is to use
the built in heap debugging provided by Microsoft. It's not as feature rich as
duma, but for complex projects were duma is causing issues it will work with less
hassle (it sits in the heap manager itself).
You can control it using the global flags (gflags) utility that comes with windbg.
You can enable it by saying: gflags.exe /p /full /enable MyProgram.exe
And do unaligned by saying:  gflags.exe /p /full /unaligned /enable MyProgram.exe
gflags will set specific registry keys to enable the pageheap debugging on the
executable. You can disable it using the "/disable" flag.
See http://support.microsoft.com/kb/286470

MPATROL
The mpatrol library is a powerful debugging tool that attempts to diagnose
run-time errors that are caused by the wrong use of dynamically allocated
memory. It acts as a malloc() debugger for debugging dynamic memory
allocations, although it can also trace and profile calls to malloc()
and free() too.
See http://www.cbmamiga.demon.co.uk/mpatrol/

Purify
does a much more thorough job than DUMA, and does not have the huge memory
overhead.

LibSafe
protects Critical Elements of Stack.
See http://www.research.avayalabs.com/

DieHard
helps buggy programs run correctly and protects them from a range of security
vulnerabilities.
See http://www.diehard-software.org/

electric-fence-win32
another Win32 port of Electric Fence.
See http://code.google.com/p/electric-fence-win32/


FURTHER READING:

Hunting Memory Bugs
http://www.edm2.com/0508/membug.html

An OS/2 Allocator for the STL
http://www.edm2.com/0508/stl.html

Wikipedia
http://en.wikipedia.org/wiki/Memory_debugger



CONTACTING THE AUTHOR:


Hayati Ayguen <h_ayguen@web.de>
Michael Eddington <meddington@gmail.com>

http://duma.sf.net


FILES IN PACKAGE:


SubDirectories:
---------------
win32-vide/*    project files for VIDE 1.24 (see http://www.objectcentral.com)
                using the Borland C++ Builder 5.5 compiler
                (FreeCommandLineTools, see http://www.borland.com)
win32-devcpp/*  project files for Dev-C++ 4.9.6 (see http://www.bloodshed.net)
                using the gcc compiler (see http://gcc.gnu.org)
win32-msvc/*    projects files for Microsoft Visual C++ 6.0 IDE/compiler
                (see http://www.microsoft.com)
debian/*        don't know; maybe some files for the Debian Linux distribution?

Projects:
---------
dumalib         DUMA library. this library should be linked with YOUR program
dumatest        first small test program
tstheap         second small test program

Files:
------
COPYING-*       License files; reade carefully!
README          this text file
CHANGES         text file listing done CHANGES

duma.h          belongs to dumalib
                this header file should be included from within YOUR C source
dumapp.h        belongs to dumalib
                this header file should be included from within YOUR C++ source
duma.c          belongs to dumalib
                contains malloc/free/.. functions
dumapp.cpp      belongs to dumalib
                contains C++ new/delete/.. functions redirecting them
                  to ANSI C malloc/free
page.c          belongs to dumalib
                library internal source file: contains paging functions
print.c         belongs to dumalib; library internal source file: contains
                  printing/aborting functions
dumatest.c      belongs to dumatest
                small test program; checks wether dumalib's paging does its job
                should work without any errors
tstheap.c       belongs to tstheap
                small test program; checks wether dumalib's heap does its job
                should report many memory leaks after execution.
Makefile        Makefile for UNIX/Linux/..
duma.3          source for UNIX man page
duma.sh         script for UNIX/Linux to start other programs using the
                  LD_PRELOAD mechanism


LICENSE:


Copyright (C) 2006 Michael Eddington <meddington@gmail.com>
Copyright (C) 2002-2008 Hayati Ayguen <h_ayguen@web.de>, Procitec GmbH
Copyright (C) 1987-1999 Bruce Perens <bruce@perens.com>
License: GNU GPL (GNU General Public License, see COPYING-GPL) for all files except dumapp.h
License: GNU LGPL (GNU Lesser General Public License, see COPYING-GPL) for dumapp.h

--- GPL --

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

--- LGPL --

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

