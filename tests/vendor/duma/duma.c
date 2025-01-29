
/*
 * DUMA - Red-Zone memory allocator.
 * Copyright (C) 2006 Michael Eddington <meddington@gmail.com>
 * Copyright (C) 2002-2009 Hayati Ayguen <h_ayguen@web.de>, Procitec GmbH
 * Copyright (C) 1987-1999 Bruce Perens <bruce@perens.com>
 * License: GNU GPL (GNU General Public License, see COPYING-GPL)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * FILE CONTENTS:
 * --------------
 * This is a special version of malloc() and company for debugging software
 * that is suspected of overrunning or underrunning the boundaries of a
 * malloc buffer, or touching free memory.
 *
 * It arranges for each malloc buffer to be followed (or preceded)
 * in the address space by an inaccessable virtual memory page,
 * and for free memory to be inaccessable. If software touches the
 * inaccessable page, it will get an immediate segmentation
 * fault. It is then trivial to uncover the offending code using a debugger.
 *
 * An advantage of this product over most malloc debuggers is that this one
 * detects reading out of bounds as well as writing, and this one stops on
 * the exact instruction that causes the error, rather than waiting until the
 * next boundary check.
 *
 * There is one product that debugs malloc buffer overruns
 * better than DUMA: "Purify" from Purify Systems, and that's only
 * a small part of what Purify does. I'm not affiliated with Purify, I just
 * respect a job well done.
 *
 * This version of malloc() should not be linked into production software,
 * since it tremendously increases the time and memory overhead of malloc().
 * Each malloc buffer will consume a minimum of two virtual memory pages,
 * this is 16 kilobytes on many systems. On some systems it will be necessary
 * to increase the amount of swap space in order to debug large programs that
 * perform lots of allocation, because of the per-buffer overhead.
 */

#ifndef DUMA_NO_DUMA

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <string.h>
extern char *strdup(const char *s);
#include <signal.h>
#include <errno.h>
#include <stdarg.h>

#ifndef WIN32
  #include <unistd.h>
  #include <fcntl.h>
  #include <sys/mman.h>
  #define __USE_BSD  // necessary for caddr_t on Linux
  #define __USE_MISC  // necessary for caddr_t on Linux
  #include <sys/types.h>
#else
  #define WIN32_LEAN_AND_MEAN 1
  #include <windows.h>
  #include <winbase.h>
  #include <io.h>

#ifndef __CYGWIN__
  /* already defined in cygwin headers */
  typedef LPVOID caddr_t;
  typedef unsigned u_int;
#endif

#endif

#ifdef _MSC_VER
  #include <crtdbg.h>
#endif

#include "duma.h"
#include "noduma.h"
#include "print.h"
#include "duma_sem.h"
#include "paging.h"

#if defined(WIN32) && !defined(__CYGWIN__) && !defined(__MINGW32__) && !defined(__MINGW64__)
DUMA_EXTERN_C void StackTraceCleanup();
DUMA_EXTERN_C void printStackTrace(char* buffer, int bufferSize, char* mapFilename);
#endif

static const char  version[] =
"DUMA 2.5.15 ("
#ifdef DUMA_SO_LIBRARY
"shared library"
#elif DUMA_DLL_LIBRARY
"DLL library"
#elif DUMA_DETOURS
"detours"
#else
"static library"
#endif
#ifdef DUMA_NO_GLOBAL_MALLOC_FREE
", NO_GLOBAL_MALLOC_FREE"
#endif
#ifdef DUMA_NO_THREAD_SAFETY
", NO_THREAD_SAFETY"
#endif
#ifdef DUMA_NO_CPP_SUPPORT
", NO_CPP_SUPPORT"
#endif
#ifdef DUMA_NO_LEAKDETECTION
", NO_LEAKDETECTION"
#endif
#ifdef DUMA_EXPLICIT_INIT
", EXPLICIT_INIT"
#endif
#ifdef DUMA_PREFER_ATEXIT
", PREFER_ATEXIT"
#endif
#ifdef DUMA_PREFER_GETENV
", PREFER_GETENV"
#endif
")\n"
"Copyright (C) 2006 Michael Eddington <meddington@gmail.com>\n"
"Copyright (C) 2002-2008 Hayati Ayguen <h_ayguen@web.de>, Procitec GmbH\n"
"Copyright (C) 1987-1999 Bruce Perens <bruce@perens.com>\n\n";


static const char unknown_file[] =
  "UNKNOWN (use #include \"duma.h\")";


#ifndef DUMA_NO_LEAKDETECTION
#define DUMA_PARAMLIST_FL       , const char * filename, int lineno
#define DUMA_PARAMS_FL          , filename, lineno
#define DUMA_PARAMS_UK          , unknown_file, 0
#else
#define DUMA_PARAMLIST_FL
#define DUMA_PARAMS_FL
#define DUMA_PARAMS_UK
#endif

#ifndef DUMA_PREFER_GETENV
#define DUMA_GETENV     duma_getenv
#else
#define DUMA_GETENV     getenv
#endif


/* Variable: MEMORY_CREATION_SIZE
 *
 * MEMORY_CREATION_SIZE is the amount of memory to get from the operating
 * system at one time. We'll break that memory down into smaller pieces for
 * malloc buffers. One megabyte is probably a good value.
 */
#define      MEMORY_CREATION_SIZE  1024 * 1024

/* Enum: _DUMA_SlotState
 *
 * State of slot values (empty, free, etc)
 */
enum _DUMA_SlotState
{
    DUMAST_EMPTY            /* slot not in use */
  , DUMAST_FREE             /* internal memory reserved, unused by user */
  , DUMAST_IN_USE           /* memory in use by allocator; see following enum AllocType */
  , DUMAST_ALL_PROTECTED    /* memory no more used by allocator; memory is not deallocated but protected */
  , DUMAST_BEGIN_PROTECTED  /* most memory deallocated, but not page covering userAddress:
                             * slot holds userAddress, userSize and allocator.
                             */
};

enum _DUMA_Slot_FileSource
{
    DUMAFS_EMPTY            /* no filename, lineno */
  , DUMAFS_ALLOCATION       /* filename, lineno from allocation */
  , DUMAFS_DEALLOCATION     /* filename, lineno from deallocation */
};

enum _DUMA_InitState
{
    DUMAIS_UNINITIALIZED = 0x1611  /* not initialized */
  , DUMAIS_IN_CONSTRUCTOR   /* in constructor _duma_init() */
  , DUMAIS_OUT_CONSTRUCTOR  /* construction _duma_init() finished */
  , DUMAIS_IN_INIT          /* in initializer duma_init() */
  , DUMAIS_OUT_INIT         /* initialization duma_init() finished */
};

/*
 * Struct Slot contains all of the information about a malloc buffer except
 * for the contents of its memory.
 */
struct _DUMA_Slot
{
  void            * internalAddress;
  void            * userAddress;
  void            * protAddress;
  size_t            internalSize;
  size_t            userSize;

#if 0
  /* just for checking compiler warnings / errors */
  enum _DUMA_SlotState        state;
  enum _DUMA_Allocator        allocator;
  #ifndef DUMA_NO_LEAKDETECTION
  enum _DUMA_Slot_FileSource  fileSource;
  #endif
#else
  /* save (some) space in production */
  unsigned short    state       :16;
  #ifdef DUMA_NO_LEAKDETECTION
  unsigned short    allocator   :16;
  #else
  unsigned short    allocator   :8;
  unsigned short    fileSource  :8;
  #endif
#endif

#ifndef DUMA_NO_LEAKDETECTION
  char            * filename;   /* filename of allocation */
  int               lineno;     /* linenumber of allocation
                                 * -1 == memory was allocated before duma_init()
                                 *  0 == no leak information present
                                 * >0 == leak information present
                                 */
#endif

/* Feature currently only works on win32 */
#if defined(WIN32) && !defined(__CYGWIN__) && !defined(__MINGW32__) && !defined(__MINGW64__)
  char            * stacktrace; /* stacktrace of allocation */
#endif

#ifdef DUMA_EXPLICIT_INIT
  int               slackfill;
#endif
};

enum _DUMA_AllocType
{
    DUMAAT_INTERNAL
  , DUMAAT_MALLOC
  , DUMAAT_NEW_ELEM
  , DUMAAT_NEW_ARRAY
  , DUMAAT_MEMBER_NEW_ELEM
  , DUMAAT_MEMBER_NEW_ARRAY
};

enum _DUMA_AllocStd
{
    DUMAAS_C
  , DUMAAS_CPP
};

static const struct _DUMA_AllocDesc
{
  char                * name;
  enum _DUMA_AllocType  type;
  enum _DUMA_AllocStd   std;
}
_duma_allocDesc[] =
{
    { "duma allocate()"       , DUMAAT_INTERNAL        , DUMAAS_C   }
  , { "duma deallocate()"     , DUMAAT_INTERNAL        , DUMAAS_C   }
  , { "malloc()"              , DUMAAT_MALLOC          , DUMAAS_C   }
  , { "calloc()"              , DUMAAT_MALLOC          , DUMAAS_C   }
  , { "free()"                , DUMAAT_MALLOC          , DUMAAS_C   }
  , { "memalign()"            , DUMAAT_MALLOC          , DUMAAS_C   }
  , { "posix_memalign()"      , DUMAAT_MALLOC          , DUMAAS_C   }
  , { "realloc()"             , DUMAAT_MALLOC          , DUMAAS_C   }
  , { "valloc()"              , DUMAAT_MALLOC          , DUMAAS_C   }
  , { "strdup()"              , DUMAAT_MALLOC          , DUMAAS_C   }
  , { "scalar new"            , DUMAAT_NEW_ELEM        , DUMAAS_CPP }
  , { "scalar delete"         , DUMAAT_NEW_ELEM        , DUMAAS_CPP }
  , { "vector new[]"          , DUMAAT_NEW_ARRAY       , DUMAAS_CPP }
  , { "vector delete[]"       , DUMAAT_NEW_ARRAY       , DUMAAS_CPP }
  , { "member scalar new"     , DUMAAT_MEMBER_NEW_ELEM , DUMAAS_CPP }
  , { "member scalar delete"  , DUMAAT_MEMBER_NEW_ELEM , DUMAAS_CPP }
  , { "member vector new[]"   , DUMAAT_MEMBER_NEW_ARRAY, DUMAAS_CPP }
  , { "member vector delete[]", DUMAAT_MEMBER_NEW_ARRAY, DUMAAS_CPP }
};

#ifdef DUMA_EXPLICIT_INIT
#define IF__DUMA_INIT_DONE if (DUMAIS_OUT_INIT == _duma_s.init_state)
#else
#define IF__DUMA_INIT_DONE
#endif


/* Collection of all global static non const variables for DUMA */

static struct _DUMA_GlobalStaticVars
{
  /* Protection Space A */
  char  acSpaceA[2 * DUMA_PAGE_SIZE];

  int   DUMA_IN_DUMA;

  /* Variable: DUMA_DISABLE_BANNER
   *
   * DUMA_DISABLE_BANNER is a global variable used to control whether DUMA prints
   * its usual startup message. Default is 0, meaning that the startup message
   * gets printed.
   */
  int   DISABLE_BANNER;

  /* Variable: DUMA_SKIPCOUNT_INIT
   *
   * DUMA_SKIPCOUNT_INIT controls after how many DUMA allocations the full internal
   * initialization is done. Default is 0.
   */
  int   SKIPCOUNT_INIT;


  /* Variable: CHECK_FREQ
   *
   * DUMA_CHECK_FREQ controls the frequency to check all memory blocks no man's land.
   * The frequency counter is incremented at each memory allocation and deallocation.
   * Whenever the counter reaches the value of DUMA_CHECK_FREQ the check is performed.
   * 0 means no checks. 1 means to check always. Be careful with this value, it may
   * get very time consuming.
   * Default is 0.
   */
  int   CHECK_FREQ;


  /* Variable: DUMA_REPORT_ALL_LEAKS
   *
   * DUMA_REPORT_ALL_LEAKS is a global variable used to control whether DUMA should
   * all leaks - even without source filename/line number. Default is 0, meaning that
   * only leaks with source information will get reported.
   */
  int   REPORT_ALL_LEAKS;

  /* Variable: DUMA_SLACKFILL
   *
   * DUMA_SLACKFILL is set to 0-255. The slack / no mans land of all new allocated
   * memory is filled with the specified value.
   * default is set to initialise with 0xAA (=binary 10101010)
   * initialisation to 0!
   */
  int   SLACKFILL;

  /* Variable: DUMA_PROTECT_FREE
   *
   * DUMA_PROTECT_FREE is used to control the disposition of memory that is
   * released using free(). It is all-caps so that its name
   * matches the name of the environment variable that is used to set it.
   * If its value is non-zero, memory released by free is made inaccessable.
   * Any software that touches free memory will then get a segmentation fault.
   * Depending on your application and your resources you may tell
   * DUMA not to use this memory ever again by setting a negative
   * value f.e. -1.
   * You can tell DUMA to limit the sum of protected memory by setting
   * a positive value, which is interpreted in kB.
   * If its value is zero, freed memory will be available for reallocation,
   * but will still be inaccessable until it is reallocated.
   */
  long  PROTECT_FREE;

  /* Variable: DUMA_MAX_ALLOC
   *
   * DUMA_MAX_ALLOC is used to control the maximum memory print of the program
   * in total: When the sum of allocated and protected memory would exceed
   * this value in kB, the protected memory is freed/deleted.
   */
  long  MAX_ALLOC;

#if 0
  /* Variable: DUMA_ALLOW_MALLOC_0
   *
   * DUMA_ALLOW_MALLOC_0 is set if DUMA is to allow malloc(0). I
   * trap malloc(0) by default because it is a common source of bugs.
   * But you should know the allocation with size 0 is ANSI conform.
   */
  int   ALLOW_MALLOC_0;
#endif

  /* Variable: DUMA_MALLOC_0_STRATEGY
   *
   * DUMA_MALLOC_0_STRATEGY how DUMA should behave on malloc(0).
   *   0 - like having former ALLOW_MALLOC_0 = 0  ==> abort program with segfault
   *   1 - return NULL pointer
   *   2 - return always the same pointer to some protected page
   *   3 - return unique protected page (=default)
   * ATTENTION: only 1 and 3 are ANSI conform. But value 1 will break most programs,
   *   cause value 3 is the usual one, the system libraries implement
   */
  int   MALLOC_0_STRATEGY;

  /* Variable: DUMA_NEW_0_STRATEGY
   *
   * DUMA_NEW_0_STRATEGY how DUMA should behave on C++ operator new or new[]
   *   with size 0
   *   2 - return always the same pointer to some protected page
   *   3 - return unique protected page (=default)
   * ATTENTION: only 3 is standard conform. Value 2 may break some but will
   * work for most programs. With value 2 you may reduce the memory consumption.
   */
  int   NEW_0_STRATEGY;

  /* Variable: DUMA_MALLOC_FAILEXIT
   *
   * DUMA_MALLOC_FAILEXIT controls the behaviour of DUMA when
   * malloc() fails and would return NULL. But most applications don't
   * check the return value for errors ... so
   * default to Exit on Fail
   */
  int   MALLOC_FAILEXIT;

  /* Variable: DUMA_FREE_ACCESS
   *
   * DUMA_FREE_ACCESS is set if DUMA is to write access memory before
   * freeing it. This makes easier using watch expressions in debuggers as the
   * process is interrupted even if the memory is going to be freed.
   */
  int   FREE_ACCESS;

  /* Variable: DUMA_SHOW_ALLOC
   *
   * DUMA_SHOW_ALLOC is set if DUMA is to print all allocations
   * and deallocations to the console. Although this generates a lot
   * of messages, the option can be useful to detect inefficient code
   * containing many allocations / deallocations
   */
  int   SHOW_ALLOC;

  /* Variable: DUMA_SUPPRESS_ATEXIT
   *
   * DUMA_SUPPRESS_ATEXIT is set if DUMA is to suppress the installation of
   * an exit handler, called at the exit of the main program. This handler allows for
   * the detection of memory areas that have not been freed correctly before
   * program exit, so the handler's installation should *normally* not be
   * suppressed. One reason for doing so regardless are some buggy environments,
   * where calls to the atexit()-function hang.
   */
  int   SUPPRESS_ATEXIT;


  /* Variable: _duma_allocListSize
   *
   * _duma_allocListSize is the size of the allocation list. This will always
   * be a multiple of the page size.
   */
  size_t  allocListSize;

  /* Variable: slotCount
   *
   * slotCount is the number of Slot structures in allocationList.
   */
  size_t  slotCount;

  /* Variable: unUsedSlots
   *
   * unUsedSlots is the number of Slot structures that are currently available
   * to represent new malloc buffers. When this number gets too low, we will
   * create new slots.
   */
  size_t  unUsedSlots;

  /* Variable: slotsPerPage
   *
   * slotsPerPage is the number of slot structures that fit in a virtual
   * memory page.
   */
  size_t  slotsPerPage;

  /* Variable: sumAllocatedMem
   *
   * internal variable: sum of allocated -freed +protected memory in kB
   */
  long    sumAllocatedMem;

  /* Variable: sumTotalAllocatedMem
   *
   * internal variable: sum of allocated memory in kB
   */
  long    sumTotalAllocatedMem;

  /* Variable: sumProtectedMem
   *
   * internal variable: sum of protected memory in kB
   */
  long    sumProtectedMem;

  /* Variable: numDeallocs
   *
   * internal variable: number of deallocations processed so far
   */
  long    numDeallocs;

  /* Variable: numAllocs
   *
   * internal variable: number of allocations processed so far
   */
  long    numAllocs;


  /* Variable checkFreqCounter
   *
   * number of (de)allocations since last checks
   */
  int     checkFreqCounter;

  /* Variable: duma_init_state
   *
   * internal variable: state of initialization
   */
  enum _DUMA_InitState  init_state;

  /* memory block for malloc() or new with size 0 */
  void *  null_block;

  /* Protection Space B */
  char  acSpaceB[2 * DUMA_PAGE_SIZE];
}

_duma_s =

{
   "Static Protection Space Front"   /* Protection Space A */

  , 0       /* int DUMA_IN_DUMA; */

  , 0       /* Variable: DISABLE_BANNER */
  , 0       /* Variable: SKIPCOUNT_INIT */
  , 1       /* Variable: CHECK_FREQ */
  , 0       /* Variable: REPORT_ALL_LEAKS */
  , 0xDB    /* Variable: SLACKFILL */
  , -1L     /* Variable: PROTECT_FREE */
  , -1L     /* Variable: MAX_ALLOC */
#if 0
  , 1       /* Variable: ALLOW_MALLOC_0 */
#endif
  , 3       /* Variable: MALLOC_0_STRATEGY; see above */
  , 3       /* Variable: NEW_0_STRATEGY; see above */
  , 1       /* Variable: MALLOC_FAILEXIT */
  , 0       /* Variable: FREE_ACCESS */
  , 0       /* Variable: SHOW_ALLOC */
  , 0       /* Variable: SUPPRESS_ATEXIT */

  , 0       /* Variable: allocListSize */
  , 0       /* Variable: slotCount */
  , 0       /* Variable: unUsedSlots */
  , 0       /* Variable: slotsPerPage */
  , 0L      /* Variable: sumAllocatedMem */
  , 0L      /* Variable: sumTotalAllocatedMem */
  , 0L      /* Variable: sumProtectedMem */
  , 0L      /* Variable: numDeallocs */
  , 0L      /* Variable: numAllocs */
  , 0       /* Variable: checkFreqCounter */
  , DUMAIS_UNINITIALIZED  /* Variable: duma_init_done */
  , (void *)0 /* Variable: null_block */

  , "Static Protection Space Back"   /* Protection Space B */
};



DUMA_GLOBALVARS_T _duma_g =
{
   "Global Protection Space Front"   /* Protection Space A */

  , (void*)0    /* Variable: allocList */
  , (void*)0    /* Variable: null_addr */

  , {   DUMA_MIN_ALIGNMENT
      , 0     /* PROTECT_BELOW */
      , 255   /* FILL */
      #if !defined(DUMA_NO_CPP_SUPPORT) && !defined(DUMA_NO_LEAKDETECTION)
        , 0     /* Magic */
        , 0     /* DelPtr */
        , { 0 }
        , { 0 }
      #endif
    }

  , "Global Protection Space Back"   /* Protection Space B */
};



/* Variable: DUMA_OUTPUT_DEBUG
 *
 * DUMA_OUTPUT_DEBUG is a global variable used to control if DUMA
 * output is printed to the win32 debugging console.  Default is 0,
 * meaning that output is not by default sent to the debugging console.
 *
 * OS: WIN32 Only
 */
int DUMA_OUTPUT_DEBUG = 0;

/* Variable: DUMA_OUTPUT_STDOUT
 *
 * DUMA_OUTPUT_STDOUT is a global variable used to control if DUMA
 * output is printed to STDOUT.  Default is 0,
 * meaning that output is not by default sent to STDOUT.
 */
int DUMA_OUTPUT_STDOUT = 0;

/* Variable: DUMA_OUTPUT_STDERR
 *
 * DUMA_OUTPUT_STDERR is a global variable used to control if DUMA
 * output is printed to STDERR.  Default is 1,
 * meaning that output is by default sent to STDERR.
 */
int DUMA_OUTPUT_STDERR = 1;

/* Variable: DUMA_OUTPUT_FILE
 *
 * DUMA_OUTPUT_FILE is a global variable used to control if DUMA
 * output is printed to a specified file.  Default is NULL,
 * meaning that output is not by default sent to a file.
 */
char* DUMA_OUTPUT_FILE = NULL;
/* char* DUMA_OUTPUT_FILE = "c:\\duma.txt"; */

/* Variable: DUMA_OUTPUT_STACKTRACE
 *
 * DUMA_OUTPUT_STACKTRACE is a global variable used to control if DUMA
 * outputs a stacktrace of the allocation that is not free'd. Default is 0,
 * meaning that this option is disabled.
 *
 * OS: WIN32 Only
 */
int DUMA_OUTPUT_STACKTRACE = 0;

/* Variable: DUMA_OUTPUT_STACKTRACE_MAPFILE
 *
 * DUMA_OUTPUT_STACKTRACE_MAPFILE is a global variable used to control
 * what mapfile is used for stack traces.  This is needed when using
 * detours and duma.  Default is NULL, indicating the system will try
 * and guess.
 *
 * OS: WIN32 Only
 */
char* DUMA_OUTPUT_STACKTRACE_MAPFILE = NULL;



/*
 * include helper functions
 */
#include "duma_hlp.h"

#ifndef DUMA_NO_LEAKDETECTION
/*
 * declare exit function
 */
void
#if ( defined(DUMA_GNU_INIT_ATTR) && !defined(DUMA_PREFER_ATEXIT) )
__attribute ((destructor))
#endif
_duma_exit(void);
#endif

// Avoid pulling in stubs_passert.h here, causes too many issues
// define a weak fallback for code that doesn't pull in clar
__attribute__((__weak__)) void passert_failed(
    const char* filename, int line_number, const char* message, ...) {
  va_list ap;
  va_start(ap, message);
  printf("*** ASSERTION FAILED: %s:%u\n", filename, line_number);
  if (message) {
    vprintf(message, ap);
  }
  va_end(ap);
  printf("\n");
  abort();
}

/* Function: _duma_assert
 *
 * Print message and halt program execution in crazy way.
 */
void _duma_assert(const char * exprstr, const char * filename, int lineno)
{
  passert_failed(filename, lineno, "\nDUMA: DUMA_ASSERT(%s) failed)\n", exprstr);
}

#ifndef DUMA_PREFER_GETENV

extern char **environ;

/* Function: duma_getenv
 *
 * replacement for standard C library function
 */
static
const char * duma_getenv( const char * varname )
{
  const char * ret = NULL;
  int varno = 0;

  if ( !varname )
    return ret;

  if ( varname[0] == '\0' )
    return ret;

  while ( environ[varno] )
  {
    const char * v = environ[varno++];
    int idx = 0;

    while ( varname[idx] != '\0' && v[idx] == varname[idx] )
      ++idx;

    if ( idx > 0 && varname[idx] == '\0' && v[idx] == '=' )
      return v + (idx +1);
  }
  return ret;
}

#endif


static
void duma_getenvvars( DUMA_TLSVARS_T * duma_tls )
{
  const char * string;

  /*
   * Import the user's environment specification of the default
   * alignment for malloc(). We want that alignment to be under
   * user control, since smaller alignment lets us catch more bugs,
   * however some software will break if malloc() returns a buffer
   * that is not word-aligned.
   *
   * I would like
   * alignment to be zero so that we could catch all one-byte
   * overruns, however if malloc() is asked to allocate an odd-size
   * buffer and returns an address that is not word-aligned, or whose
   * size is not a multiple of the word size, software breaks.
   * This was the case with the Sun string-handling routines,
   * which can do word fetches up to three bytes beyond the end of a
   * string. I handle this problem in part by providing
   * byte-reference-only versions of the string library functions, but
   * there are other functions that break, too. Some in X Windows, one
   * in Sam Leffler's TIFF library, and doubtless many others.
   */
  if ( (string = DUMA_GETENV("DUMA_ALIGNMENT")) != 0 )
  {
    duma_tls->ALIGNMENT = (size_t)atoi(string);
    /* we could check for DUMA_MIN_ALIGNMENT. should we do so? */
    if (!duma_tls->ALIGNMENT)
      duma_tls->ALIGNMENT = 1;
  }

  /*
   * See if the user wants to protect the address space below a buffer,
   * rather than that above a buffer.
   */
  if ( (string = DUMA_GETENV("DUMA_PROTECT_BELOW")) != 0 )
    duma_tls->PROTECT_BELOW = (atoi(string) != 0);

  /* Should we report all leaks? */
  if ( (string = DUMA_GETENV("DUMA_REPORT_ALL_LEAKS")) != 0 )
    _duma_s.REPORT_ALL_LEAKS = (atoi(string) != 0);

  /*
   * See if the user wants to protect memory that has been freed until
   * the program exits, rather than until it is re-allocated.
   * =-1 protect as much free'd memory as possible
   * =0 do not protect free'd memory
   * =N protect memory up to N kB
   */
  if ( (string = DUMA_GETENV("DUMA_PROTECT_FREE")) != 0 )
    _duma_s.PROTECT_FREE = atol(string);

  /*
   * See if the user has a memory usage limit. This controls the maximum
   * memory print of the program in total: The sum of allocated and protected
   * memory won't exceed this limit.
   * =-1 use as much memory as possible
   * =N limit total memory usage to N kB
   */
  if ( (string = DUMA_GETENV("DUMA_MAX_ALLOC")) != 0 )
    _duma_s.MAX_ALLOC = atol(string);

#if 0
  /*
   * See if the user wants to allow malloc(0).
   */
  if ( (string = DUMA_GETENV("DUMA_ALLOW_MALLOC_0")) != 0 )
    _duma_s.ALLOW_MALLOC_0 = (atoi(string) != 0);
#endif

  /*
   * See what strategy the user wants for malloc(0).
   */
  if ( (string = DUMA_GETENV("MALLOC_0_STRATEGY")) != 0 )
  {
    int tmp = atoi(string);
    if ( tmp >= 0 && tmp <= 3 )
      _duma_s.MALLOC_0_STRATEGY = tmp;
  }

  /*
   * See what strategy the user wants for C++ operator new with size zero.
   */
  if ( (string = DUMA_GETENV("NEW_0_STRATEGY")) != 0 )
  {
    int tmp = atoi(string);
    if ( tmp >= 2 && tmp <= 3 )
      _duma_s.NEW_0_STRATEGY = tmp;
  }

  /*
   * See if the user wants to exit on malloc() failure
   */
  if ( (string = DUMA_GETENV("DUMA_MALLOC_FAILEXIT")) != 0 )
    _duma_s.MALLOC_FAILEXIT = (atoi(string) != 0);

  /*
   * See if the user wants to write access freed memory
   */
  if ( (string = DUMA_GETENV("DUMA_FREE_ACCESS")) != 0 )
    _duma_s.FREE_ACCESS = (atoi(string) != 0);

  /*
   * Check if we should be filling new memory with a value.
   */
  if ( (string = DUMA_GETENV("DUMA_FILL")) != 0)
  {
    duma_tls->FILL = atoi(string);
    if ( -1 != duma_tls->FILL )
      duma_tls->FILL &= 255;
  }

  /*
   * Check with which value the memories no mans land is filled
   */
  if ( (string = DUMA_GETENV("DUMA_SLACKFILL")) != 0)
    _duma_s.SLACKFILL = atoi(string);
  _duma_s.SLACKFILL &= 255;

  /*
   * See if the user wants to see allocations / frees
   */
  if ( (string = DUMA_GETENV("DUMA_SHOW_ALLOC")) != 0 )
    _duma_s.SHOW_ALLOC = (atoi(string) != 0);

  /*
   * See if the user wants to call atexit()
   */
  if ( (string = DUMA_GETENV("DUMA_SUPPRESS_ATEXIT")) != 0 )
    _duma_s.SUPPRESS_ATEXIT = (atoi(string) != 0);

  /*
   * DUMA_OUTPUT_STACKTRACE is a global variable used to control if DUMA
   * outputs a stacktrace of the allocation that is not free'd. Default is 0,
   * meaning that this option is disabled.
   */
  if ( (string = DUMA_GETENV("DUMA_OUTPUT_STACKTRACE")) != 0 )
    DUMA_OUTPUT_STACKTRACE = (atoi(string) != 0);

  /*
   * DUMA_OUTPUT_STACKTRACE is a global variable used to control if DUMA
   * outputs a stacktrace of the allocation that is not free'd. Default is 0,
   * meaning that this option is disabled.
   */
  if ( (string = DUMA_GETENV("DUMA_OUTPUT_STACKTRACE_MAPFILE")) != 0 )
    DUMA_OUTPUT_STACKTRACE_MAPFILE = strdup(string);

  /*
   * DUMA_OUTPUT_DEBUG is a global variable used to control if DUMA
   * output is printed to the win32 debugging console.  Default is 0,
   * meaning that output is not by default sent to the debugging console.
   */
  if ( (string = DUMA_GETENV("DUMA_OUTPUT_DEBUG")) != 0 )
    DUMA_OUTPUT_DEBUG = (atoi(string) != 0);

  /*
   * DUMA_OUTPUT_STDOUT is a global variable used to control if DUMA
   * output is printed to STDOUT.  Default is 0,
   * meaning that output is not by default sent to STDOUT.
   */
  if ( (string = DUMA_GETENV("DUMA_OUTPUT_STDOUT")) != 0 )
    DUMA_OUTPUT_STDOUT = (atoi(string) != 0);

  /*
   * DUMA_OUTPUT_STDERR is a global variable used to control if DUMA
   * output is printed to STDERR.  Default is 1,
   * meaning that output is by default sent to STDERR.
   */
  if ( (string = DUMA_GETENV("DUMA_OUTPUT_STDERR")) != 0 )
    DUMA_OUTPUT_STDERR = (atoi(string) != 0);

  /*
   * DUMA_OUTPUT_FILE is a global variable used to control if DUMA
   * output is printed to a specified file.  Default is NULL,
   * meaning that output is not by default sent to a file.
   */
  if ( (string = DUMA_GETENV("DUMA_OUTPUT_FILE")) != 0 )
    DUMA_OUTPUT_FILE = strdup(string);

  /* Get Value for DUMA_SKIPCOUNT_INIT */
  if ( (string = DUMA_GETENV("DUMA_SKIPCOUNT_INIT")) != 0 )
    _duma_s.SKIPCOUNT_INIT = (atoi(string) != 0);

  /* Get Value for DUMA_CHECK_FREQ */
  if ( (string = DUMA_GETENV("DUMA_CHECK_FREQ")) != 0 )
  {
    int tmp = atoi(string);
    if ( tmp > 0 )
    _duma_s.CHECK_FREQ = tmp;
  }

  /* Should we send banner? */
  if ( (string = DUMA_GETENV("DUMA_DISABLE_BANNER")) != 0 )
    _duma_s.DISABLE_BANNER = (atoi(string) != 0);

  if ( !_duma_s.DISABLE_BANNER )
    DUMA_Print(version);
}

#include <signal.h>
static void sigsegv_handler(int signum) {
  passert_failed(__FILE__, __LINE__, "\nDUMA: DUMA_ASSERT(%d) failed)\n", signum);
}

/* Function: duma_init
 *
 * duma_init sets configuration settings.  Can sometimes cause problems
 * when called from _duma_init.
 *
 * duma_init is called from _duma_init unless DUMA_EXPLICIT_INIT
 * is defined at compile time.
 *
 * See Also: <_duma_init>
 */
#ifndef DUMA_EXPLICIT_INIT
static
#endif
void duma_init(void)
{
  void            * testAlloc;
  DUMA_TLSVARS_T  * duma_tls;


  /* avoid double call, when initialization already in progress */
  if ( _duma_s.init_state >= DUMAIS_IN_INIT && _duma_s.init_state <= DUMAIS_OUT_INIT )
    return;
  else
  {
#if DUMA_DETOURS
    _duma_init();
#endif

    _duma_s.init_state = DUMAIS_IN_INIT;
  }

  duma_tls = GET_DUMA_TLSVARS();

  duma_getenvvars( duma_tls );


  // SIGSEGV callback setup
  // So that failed tests show more info
  // and validate on asserts
  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sa.sa_handler = sigsegv_handler;
  sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGBUS, &sa, NULL);  // required for OSX

#if ( !defined(DUMA_NO_LEAKDETECTION) && ( defined(DUMA_PREFER_ATEXIT) || !defined(DUMA_GNU_INIT_ATTR) ) )
  /*
   * Register atexit()
   *  a) when we have Leak Detection and atexit() is preferred over GNU_INIT_ATTR
   *  b) when we have Leak Detection and GNU_INIT_ATTR is not set
   */

  #ifndef DUMA_NO_HANG_MSG
    if (0 == _duma_s.SUPPRESS_ATEXIT)
      DUMA_Print("\nDUMA: Registering with atexit().\n"
    #ifdef WIN32
                 "DUMA: If this hangs, change the library initialization order with DUMA_EXPLICIT_INIT.\n");
    #else
                 "DUMA: If this hangs, change the library load/init order with DUMA_EXPLICIT_INIT or LD_PRELOAD.\n");
    #endif
    else
      DUMA_Print("\nDUMA: Skipping registering with atexit(). Set DUMA_SUPPRESS_ATEXIT to 0 to register.\n");
  #endif /* DUMA_NO_HANG_MSG */

  if (!_duma_s.SUPPRESS_ATEXIT)
  {
    if ( atexit( _duma_exit ) )
      DUMA_Abort("Cannot register exit function.\n");

    #ifndef DUMA_NO_HANG_MSG
      DUMA_Print("DUMA: Registration was successful.\n");
    #endif /* DUMA_NO_HANG_MSG */
  }

#endif /* ( !defined(DUMA_NO_LEAKDETECTION) && ( defined(DUMA_PREFER_ATEXIT) || !defined(DUMA_GNU_INIT_ATTR) ) ) */

  /* initialize semaphoring */
  DUMA_INIT_SEMAPHORE();

#ifndef DUMA_NO_GLOBAL_MALLOC_FREE
  /*
   * Check whether malloc and free is available
   */
  testAlloc = malloc(123);
  if (_duma_s.numAllocs == 0)
    DUMA_Abort("malloc() is not bound to duma.\nDUMA Aborting: Preload lib with 'LD_PRELOAD=libduma.so <prog>'.\n");

  free(testAlloc);
  if (_duma_s.numDeallocs == 0)
    DUMA_Abort("free() is not bound to duma.\nDUMA Aborting: Preload lib with 'LD_PRELOAD=libduma.so <prog>'.\n");
#endif

  /* initialization finished */
  _duma_s.init_state = DUMAIS_OUT_INIT;
}


/* Function: _duma_init
 *
 * _duma_init sets up the memory allocation arena and the run-time
 * configuration information.  We will call duma_init unless DUMA_EXPLICIT_INIT
 * is defined at compile time.
 *
 * See Also: <duma_init>
 */
void
#ifdef DUMA_GNU_INIT_ATTR
__attribute ((constructor))
#endif
_duma_init(void)
{
  size_t size = MEMORY_CREATION_SIZE;
  struct _DUMA_Slot * slot;
  int               inRecursion = (_duma_s.init_state >= DUMAIS_IN_CONSTRUCTOR && _duma_s.init_state <= DUMAIS_OUT_INIT);

  /* constuction already done? this should not happen! */
  if (_duma_s.init_state >= DUMAIS_OUT_CONSTRUCTOR && _duma_s.init_state <= DUMAIS_OUT_INIT)
  {
#ifndef DUMA_EXPLICIT_INIT
    goto duma_constructor_callinit;
#else
    return;
#endif
  }
  else
    _duma_s.init_state = DUMAIS_IN_CONSTRUCTOR;

  if ( DUMA_PAGE_SIZE != Page_Size() )
    DUMA_Abort("DUMA_PAGE_SIZE is not correct. Run createconf and save results as duma_config.h");

  if(!inRecursion)
    DUMA_GET_SEMAPHORE();

  /* call of DUMA_GET_SEMAPHORE() may already have done the construction recursively! */
  if ( _duma_s.init_state >= DUMAIS_OUT_CONSTRUCTOR )
    goto duma_constructor_relsem;

  /*
   * Allocate special memory for malloc() or C++ operator new, when size is 0
   */
  _duma_s.null_block = Page_Create(2*DUMA_PAGE_SIZE, 1/*=exitonfail*/, 1/*=printerror*/);

  Page_DenyAccess(_duma_s.null_block, 2*DUMA_PAGE_SIZE);
    _duma_g.null_addr  = (void*)( (DUMA_ADDR)_duma_s.null_block + DUMA_PAGE_SIZE );

  /*
   * Figure out how many Slot structures to allocate at one time.
   */
  _duma_s.slotCount = _duma_s.slotsPerPage = DUMA_PAGE_SIZE / sizeof(struct _DUMA_Slot);
  _duma_s.allocListSize = DUMA_PAGE_SIZE;

  if ( size < _duma_s.allocListSize )
    size = _duma_s.allocListSize;

  size = ( size + DUMA_PAGE_SIZE -1 ) & ~( DUMA_PAGE_SIZE -1 );

  /*
   * Allocate memory, and break it up into two malloc buffers. The
   * first buffer will be used for Slot structures, the second will
   * be marked free.
   */
  slot = _duma_g.allocList = (struct _DUMA_Slot *)Page_Create(size, 0/*=exitonfail*/, 0/*=printerror*/);
  if ( 0 == _duma_g.allocList  &&  0L != _duma_s.PROTECT_FREE )
  {
    int reduce_more;
    do
    {
      /* reduce as much protected memory as we need - or at least try so */
      reduce_more = reduceProtectedMemory( (size+1023) >>10 );
      /* simply try again */
      slot = _duma_g.allocList = (struct _DUMA_Slot *)Page_Create( size, 0/*=exitonfail*/, 0/*= printerror*/ );
    }
    while ( reduce_more && 0 == _duma_g.allocList );

    if ( 0 == _duma_g.allocList )
      slot = _duma_g.allocList = (struct _DUMA_Slot *)Page_Create( size, 1/*=exitonfail*/, 1/*= printerror*/ );
  }

  memset((char *)_duma_g.allocList, 0, _duma_s.allocListSize);

  /* enter _duma_g.allocList as slot to allow call to free() when doing allocateMoreSlots() */
  slot[0].internalAddress   = slot[0].userAddress = _duma_g.allocList;
  slot[0].internalSize      = slot[0].userSize    = _duma_s.allocListSize;
  slot[0].state             = DUMAST_IN_USE;
  slot[0].allocator         = EFA_INT_ALLOC;
#ifndef DUMA_NO_LEAKDETECTION
  slot[0].fileSource        = DUMAFS_ALLOCATION;
  slot[0].filename          = __FILE__;
  slot[0].lineno            = __LINE__;
#endif

  if ( size > _duma_s.allocListSize )
  {
    slot[1].internalAddress = slot[1].userAddress
        = ((char *)slot[0].internalAddress) + slot[0].internalSize;
    slot[1].internalSize    = slot[1].userSize
        =   size - slot[0].internalSize;
    slot[1].state           = DUMAST_FREE;
    slot[1].allocator       = EFA_INT_ALLOC;
  #ifndef DUMA_NO_LEAKDETECTION
    slot[1].fileSource      = DUMAFS_ALLOCATION;
    slot[1].filename        = __FILE__;
    slot[1].lineno          = __LINE__;
  #endif
  }

  /*
   * Deny access to the free page, so that we will detect any software
   * that treads upon free memory.
   */
  Page_DenyAccess(slot[1].internalAddress, slot[1].internalSize);

  /*
   * Account for the two slot structures that we've used.
   */
  _duma_s.unUsedSlots = _duma_s.slotCount - 2;

  /* construction done */
  if ( _duma_s.init_state < DUMAIS_OUT_CONSTRUCTOR )
    _duma_s.init_state = DUMAIS_OUT_CONSTRUCTOR;

  duma_constructor_relsem:
  /***********************/

  if ( !inRecursion )
    DUMA_RELEASE_SEMAPHORE(0);

#ifndef DUMA_EXPLICIT_INIT
  duma_constructor_callinit:
  /*************************/
  if ( _duma_s.init_state < DUMAIS_OUT_INIT )
    duma_init();
#elif 0
  /* this output produces other problems !!! */
  DUMA_Print("\nDUMA: This platform needs an explicit call of duma_init() (DUMA_EXPLICIT_INIT).");
  DUMA_Print("\n  Take care that duma_init() is called early in main()!\n\n");
#endif
}


/* Function: allocateMoreSlots
 *
 * allocateMoreSlots is called when there are only enough slot structures
 * left to support the allocation of a single malloc buffer.
 *
 * See Also: <_duma_allocate>
 */
static void allocateMoreSlots(void)
{
  size_t  newSize = _duma_s.allocListSize + DUMA_PAGE_SIZE;
  void *  newAllocation;
  void *  oldAllocation = _duma_g.allocList;

  #ifndef DUMA_NO_LEAKDETECTION
    newAllocation = _duma_allocate( 1 /*=alignment*/
                                  , newSize
                                  , 0 /*=protectBelow*/
                                  , -1 /*=fillByte*/
                                  , 0 /*=protectAllocList*/
                                  , EFA_INT_ALLOC
                                  , DUMA_FAIL_NULL
                                  , __FILE__, __LINE__
                                  );
  #else
    newAllocation = _duma_allocate( 1 /*=alignment*/
                                  , newSize
                                  , 0 /*=protectBelow*/
                                  , -1 /*=fillByte*/
                                  , 0 /*=protectAllocList*/
                                  , EFA_INT_ALLOC
                                  , DUMA_FAIL_NULL
                                  );
  #endif

  if ( ! newAllocation )
    return;

  memcpy(newAllocation, _duma_g.allocList, _duma_s.allocListSize);
  memset(&(((char *)newAllocation)[_duma_s.allocListSize]), 0, DUMA_PAGE_SIZE);

  _duma_g.allocList = (struct _DUMA_Slot *)newAllocation;
  _duma_s.allocListSize = newSize;
  _duma_s.slotCount   += _duma_s.slotsPerPage;
  _duma_s.unUsedSlots += _duma_s.slotsPerPage;

  #ifndef DUMA_NO_LEAKDETECTION
    _duma_deallocate( oldAllocation, 0 /*=protectAllocList*/, EFA_INT_DEALLOC, __FILE__, __LINE__ );
  #else
    _duma_deallocate( oldAllocation, 0 /*=protectAllocList*/, EFA_INT_DEALLOC);
  #endif
}


/* Function: duma_alloc_return
 *
 * set your conditional breakpoint here to catch a specific allocation
 */
void * duma_alloc_return( void * address )
{
  return address;
}


/* Function: _duma_allocate
 *
 * This is the memory allocator. When asked to allocate a buffer, allocate
 * it in such a way that the end of the buffer is followed by an inaccessable
 * memory page. If software overruns that buffer, it will touch the bad page
 * and get an immediate segmentation fault. It's then easy to zero in on the
 * offending code with a debugger.
 *
 * There are a few complications. If the user asks for an odd-sized buffer,
 * we would have to have that buffer start on an odd address if the byte after
 * the end of the buffer was to be on the inaccessable page. Unfortunately,
 * there is lots of software that asks for odd-sized buffers and then
 * requires that the returned address be word-aligned, or the size of the
 * buffer be a multiple of the word size. An example are the string-processing
 * functions on Sun systems, which do word references to the string memory
 * and may refer to memory up to three bytes beyond the end of the string.
 * For this reason, I take the alignment requests to memalign() and valloc()
 * seriously, and
 *
 * DUMA wastes lots of memory.
 *
 * See Also: <_duma_deallocate>
 */
void * _duma_allocate(size_t alignment, size_t userSize, int protectBelow, int fillByte, int protectAllocList, enum _DUMA_Allocator allocator, enum _DUMA_FailReturn fail  DUMA_PARAMLIST_FL)
{
  size_t            count;
  struct _DUMA_Slot * slot;
  struct _DUMA_Slot * fullSlot;
  struct _DUMA_Slot * emptySlots[2];
  DUMA_ADDR           intAddr, userAddr, protAddr, endAddr;
  size_t              internalSize;
  int                 allocationStrategy;
#if defined(WIN32) && !defined(__CYGWIN__) && !defined(__MINGW32__) && !defined(__MINGW64__)
  char        stacktrace[601];
  char*       ptrStacktrace;
#endif
  DUMA_TLSVARS_T    * duma_tls = GET_DUMA_TLSVARS();

  /* check allocation strategy to use */
  switch ( _duma_allocDesc[allocator].std )
  {
    default:
    case DUMAAS_C:
      allocationStrategy = _duma_s.MALLOC_0_STRATEGY;
      break;
    case DUMAAS_CPP:
      allocationStrategy = _duma_s.NEW_0_STRATEGY;
      break;
  }

  DUMA_ASSERT( 0 != _duma_g.allocList );

#if defined(WIN32) && !defined(__CYGWIN__) && !defined(__MINGW32__) && !defined(__MINGW64__)
  /* When getting the stack trace memory will be allocated
   * via DUMA.  In situations were additional slots must
   * be allocated we must do this prior to getting a pointer
   * to the new empty slot.  For this reason please leave
   * this code at the top of this function.
   */
  if(!_duma_s.DUMA_IN_DUMA && _duma_s.init_state && DUMA_OUTPUT_STACKTRACE)
  {
    _duma_s.DUMA_IN_DUMA = 1;

    printStackTrace(stacktrace, sizeof(stacktrace), DUMA_OUTPUT_STACKTRACE_MAPFILE);
    internalSize = strlen(stacktrace) * sizeof(char) + 1;
    ptrStacktrace = (char*) LocalAlloc(NULL, internalSize);
    strcpy(ptrStacktrace, stacktrace);
    memset(stacktrace, 0, 600);

    _duma_s.DUMA_IN_DUMA = 0;
  }
#endif


  /* initialize return value */
  userAddr = 0;

  /* count and show allocation, if requested */
  _duma_s.numAllocs++;
  if (_duma_s.SHOW_ALLOC)
  {
    #ifndef DUMA_NO_LEAKDETECTION
      DUMA_Print("\nDUMA: Allocating %d bytes at %s(%i).", (DUMA_SIZE)userSize, filename, lineno);
    #else
      DUMA_Print("\nDUMA: Allocating %d bytes.", (DUMA_SIZE)userSize);
    #endif
    if ( 0 == userSize )
      DUMA_Print(" This is ANSI conform but probably a bug. See DUMA_ALLOW_MALLOC_0.");
  }

  /* check userSize */
  if ( 0 == userSize )
  {
#if 0
    if ( !_duma_s.ALLOW_MALLOC_0 )
    {
      #ifndef DUMA_NO_LEAKDETECTION
        DUMA_Abort("Allocating 0 bytes, probably a bug at %s(%i). See DUMA_ALLOW_MALLOC_0.", filename, lineno);
      #else
        DUMA_Abort("Allocating 0 bytes, probably a bug. See DUMA_ALLOW_MALLOC_0.");
      #endif
    }
    else
    {
      if ( allocationStrategy )
        userAddr = (DUMA_ADDR)_duma_g.null_addr;
      return (void*)userAddr;
    }
#else
    switch ( allocationStrategy )
    {
      case 0: /* like having former ALLOW_MALLOC_0 = 0  ==> abort program with segfault */
        #ifndef DUMA_NO_LEAKDETECTION
          DUMA_Abort("Allocating 0 bytes, probably a bug at %s(%i). See DUMA_ALLOW_MALLOC_0.", filename, lineno);
        #else
          DUMA_Abort("Allocating 0 bytes, probably a bug. See DUMA_ALLOW_MALLOC_0.");
        #endif
        return (void*)userAddr;
        break;
      case 1: /* return NULL pointer */
        return (void*)userAddr;
        break;
      case 2: /* return always the same pointer to some protected page */
      default:
        userAddr = (DUMA_ADDR)_duma_g.null_addr;
        return (void*)userAddr;
        break;
      case 3: /* return unique protected page */
        /* continue allocation! */
        break;
    } /* end switch () */

    /* only case 3 */
    internalSize = DUMA_PAGE_SIZE;
#endif
  }
  else /* if ( userSize ) */
  {
    /* check alignment */
    if ( ! alignment )
    {
      DUMA_SIZE a = (DUMA_SIZE)duma_tls->ALIGNMENT;
      DUMA_SIZE s = (DUMA_SIZE)userSize;

      if ( s < a )
      {
        /* to next lower power of 2 */
        for (a = s; a & (a-1); a &= a-1)  ;
      }

      alignment = (size_t)a; /* this is new alignment */
    }

    if ( (int)alignment != ((int)alignment & -(int)alignment) )
    {
      #ifndef DUMA_NO_LEAKDETECTION
        DUMA_Abort("Alignment (=%d) is not a power of 2 requested from %s(%i)", (DUMA_SIZE)alignment, filename, lineno);
      #else
        DUMA_Abort("Alignment (=%d) is not a power of 2", (DUMA_SIZE)alignment);
      #endif
    }

    /*
     * If protectBelow is set, all addresses returned by malloc()
     * and company will be page-aligned.
     *
     * The internal size of the buffer is rounded up to the next alignment and page-size
     * boundary, and then we add another page's worth of memory for the dead page.
     */
    /* a bit tricky but no modulo and no if () */
    internalSize = ( (userSize + DUMA_PAGE_SIZE -1) & ~(DUMA_PAGE_SIZE -1) ) + DUMA_PAGE_SIZE;

    if ( alignment > DUMA_PAGE_SIZE )
      internalSize += alignment - DUMA_PAGE_SIZE;
  } /* end if ( userSize ) */

  /*
   * These will hold the addresses of two empty Slot structures, that
   * can be used to hold information for any memory I create, and any
   * memory that I mark free.
   */
  fullSlot = emptySlots[0] = emptySlots[1] = 0;

  /*
   * The internal memory used by the allocator is currently
   * inaccessable, so that errant programs won't scrawl on the
   * allocator's arena. I'll un-protect it here so that I can make
   * a new allocation. I'll re-protect it before I return.
   */
  if ( protectAllocList )
  {
    IF__DUMA_INIT_DONE
      DUMA_GET_SEMAPHORE();

    Page_AllowAccess(_duma_g.allocList, _duma_s.allocListSize);
  }

  if ( _duma_s.CHECK_FREQ > 0 )
  {
    if ( (++ _duma_s.checkFreqCounter) == _duma_s.CHECK_FREQ )
    {
      _duma_check_all_slacks();
      _duma_s.checkFreqCounter = 0;
    }
  }

  /*
   * If I'm running out of empty slots, create some more before
   * I don't have enough slots left to make an allocation.
   */
  if ( DUMAAT_INTERNAL != _duma_allocDesc[allocator].type  &&  _duma_s.unUsedSlots < 7 )
    allocateMoreSlots();

  /*
   * Iterate through all of the slot structures. Attempt to find a slot
   * containing free memory of the exact right size. Accept a slot with
   * more memory than we want, if the exact right size is not available.
   * Find two slot structures that are not in use. We will need one if
   * we split a buffer into free and allocated parts, and the second if
   * we have to create new memory and mark it as free.
   *
   */
  for ( slot = _duma_g.allocList, count = _duma_s.slotCount ; count > 0; --count, ++slot )
  {
    /*
     * Windows needs special treatment, cause Page_Delete() needs exactly
     * the same memory region as Page_Create()!
     * Thus as a quick hack no memory management is done by DUMA.
     */
#if !defined(WIN32)
    if ( DUMAST_FREE == slot->state  &&  slot->internalSize >= internalSize )
    {
      if ( !fullSlot || slot->internalSize < fullSlot->internalSize )
      {
        fullSlot = slot;
        if ( slot->internalSize == internalSize )
          break;  /* All done; no empty slot needed in this case */
      }
    }
    else
#endif
      if ( DUMAST_EMPTY == slot->state )
      {
        if(!emptySlots[0])
          emptySlots[0] = slot;
        else if ( !emptySlots[1] )
          emptySlots[1] = slot;

        #if defined(WIN32)
          break;
        #endif
      }
  }

  if ( !fullSlot )
  {
    /*
      * I get here if I haven't been able to find a free buffer
      * with all of the memory I need. I'll have to create more
      * memory. I'll mark it all as free, and then split it into
      * free and allocated portions later.
      */
    size_t  chunkSize;
    long    chunkSizekB;

    #if defined(WIN32)
      chunkSize = internalSize;
    #else
      chunkSize = MEMORY_CREATION_SIZE;

      if ( chunkSize < internalSize )
        chunkSize = internalSize;

      chunkSize = ( chunkSize + DUMA_PAGE_SIZE -1 ) & ~( DUMA_PAGE_SIZE -1 );
    #endif

    chunkSizekB = (long)( (chunkSize+1023) >>10 );


    /* Use up one of the empty slots to make the full slot. */
    if ( !emptySlots[0] )
      DUMA_Abort("Internal error in allocator: No empty slot 0.\n");

    #if !defined(WIN32)
      if ( !emptySlots[1] )
        DUMA_Abort("Internal error in allocator: No empty slot 1.\n");
    #endif

    fullSlot      = emptySlots[0];
    emptySlots[0] = emptySlots[1];

    /* reduce protected memory when we would exceed _duma_s.MAX_ALLOC */
    if ( _duma_s.MAX_ALLOC > 0L  &&  _duma_s.sumAllocatedMem + chunkSizekB > _duma_s.MAX_ALLOC )
      reduceProtectedMemory( chunkSizekB );

    fullSlot->internalAddress = Page_Create( chunkSize, 0/*= exitonfail*/, 0/*= printerror*/ );

    if ( 0 == fullSlot->internalAddress  &&  0L != _duma_s.PROTECT_FREE )
    {
      int reduce_more;
      do
      {
        /* reduce as much protected memory as we need - or at least try so */
        reduce_more = reduceProtectedMemory( (chunkSize+1023) >>10 );
        /* simply try again */
        fullSlot->internalAddress = Page_Create( chunkSize, 0/*= exitonfail*/, 0/*= printerror*/ );
      }
      while ( reduce_more && 0 == fullSlot->internalAddress );

      if ( 0 == fullSlot->internalAddress  &&  DUMA_FAIL_ENV == fail )
        fullSlot->internalAddress = Page_Create( chunkSize, _duma_s.MALLOC_FAILEXIT, 1/*= printerror*/ );
    }

    if ( fullSlot->internalAddress )
    {
      _duma_s.sumAllocatedMem       += ( (chunkSize +1023) >>10 );
      _duma_s.sumTotalAllocatedMem  += ( (chunkSize +1023) >>10 );
      fullSlot->internalSize  = chunkSize;
      fullSlot->state         = DUMAST_FREE;
      --_duma_s.unUsedSlots;
    }
  } /* end if ( !fullSlot ) */

  if ( fullSlot->internalSize )
  {

#if !defined(WIN32)
    /*
     * If the buffer I've found is larger than I need, split it into
     * an allocated buffer with the exact amount of memory I need, and
     * a free buffer containing the surplus memory.
     */
    if ( fullSlot->internalSize > internalSize )
    {
      /* copy and adjust contents for free slot */
      *emptySlots[0]                 = *fullSlot;
      emptySlots[0]->internalAddress = (char *)emptySlots[0]->internalAddress + internalSize;
      emptySlots[0]->internalSize   -= internalSize;
      emptySlots[0]->userAddress     = emptySlots[0]->internalAddress;
      emptySlots[0]->userSize        = emptySlots[0]->internalSize;

      /* adjust size of fullSlot */
      fullSlot->internalSize         = internalSize;

      --_duma_s.unUsedSlots;
    }
#endif

#if 0
#else
    if ( 0 == userSize )
    {
      /*
       * we need just a single page
       * may deny any access to it
       *
       */

      /* Figure out what address to give the user: mid of protected page */
      intAddr  = (DUMA_ADDR)fullSlot->internalAddress;
      endAddr  = intAddr + internalSize;
      userAddr = intAddr  + (DUMA_PAGE_SIZE >> 1);
      protAddr = intAddr;

      /* Set up the "dead" page(s). */
      Page_DenyAccess( (char*)protAddr, endAddr - protAddr );
    }
    else
#endif
    if ( !protectBelow )
    {
      /*
       * Arrange the buffer so that it is followed by an inaccessable
       * memory page. A buffer overrun that touches that page will
       * cause a segmentation fault.
       * internalAddr <= userAddr < protectedAddr
       */

      /* Figure out what address to give the user. */
      intAddr  = (DUMA_ADDR)fullSlot->internalAddress;
      endAddr  = intAddr + internalSize;
      userAddr = ( intAddr  + internalSize - DUMA_PAGE_SIZE - userSize ) & ~(alignment -1);
      protAddr = ( userAddr + userSize     + DUMA_PAGE_SIZE -1)     & ~(DUMA_PAGE_SIZE -1);

      /* DUMA_ASSERT(intAddr <= userAddr && intAddr < protAddr ); */

      /* Set up the "live" page(s). */
      Page_AllowAccess( (char*)intAddr, protAddr - intAddr );
      /* Set up the "dead" page(s). */
      Page_DenyAccess( (char*)protAddr, endAddr - protAddr );
    }
    else /* if (protectBelow) */
    {
      /*
       * Arrange the buffer so that it is preceded by an inaccessable
       * memory page. A buffer underrun that touches that page will
       * cause a segmentation fault.
       */
      /* Figure out what address to give the user. */
      intAddr  = (DUMA_ADDR)fullSlot->internalAddress;
      endAddr  = intAddr + internalSize;
      userAddr = ( intAddr + DUMA_PAGE_SIZE + alignment -1) & ~(alignment -1);
      protAddr = ( userAddr & ~(DUMA_PAGE_SIZE -1) ) - DUMA_PAGE_SIZE;

      /* DUMA_ASSERT(intAddr < userAddr && intAddr <= protAddr ); */

      /* Set up the "live" page(s). userAddr == protAddr + DUMA_PAGE_SIZE ! */
      Page_AllowAccess( (char*)userAddr, internalSize - (userAddr - protAddr) );
      /* Set up the "dead" page(s). */
      Page_DenyAccess( (char*)intAddr, userAddr - intAddr );
    }

    /* => userAddress = internalAddress + DUMA_PAGE_SIZE */
    fullSlot->userAddress = (char*)userAddr;
    fullSlot->protAddress = (char*)protAddr;
    fullSlot->userSize    = userSize;
    fullSlot->state       = DUMAST_IN_USE;
    fullSlot->allocator   = allocator;

    #ifndef DUMA_NO_LEAKDETECTION
      fullSlot->fileSource  = DUMAFS_ALLOCATION;
      fullSlot->filename    = (char*)filename;
      #ifdef DUMA_EXPLICIT_INIT

        /* mark allocations from standard libraries
         * before duma_init() is finished with lineno = -1
         * to allow special treatment in leak_checking
         */
        fullSlot->lineno    = (DUMAIS_OUT_INIT == _duma_s.init_state) ? lineno : -1;
      #else
        fullSlot->lineno    = lineno;
      #endif
    #endif

    /* initialise no mans land of slot */
    _duma_init_slack( fullSlot );

#if defined(WIN32) && !defined(__CYGWIN__) && !defined(__MINGW32__) && !defined(__MINGW64__)
    if(!_duma_s.DUMA_IN_DUMA && _duma_s.init_state && DUMA_OUTPUT_STACKTRACE)
    {
      _duma_s.DUMA_IN_DUMA = 1;

      /* Get stacktrace */
      if(fullSlot->stacktrace)
        LocalFree(fullSlot->stacktrace);

      fullSlot->stacktrace = ptrStacktrace;

      _duma_s.DUMA_IN_DUMA = 0;
    }
    else
      fullSlot->stacktrace = 0;
#endif

  } /* end if ( fullSlot->internalSize ) */

  /*
   * Make the pool's internal memory inaccessable, so that the program
   * being debugged can't stomp on it.
   */
  if ( protectAllocList )
  {
    Page_DenyAccess(_duma_g.allocList, _duma_s.allocListSize);

    IF__DUMA_INIT_DONE
      DUMA_RELEASE_SEMAPHORE(0);
  }

  /* Fill the memory if it was specified to do so. */
  if ( ((char*)userAddr) && fillByte != -1 && userSize )
    memset( (char*)userAddr, fillByte, userSize);

  return duma_alloc_return( (char*)userAddr );
}


/* Function: _duma_deallocate
 *
 * Deallocate allocated memory after running some checks, then open
 * slot for use.  Uses Page_Delete to free the underlying memory.
 *
 * See Also: <Page_Delete> <_duma_allocate>
 */
void _duma_deallocate(void * address, int protectAllocList, enum _DUMA_Allocator allocator  DUMA_PARAMLIST_FL)
{
  struct _DUMA_Slot   * slot;
  long                internalSizekB;

  if ( 0 == _duma_g.allocList )
  {
#ifdef DUMA_DETOURS
    /* Odd things happen with detours sometimes... */
    DUMA_Print("DUMA_Warning: free() called before first malloc().");
    return;
#else
    DUMA_Abort("free() called before first malloc().");
#endif
  }

  if ( 0 == address || _duma_g.null_addr == address )
    return;

  if ( protectAllocList )
  {
    IF__DUMA_INIT_DONE
      DUMA_GET_SEMAPHORE();

    Page_AllowAccess(_duma_g.allocList, _duma_s.allocListSize);
  }

  if ( _duma_s.CHECK_FREQ > 0 )
  {
    if ( (++ _duma_s.checkFreqCounter) == _duma_s.CHECK_FREQ )
    {
      _duma_check_all_slacks();
      _duma_s.checkFreqCounter = 0;
    }
  }

  if ( !(slot = slotForUserAddress(address)) )
  {
    if ( (slot = nearestSlotForUserAddress(address)) )
    {
#ifndef DUMA_NO_LEAKDETECTION
      if ( DUMAFS_ALLOCATION == slot->fileSource )
        DUMA_Abort("free(%a): address not from DUMA or already freed. Address may be corrupted from %a allocated from %s(%i)",
          (DUMA_ADDR)address, (DUMA_ADDR)slot->userAddress, slot->filename, slot->lineno);
      else if ( DUMAFS_DEALLOCATION == slot->fileSource )
        DUMA_Abort("free(%a): address not from DUMA or already freed. Address may be corrupted from %a deallocated at %s(%i)",
          (DUMA_ADDR)address, (DUMA_ADDR)slot->userAddress, slot->filename, slot->lineno);
      else
#endif
        DUMA_Abort("free(%a): address not from DUMA or already freed. Address may be corrupted from %a.",
          (DUMA_ADDR)address, (DUMA_ADDR)slot->userAddress);
    }
    else
    {
#if DUMA_DETOURS
      /* For Detours we need to not dump out, we get one extra free up front for some reason. */
      DUMA_Print("DUMA_Warning: free(%a): address not from DUMA or already freed.", (DUMA_ADDR)address);
      return;
#else
      DUMA_Abort("free(%a): address not from DUMA or already freed.", (DUMA_ADDR)address);
#endif
    }
  }

  if ( DUMAST_ALL_PROTECTED == slot->state || DUMAST_BEGIN_PROTECTED == slot->state )
  {
#ifndef DUMA_NO_LEAKDETECTION
    if ( DUMAFS_ALLOCATION == slot->fileSource )
      DUMA_Abort("free(%a): memory already freed. allocated from %s(%i)",
        (DUMA_ADDR)address, slot->filename, slot->lineno);
    else if ( DUMAFS_DEALLOCATION == slot->fileSource )
      DUMA_Abort("free(%a): memory already freed at %s(%i)",
        (DUMA_ADDR)address, slot->filename, slot->lineno);
    else
#endif
      DUMA_Abort("free(%a): memory already freed.", (DUMA_ADDR)address);
  }
  else if ( _duma_allocDesc[slot->allocator].type != _duma_allocDesc[allocator].type )
  {
#ifndef DUMA_NO_LEAKDETECTION
    if ( DUMAFS_ALLOCATION == slot->fileSource )
      /*                                    1            2  3                        4             5  6 */
      DUMA_Abort("Free mismatch: allocator '%s' used  at %s(%i)\n  but  deallocator '%s' called at %s(%i)!",
        _duma_allocDesc[slot->allocator].name, slot->filename, slot->lineno,
        _duma_allocDesc[allocator].name, filename, lineno );
    else if ( DUMAFS_DEALLOCATION == slot->fileSource )
      /*                                    1                           2             3  4 */
      DUMA_Abort("Free mismatch: allocator '%s' used \nbut deallocator '%s' called at %s(%i)!",
        _duma_allocDesc[slot->allocator].name,
        _duma_allocDesc[allocator].name, filename, lineno );
    else
#endif
      DUMA_Abort("Free mismatch: allocator '%s' used  but  deallocator '%s' called!",
        _duma_allocDesc[slot->allocator].name, _duma_allocDesc[allocator].name );
  }

  /* count and show deallocation, if requested */
  _duma_s.numDeallocs++;
  if (_duma_s.SHOW_ALLOC)
#ifndef DUMA_NO_LEAKDETECTION
    DUMA_Print("\nDUMA: Freeing %d bytes at %s(%i) (Allocated from %s(%i)).",
      (DUMA_SIZE)slot->userSize, filename, lineno, slot->filename, slot->lineno);
#else
    DUMA_Print("\nDUMA: Freeing %d bytes.", (DUMA_SIZE)slot->userSize);
#endif

  /* CHECK INTEGRITY OF NO MANS LAND */
  _duma_check_slack( slot );

  if ( _duma_s.FREE_ACCESS )
  {
    volatile char *start = slot->userAddress;
    volatile char *cur;

    for (cur = (char*)slot->userAddress+slot->userSize; --cur >= start; )
    {
      char c = *cur;
      *cur = c-1;
      *cur = c;
    }
  }

  internalSizekB = (slot->internalSize+1023) >>10;

  /* protect memory, that nobody can access it */
  /* Free as much protected memory, that we can protect this one */
  /* is there need? and is there a way to free such much? */
  if ( _duma_s.PROTECT_FREE > 0L
    && _duma_s.sumProtectedMem  + internalSizekB >  _duma_s.PROTECT_FREE
    &&                            internalSizekB <  _duma_s.PROTECT_FREE
    && _duma_s.sumProtectedMem >= internalSizekB)
  {
    reduceProtectedMemory( internalSizekB );
  }

  if (( EFA_INT_ALLOC != slot->allocator )
    && ( _duma_s.PROTECT_FREE < 0L
    || ( _duma_s.PROTECT_FREE > 0L
    && _duma_s.sumProtectedMem + internalSizekB <= _duma_s.PROTECT_FREE
    )   )
    )
  {
    slot->state = DUMAST_ALL_PROTECTED;
    Page_DenyAccess(slot->internalAddress, slot->internalSize);
    _duma_s.sumProtectedMem += internalSizekB;

#ifndef DUMA_NO_LEAKDETECTION
    if ( lineno )
    {
      slot->fileSource  = DUMAFS_DEALLOCATION;
      slot->filename    = (char*)filename;
      slot->lineno      = lineno;
    }
#endif
  }
  else
  {
    /* free all the memory */
    Page_Delete(slot->internalAddress, slot->internalSize);
    _duma_s.sumAllocatedMem -= internalSizekB;

    /* free slot and userAddr */
    slot->internalAddress = slot->userAddress = 0;
    slot->internalSize    = slot->userSize    = 0;
    slot->state           = DUMAST_EMPTY;
    slot->allocator       = EFA_INT_ALLOC;
#ifndef DUMA_NO_LEAKDETECTION
    slot->fileSource      = DUMAFS_EMPTY;
    slot->filename      = 0;
    slot->lineno        = 0;
#endif
#if defined(WIN32) && !defined(__CYGWIN__) && !defined(__MINGW32__) && !defined(__MINGW64__)
    if(slot->stacktrace)
    {
      slot->stacktrace = 0;
      LocalFree(slot->stacktrace);
    }
#endif
  }

  if ( protectAllocList )
  {
    Page_DenyAccess(_duma_g.allocList, _duma_s.allocListSize);
    IF__DUMA_INIT_DONE
      DUMA_RELEASE_SEMAPHORE(0);
  }
}


/* Function: duma_check
 *
 * Check No Mans Land of a memory block.
 *
 */
void duma_check(void * address)
{
  struct _DUMA_Slot   * slot;

  if ( 0 == address )
    return;

  IF__DUMA_INIT_DONE
    DUMA_GET_SEMAPHORE();

  Page_AllowAccess(_duma_g.allocList, _duma_s.allocListSize);

  if ( !(slot = slotForUserAddress(address)) )
  {
    if ( (slot = nearestSlotForUserAddress(address)) )
    {
#ifndef DUMA_NO_LEAKDETECTION
      if ( DUMAFS_ALLOCATION == slot->fileSource )
        DUMA_Abort("check(%a): address not from DUMA or already freed. Address may be corrupted from %a allocated from %s(%i)",
          (DUMA_ADDR)address, (DUMA_ADDR)slot->userAddress, slot->filename, slot->lineno);
      else if ( DUMAFS_DEALLOCATION == slot->fileSource )
        DUMA_Abort("check(%a): address not from DUMA or already freed. Address may be corrupted from %a deallocated at %s(%i)",
          (DUMA_ADDR)address, (DUMA_ADDR)slot->userAddress, slot->filename, slot->lineno);
      else
#endif
        DUMA_Abort("check(%a): address not from DUMA or already freed. Address may be corrupted from %a.",
          (DUMA_ADDR)address, (DUMA_ADDR)slot->userAddress);
    }
    else
      DUMA_Abort("check(%a): address not from DUMA or already freed.", (DUMA_ADDR)address);
  }

  if ( DUMAST_ALL_PROTECTED == slot->state || DUMAST_BEGIN_PROTECTED == slot->state )
  {
#ifndef DUMA_NO_LEAKDETECTION
    if ( DUMAFS_ALLOCATION == slot->fileSource )
      DUMA_Abort("check(%a): memory already freed. allocated from %s(%i)",
        (DUMA_ADDR)address, slot->filename, slot->lineno);
    else if ( DUMAFS_DEALLOCATION == slot->fileSource )
      DUMA_Abort("check(%a): memory already freed at %s(%i)",
        (DUMA_ADDR)address, slot->filename, slot->lineno);
    else
#endif
      DUMA_Abort("check(%a): memory already freed.", (DUMA_ADDR)address);
  }

  /* CHECK INTEGRITY OF NO MANS LAND */
  _duma_check_slack( slot );


  Page_DenyAccess(_duma_g.allocList, _duma_s.allocListSize);
  IF__DUMA_INIT_DONE
    DUMA_RELEASE_SEMAPHORE(0);
}


/* Function: duma_checkAll
 *
 * Check No Mans Land of all memory blocks.
 *
 */
void duma_checkAll()
{
  IF__DUMA_INIT_DONE
    DUMA_GET_SEMAPHORE();

  Page_AllowAccess(_duma_g.allocList, _duma_s.allocListSize);

  _duma_check_all_slacks();

  Page_DenyAccess(_duma_g.allocList, _duma_s.allocListSize);
  IF__DUMA_INIT_DONE
    DUMA_RELEASE_SEMAPHORE(0);
}


/*********************************************************/

/* Function: _duma_malloc
 *
 * A version of malloc.
 */
void * _duma_malloc(size_t size  DUMA_PARAMLIST_FL)
{
  DUMA_TLSVARS_T  * duma_tls;

  if ( _duma_g.allocList == 0 )
    _duma_init();  /* This sets DUMA_ALIGNMENT, DUMA_PROTECT_BELOW, DUMA_FILL, ... */

  duma_tls = GET_DUMA_TLSVARS();

  return _duma_allocate(0, size, duma_tls->PROTECT_BELOW,
    duma_tls->FILL, 1 /*=protectAllocList*/, EFA_MALLOC,
    DUMA_FAIL_ENV  DUMA_PARAMS_FL);
}


/* Function: _duma_calloc
 *
 * A version of calloc.
 */
void * _duma_calloc(size_t nelem, size_t elsize  DUMA_PARAMLIST_FL)
{
  DUMA_TLSVARS_T  * duma_tls;

  if ( _duma_g.allocList == 0 )
    _duma_init();  /* This sets DUMA_ALIGNMENT, DUMA_PROTECT_BELOW, DUMA_FILL, ... */

  duma_tls = GET_DUMA_TLSVARS();

  return _duma_allocate(0, nelem * elsize,
    duma_tls->PROTECT_BELOW, 0 /*=fillByte*/,
    1 /*=protectAllocList*/, EFA_CALLOC,
    DUMA_FAIL_ENV  DUMA_PARAMS_FL);
}


/* Function: _duma_free
 *
 * A version of free.
 */
void   _duma_free(void * baseAdr  DUMA_PARAMLIST_FL)
{
  if ( _duma_g.allocList == 0 )
    _duma_init();  /* This sets DUMA_ALIGNMENT, DUMA_PROTECT_BELOW, DUMA_FILL, ... */

  _duma_deallocate(baseAdr, 1 /*=protectAllocList*/, EFA_FREE  DUMA_PARAMS_FL);
}


/* Function: _duma_memalign
 *
 * A version of memalign.
 */
void * _duma_memalign(size_t alignment, size_t size  DUMA_PARAMLIST_FL)
{
  DUMA_TLSVARS_T  * duma_tls;

  if ( _duma_g.allocList == 0 )
    _duma_init();  /* This sets DUMA_ALIGNMENT, DUMA_PROTECT_BELOW, DUMA_FILL, ... */

  duma_tls = GET_DUMA_TLSVARS();

  return _duma_allocate(alignment, size, duma_tls->PROTECT_BELOW,
    duma_tls->FILL, 1 /*=protectAllocList*/, EFA_MEMALIGN,
    DUMA_FAIL_ENV  DUMA_PARAMS_FL);
}


/* Function: _duma_posix_memalign
 *
 * A version of posix_memalign.
 */
int    _duma_posix_memalign(void **memptr, size_t alignment, size_t size  DUMA_PARAMLIST_FL)
{
  DUMA_TLSVARS_T  * duma_tls;
  void * retptr;

  if ( (alignment & (alignment -1)) || alignment < sizeof(void *) )
    return EINVAL;

  if ( _duma_g.allocList == 0 )
    _duma_init();  /* This sets DUMA_ALIGNMENT, DUMA_PROTECT_BELOW, DUMA_FILL, ... */

  duma_tls = GET_DUMA_TLSVARS();

  retptr = _duma_allocate(alignment, size, duma_tls->PROTECT_BELOW,
    duma_tls->FILL, 1 /*=protectAllocList*/, EFA_POSIX_MEMALIGN,
    DUMA_FAIL_ENV  DUMA_PARAMS_FL);

  if ( retptr )
  {
    (*(char**)memptr) = (char*)retptr;
    return 0;
  }
  else
  {
    (*(char**)memptr) = NULL;
    return ENOMEM;
  }
}


/* Function: _duma_realloc
 *
 * A version of realloc that provides extra checks based on
 * information we know about HEAP.
 */
void * _duma_realloc(void * oldBuffer, size_t newSize  DUMA_PARAMLIST_FL)
{
  void * ptr;
  DUMA_TLSVARS_T  * duma_tls;

  if( _duma_g.allocList == 0 )
    _duma_init();  /* This sets DUMA_ALIGNMENT, DUMA_PROTECT_BELOW, DUMA_FILL, ... */

  duma_tls = GET_DUMA_TLSVARS();

  IF__DUMA_INIT_DONE
  DUMA_GET_SEMAPHORE();

  Page_AllowAccess(_duma_g.allocList, _duma_s.allocListSize);

  ptr = _duma_allocate(0, newSize, duma_tls->PROTECT_BELOW,
    -1 /*=fillByte*/, 0 /*=protectAllocList*/, EFA_REALLOC,
    DUMA_FAIL_ENV  DUMA_PARAMS_FL);

  if( ptr && oldBuffer )
  {
    struct _DUMA_Slot * slot = slotForUserAddress(oldBuffer);

    if ( slot == 0 )
      DUMA_Abort("realloc(%a, %d): address not from malloc().",
      (DUMA_ADDR)oldBuffer, (DUMA_SIZE)newSize);

    if ( newSize > slot->userSize )
    {
      memcpy( ptr, oldBuffer, slot->userSize );
      memset( (char*)ptr + slot->userSize, 0, newSize - slot->userSize );
    }
    else if ( newSize > 0 )
      memcpy(ptr, oldBuffer, newSize);

    _duma_deallocate(oldBuffer, 0 /*=protectAllocList*/, EFA_REALLOC  DUMA_PARAMS_FL);
  }

  Page_DenyAccess(_duma_g.allocList, _duma_s.allocListSize);

  IF__DUMA_INIT_DONE
  DUMA_RELEASE_SEMAPHORE(0);

  return ptr;
}


/* Function: _duma_valloc
 *
 * A version of valloc.
 */
void * _duma_valloc(size_t size  DUMA_PARAMLIST_FL)
{
  DUMA_TLSVARS_T  * duma_tls;

  if ( _duma_g.allocList == 0 )
    _duma_init();  /* This sets DUMA_ALIGNMENT, DUMA_PROTECT_BELOW, DUMA_FILL, ... */

  duma_tls = GET_DUMA_TLSVARS();

  return _duma_allocate(DUMA_PAGE_SIZE, size, duma_tls->PROTECT_BELOW,
    duma_tls->FILL, 1 /*=protectAllocList*/, EFA_VALLOC,
    DUMA_FAIL_ENV  DUMA_PARAMS_FL);
}


/* Function: _duma_strdup
 *
 * A version of strdup.
 */
char * _duma_strdup(const char * str  DUMA_PARAMLIST_FL)
{
  size_t size;
  char * dup;
  DUMA_TLSVARS_T  * duma_tls;
  unsigned i;

  if ( _duma_g.allocList == 0 )
    _duma_init();  /* This sets DUMA_ALIGNMENT, DUMA_PROTECT_BELOW, DUMA_FILL, ... */

  duma_tls = GET_DUMA_TLSVARS();

  size = 0;
  while (str[size])
    ++size;

  dup = _duma_allocate(0, size +1, duma_tls->PROTECT_BELOW,
    -1 /*=fillByte*/, 1 /*=protectAllocList*/, EFA_STRDUP,
    DUMA_FAIL_ENV  DUMA_PARAMS_FL);

  if (dup)                    /* if successful */
    for (i=0; i<=size; ++i)   /* copy string */
      dup[i] = str[i];

  return dup;
}


/* Function: _duma_memcpy
 *
 * A version of memcpy that provides extra checks based on
 * information we know about HEAP.
 *
 * Currently the only check we perform is overlapping memory
 * regions.  This should be expanded to include checking size
 * of dest to verify assumptions.
 */
void * _duma_memcpy(void *dest, const void *src, size_t size  DUMA_PARAMLIST_FL)
{
  char       * d = (char *)dest;
  const char * s = (const char *)src;
  unsigned i;

  if ( (s < d  &&  d < s + size) || (d < s  &&  s < d + size) )
  {
#ifndef DUMA_NO_LEAKDETECTION
    DUMA_Abort("memcpy(%a, %a, %d): memory regions overlap at %s(%i)."
      , (DUMA_ADDR)dest, (DUMA_ADDR)src, (DUMA_SIZE)size, filename, lineno );
#else
    DUMA_Abort("memcpy(%a, %a, %d): memory regions overlap."
      , (DUMA_ADDR)dest, (DUMA_ADDR)src, (DUMA_SIZE)size );
#endif
  }

  for (i=0; i<size; ++i)
    d[i] = s[i];

  return dest;
}


/* Function: _duma_strnlen
 *
 * like strlen() but maximum return value is size
 */
size_t _duma_strnlen(const char *src, size_t size)
{
  size_t len;
  for ( len =0; len < size && src[len]; ++len )
    ;
  return len;
}


/* Function: _duma_strcpy
 *
 * A version of strcpy that provides extra checks based on
 * information we know about HEAP.
 *
 * Currently the only check we perform is overlapping memory
 * regions.  This should be expanded to include checking size
 * of dest to verify assumptions.
 */
char * _duma_strcpy(char *dest, const char *src  DUMA_PARAMLIST_FL)
{
  size_t i;
  size_t size = strlen(src) +1;

  if ( src < dest  &&  dest < src + size )
  {
#ifndef DUMA_NO_LEAKDETECTION
    DUMA_Abort("strcpy(%a, %a): memory regions overlap at %s(%i)."
      , (DUMA_ADDR)dest, (DUMA_ADDR)src, filename, lineno );
#else
    DUMA_Abort("strcpy(%a, %a): memory regions overlap."
      , (DUMA_ADDR)dest, (DUMA_ADDR)src );
#endif
  }

  for (i=0; i<size; ++i)
    dest[i] = src[i];

  return dest;
}


/* Function: _duma_strncpy
 *
 * A version of strncpy that provides extra checks based on
 * information we know about HEAP.
 *
 * Currently the only check we perform is overlapping memory
 * regions.  This should be expanded to include checking size
 * of dest to verify assumptions.
 */
char * _duma_strncpy(char *dest, const char *src, size_t size  DUMA_PARAMLIST_FL)
{
  size_t i, srclen;

  srclen  = _duma_strnlen(src, size);

  if ( size > 0  &&
      (  ( src < dest  &&  dest < src  + srclen )
       ||( dest < src  &&  src  < dest + size   )
     ))
  {
#ifndef DUMA_NO_LEAKDETECTION
    DUMA_Abort("strncpy(%a, %a, %d): memory regions overlap at %s(%i)."
      , (DUMA_ADDR)dest, (DUMA_ADDR)src, (DUMA_SIZE)size, filename, lineno );
#else
    DUMA_Abort("strncpy(%a, %a, %d): memory regions overlap."
      , (DUMA_ADDR)dest, (DUMA_ADDR)src, (DUMA_SIZE)size );
#endif
  }

  /* copy src to dest - up to size or zero terminator
   *   whatever happens first
   */
  for (i =0; i < size && src[i]; ++i)
    dest[i] = src[i];

  /* fill rest with '\0' character */
  for ( ; i<size; ++i)
    dest[i] = 0;

  return dest;
}


/* Function: _duma_strcat
 *
 * A version of strcat that provides extra checks based on
 * information we know about HEAP.
 *
 * Currently the only check we perform is overlapping memory
 * regions.  This should be expanded to include checking size
 * of dest to verify assumptions.
 */
char * _duma_strcat(char *dest, const char *src  DUMA_PARAMLIST_FL)
{
  unsigned i;
  size_t destlen = strlen(dest);
  size_t srcsize = strlen(src) + 1;

  if ( src < dest +destlen  &&  dest + destlen < src + srcsize )
  {
#ifndef DUMA_NO_LEAKDETECTION
    DUMA_Abort("strcat(%a, %a): memory regions overlap at %s(%i)."
      , (DUMA_ADDR)dest, (DUMA_ADDR)src, filename, lineno );
#else
    DUMA_Abort("strcat(%a, %a): memory regions overlap."
      , (DUMA_ADDR)dest, (DUMA_ADDR)src );
#endif
  }

  for (i=0; i<srcsize; ++i)
    dest[destlen+i] = src[i];

  return dest;
}


/* Function: _duma_strncat
 *
 * A version of strncat that provides extra checks based on
 * information we know about HEAP.
 *
 * Currently the only check we perform is overlapping memory
 * regions.  This should be expanded to include checking size
 * of dest to verify assumptions (like is size right).
 */
char * _duma_strncat(char *dest, const char *src, size_t size  DUMA_PARAMLIST_FL)
{
  unsigned i;
  size_t destlen, srclen;

  /* do nothing, when size not > 0 */
  if ( size <= 0 )
    return dest;

  /* calculate number of characters to copy from src to dest */
  destlen = strlen(dest);
  srclen  = _duma_strnlen(src, size);

  /* CHECK: Verify memory regions do not overlap */
  if ( src < (dest + destlen) && (dest + destlen) < (src + srclen + 1) )
  {
#ifndef DUMA_NO_LEAKDETECTION
    DUMA_Abort("strncat(%a, %a, %d): memory regions overlap at %s(%i)."
      , (DUMA_ADDR)dest, (DUMA_ADDR)src, (DUMA_SIZE)size, filename, lineno );
#else
    DUMA_Abort("strncat(%a, %a, %d): memory regions overlap."
      , (DUMA_ADDR)dest, (DUMA_ADDR)src, (DUMA_SIZE)size );
#endif
  }

  /* copy up to size characters from src to dest */
  for (i=0; i<srclen; ++i)
    dest[destlen+i] = src[i];

  /* append single '\0' character */
  dest[destlen+srclen] = 0;

  return dest;
}


/*********************************************************/


#ifndef DUMA_NO_GLOBAL_MALLOC_FREE

/*
 * define global functions for malloc(), free(), ..
 */

#ifdef _MSC_VER
/* define these functions as non-intrinsic */
#pragma function( memcpy, strcpy, strcat )
#endif


void * malloc(size_t size)
{
  return _duma_malloc(size  DUMA_PARAMS_UK);
}


void * calloc(size_t nelem, size_t elsize)
{
  return _duma_calloc(nelem, elsize  DUMA_PARAMS_UK);
}


void   free(void * address)
{
  _duma_free(address  DUMA_PARAMS_UK);
}


void * memalign(size_t alignment, size_t size)
{
  return _duma_memalign(alignment, size  DUMA_PARAMS_UK);
}


int    posix_memalign(void **memptr, size_t alignment, size_t size)
{
  return _duma_posix_memalign(memptr, alignment, size  DUMA_PARAMS_UK);
}


void * realloc(void * oldBuffer, size_t newSize)
{
  return _duma_realloc(oldBuffer, newSize  DUMA_PARAMS_UK);
}


void * valloc(size_t size)
{
  return _duma_valloc(size  DUMA_PARAMS_UK);
}


char * strdup(const char * str)
{
  return _duma_strdup(str  DUMA_PARAMS_UK);
}


void * memcpy(void *dest, const void *src, size_t size)
{
  return _duma_memcpy(dest, src, size  DUMA_PARAMS_UK);
}


char * strcpy(char *dest, const char *src)
{
  return _duma_strcpy(dest, src  DUMA_PARAMS_UK);
}


char * strncpy(char *dest, const char *src, size_t size)
{
  return _duma_strncpy(dest, src, size  DUMA_PARAMS_UK);
}


char * strcat(char *dest, const char *src)
{
  return _duma_strcat(dest, src  DUMA_PARAMS_UK);
}


char * strncat(char *dest, const char *src, size_t size)
{
  return _duma_strncat(dest, src, size  DUMA_PARAMS_UK);
}


#endif /* DUMA_NO_GLOBAL_MALLOC_FREE */


#ifndef DUMA_NO_LEAKDETECTION

/* Function DUMA_newFrame
 */
void  DUMA_newFrame(void)
{
}


/* Function DUMA_delFrame
 *
 * Will output DUMA message for all in use frames along with totals.
 * This method is called to when all memory should have been free'd by
 * the application to locate memory leaks.
 */
void  DUMA_delFrame(void)
{
  struct _DUMA_Slot * slot  = _duma_g.allocList;
  size_t        count     = _duma_s.slotCount;
  int    nonFreedTotal    = 0;
  int    nonFreedReported = 0;
  int    iExtraLeaks;

  IF__DUMA_INIT_DONE
    DUMA_GET_SEMAPHORE();

  Page_AllowAccess(_duma_g.allocList, _duma_s.allocListSize);

  for ( ; count > 0; --count, ++slot )
  {
    if ( DUMAST_IN_USE  == slot->state
      && EFA_INT_ALLOC  != slot->allocator
#ifdef DUMA_EXPLICIT_INIT
      && -1 !=  slot->lineno
#endif
       )
    {

      if ( _duma_s.REPORT_ALL_LEAKS || slot->lineno > 0 )
      {

#if defined(DUMA_DLL_LIBRARY) || defined(DUMA_SO_LIBRARY) || defined(DUMA_DETOURS)
      DUMA_Print("\nDUMA: ptr=0x%a size=%d type='%s' not freed\n"
                , (DUMA_ADDR)slot->userAddress, (DUMA_SIZE)slot->userSize
                , _duma_allocDesc[slot->allocator].name
                );
#else
      DUMA_Print("\nDUMA: ptr=0x%a size=%d type='%s' alloced from %s(%i) not freed\n"
                , (DUMA_ADDR)slot->userAddress, (DUMA_SIZE)slot->userSize
                , _duma_allocDesc[slot->allocator].name
                , slot->filename, slot->lineno
                );
#endif
#if defined(WIN32) && !defined(__CYGWIN__) && !defined(__MINGW32__) && !defined(__MINGW64__)
      if(DUMA_OUTPUT_STACKTRACE)
        DUMA_Print("Stacktrace of allocation:\n%s\n", slot->stacktrace);
#endif

        ++nonFreedReported;
      }

      ++nonFreedTotal;
    }
  }

  iExtraLeaks = nonFreedTotal - nonFreedReported;

  if ( nonFreedReported )
    DUMA_Abort("DUMA: Reported %i leaks. There are %i extra leaks without allocation information\n"
              , nonFreedReported, iExtraLeaks
              );
  else if ( nonFreedReported < nonFreedTotal )
    DUMA_Print("DUMA: Reported %i leaks. There are %i extra leaks without allocation information\n"
              , nonFreedReported, iExtraLeaks
              );


  Page_DenyAccess(_duma_g.allocList, _duma_s.allocListSize);

  IF__DUMA_INIT_DONE
  DUMA_RELEASE_SEMAPHORE(0);

  if (_duma_s.SHOW_ALLOC)
    DUMA_Print("\nDUMA: Processed %l allocations and %l deallocations in total.\n", _duma_s.numAllocs, _duma_s.numDeallocs);
}


/* Function: _duma_exit
 *
 * DUMA's exit function, called atexit() or with GNU C Compiler's destructor attribute.
 * This function also calls DUMA_delFrame to check for still in use memory and allert
 * the user.
 */
void
#if ( defined(DUMA_GNU_INIT_ATTR) && !defined(DUMA_PREFER_ATEXIT) )
__attribute ((destructor))
#endif
_duma_exit(void)
{
#if defined(WIN32) && !defined(__CYGWIN__) && !defined(__MINGW32__) && !defined(__MINGW64__)
  /* Cleanup memory owned by the stack library */
  /* wouldn't do to leak memory :) */
  StackTraceCleanup();
#endif

  DUMA_delFrame();
}


#endif /* end ifndef DUMA_NO_LEAKDETECTION */

#endif /* ifndef DUMA_NO_DUMA */

