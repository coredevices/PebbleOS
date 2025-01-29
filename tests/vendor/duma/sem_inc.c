
/*
 * DUMA - Red-Zone memory allocator.
 * Copyright (C) 2002-2008 Hayati Ayguen <h_ayguen@web.de>, Procitec GmbH
 * License: GNU LGPL (GNU Lesser General Public License, see COPYING-GPL)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * FILE CONTENTS:
 * internal implementation file
 * contains thread safety functions (semaphore lock/release)
 */

#include "duma_config.h"
#include "duma_sem.h"
#include "print.h"
#include <stdlib.h>

#ifndef DUMA_NO_THREAD_SAFETY

/* check for pthread library */
/* use WIN32_SEMAPHORES on Win32-Cygwin,
 * with this configuration testmt.c works either with pthreads and with the Win32 API
 */
/* || defined(__CYGWIN__) || defined(__MINGW32__) || defined(__MINGW64__)) */
#if (!defined(WIN32))
#define HAVE_PTHREADS         1
#define USE_WIN32_SEMAPHORES  0
#define USE_WIN32_CRIT_SECT   0
#else
#define HAVE_PTHREADS         0
#define USE_WIN32_SEMAPHORES  1
#define USE_WIN32_CRIT_SECT   0
#endif

#if HAVE_PTHREADS
  #include <pthread.h>
  #include <semaphore.h>
#elif USE_WIN32_SEMAPHORES || USE_WIN32_CRIT_SECT
  #define WIN32_LEAN_AND_MEAN 1
  #include <windows.h>
  #include <winbase.h>
#endif


/*
 * DUMA_sem is a semaphore used to allow one thread at a time into
 * these routines.
 * Also, we use semInited as a boolean to see if we should be
 * using the semaphore.
 * semThread is set to the thread id of the thread that currently
 * has the semaphore so that when/if it tries to get the semaphore
 * again (realloc calling malloc/free) - nothing will happen to the
 * semaphore.
 * semDepth is used to keep track of how many times the same thread
 * gets the semaphore - so we know when it is actually freed.
 */


#if HAVE_PTHREADS

  #define DUMA_thread_self()  pthread_self()

  #ifndef DUMA_SEMAPHORES
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    static pthread_t mutextid=0;
    static int locknr=0;
  #else
    static sem_t      DUMA_sem = { 0 };
    static pthread_t  semThread = (pthread_t) 0;
  #endif

#elif USE_WIN32_SEMAPHORES

  #define DUMA_thread_self()  GetCurrentThreadId()

  #ifndef UNICODE
    #define SEM_NAME_TYPE char
    #define SEM_STRCPY    strcpy
    #define SEM_STRCAT    strcat
    static char    semObjectName[] = "DUMA_";
  #else
    #define SEM_NAME_TYPE wchar_t
    #define SEM_STRCPY    wcscpy
    #define SEM_STRCAT    wcscat
    static wchar_t semObjectName[] = L"DUMA_";
  #endif

  static SECURITY_ATTRIBUTES  semSecAttr;
  static DWORD      semThread = 0;
  static HANDLE     semHandle = 0;

#elif USE_WIN32_CRIT_SECT

  /* see http://msdn.microsoft.com/en-us/library/ms682530(VS.85).aspx */
  static CRITICAL_SECTION critsect;

#endif


static int semInInit = 0;

#if HAVE_PTHREADS && !defined(DUMA_SEMAPHORES)
static int semInited = 1;
static int semDepth  = 0;
#elif USE_WIN32_SEMAPHORES
static int semInited = 0;
static int semDepth  = 0;
#elif USE_WIN32_CRIT_SECT
static int semInited = 0;
#endif

#if HAVE_PTHREADS

#ifndef DUMA_SEMAPHORES

static void lock()
{
  if (pthread_mutex_trylock(&mutex))
  {
    if ( mutextid==pthread_self() )
    {
      ++locknr;
      return;
    }
    else
    {
      pthread_mutex_lock(&mutex);
    }
  } 
  mutextid=pthread_self();
  locknr=1;
}

static void unlock()
{
  --locknr;
  if (!locknr)
  {
    mutextid=0;
    pthread_mutex_unlock(&mutex);
  }
}

#endif
#endif


void
DUMA_init_sem(void)
{
#if USE_WIN32_SEMAPHORES
  SEM_NAME_TYPE   semLocalName[32];
  SEM_NAME_TYPE   acPID[16];
  DWORD pid;
#endif

  /* avoid recursive call to sem_init(),
   * when sem_init() calls malloc() or other allocation function
   */
  if (semInited || semInInit)
    return;
  semInInit = 1;

#if HAVE_PTHREADS
  #ifndef DUMA_SEMAPHORES
    pthread_mutex_init(&mutex, NULL);
    semInited = 1;
  #else
    if (sem_init(&DUMA_sem, 0, 1) >= 0)
      semInited = 1;
  #endif
#elif USE_WIN32_SEMAPHORES
  pid = GetCurrentProcessId();
  SEM_STRCPY(semLocalName, semObjectName);
  /* append ProcessId() to get inter-process unique semaphore name */
  acPID[0] = 'A' + (SEM_NAME_TYPE)( (pid >> 28) & 0x0F );
  acPID[1] = 'A' + (SEM_NAME_TYPE)( (pid >> 24) & 0x0F );
  acPID[2] = 'A' + (SEM_NAME_TYPE)( (pid >> 20) & 0x0F );
  acPID[3] = 'A' + (SEM_NAME_TYPE)( (pid >> 16) & 0x0F );
  acPID[4] = 'A' + (SEM_NAME_TYPE)( (pid >> 12) & 0x0F );
  acPID[5] = 'A' + (SEM_NAME_TYPE)( (pid >>  8) & 0x0F );
  acPID[6] = 'A' + (SEM_NAME_TYPE)( (pid >>  4) & 0x0F );
  acPID[7] = 'A' + (SEM_NAME_TYPE)( (pid      ) & 0x0F );
  acPID[8] = 0;
  SEM_STRCAT( semLocalName, acPID );

  semSecAttr.nLength = sizeof(semSecAttr);
  semSecAttr.lpSecurityDescriptor = NULL;
  semSecAttr.bInheritHandle = FALSE;

  semHandle = CreateSemaphore( &semSecAttr      /* pointer to security attributes */
                               , 1              /* initial count */
                               , 1              /* maximum count */
                               , semLocalName   /* pointer to semaphore-object name */
                             );
  semInited = 1;
#elif USE_WIN32_CRIT_SECT
  InitializeCriticalSection(&critsect);
  semInited = 1;
#endif

  semInInit = 0;

  if (!semInited)     DUMA_Abort("\nCouldn't initialise semaphore");
}


void DUMA_get_sem(void)
{
  if (semInInit)      return;             /* avoid recursion */
  if (!semInited)     DUMA_init_sem();    /* initialize if necessary */

#if HAVE_PTHREADS
  #ifndef DUMA_SEMAPHORES
    lock();
  #else
    if (semThread != DUMA_thread_self())
    {
      while (sem_wait(&DUMA_sem) < 0);   /* wait for the semaphore. */
      semThread = DUMA_thread_self();     /* let everyone know who has the semaphore. */
    }
  #endif
  ++semDepth;                         /* increment semDepth - push one stack level */
#elif USE_WIN32_SEMAPHORES
  if (semThread != DUMA_thread_self())
  {
    while (WaitForSingleObject(semHandle, 1000) != WAIT_OBJECT_0) ; /* wait for the semaphore. */
    semThread = DUMA_thread_self();     /* let everyone know who has the semaphore. */
  }
  ++semDepth;                         /* increment semDepth - push one stack level */
#elif USE_WIN32_CRIT_SECT
  EnterCriticalSection(&critsect);
#endif
}


int DUMA_rel_sem(int retval)
{
  if (semInInit)      return retval;  /* avoid recursion */
  if (!semInited)     DUMA_Abort("\nSemaphore isn't initialised");

#ifdef DUMA_SEMAPHORES
  if (!semThread)     DUMA_Abort("\nSemaphore isn't owned by this thread");
#endif

#if HAVE_PTHREADS || USE_WIN32_SEMAPHORES
  if (semDepth <= 0)  DUMA_Abort("\nSemaphore isn't locked");
#endif

  --semDepth;              /* decrement semDepth - popping one stack level */
#if HAVE_PTHREADS
  #ifndef DUMA_SEMAPHORES
    unlock();
  #else
    semThread = (pthread_t) 0;        /* zero this before actually free'ing the semaphore. */
    if (sem_post(&DUMA_sem) < 0)
      DUMA_Abort("Failed to post the semaphore.");
  #endif
#elif USE_WIN32_SEMAPHORES
  semThread = 0;                    /* zero this before actually free'ing the semaphore. */
  if (0 == ReleaseSemaphore(semHandle, 1 /* amount to add to current count */, NULL) )
    DUMA_Abort("Failed to post the semaphore.");
#elif USE_WIN32_CRIT_SECT
  LeaveCriticalSection(&critsect);
#endif
  return retval;
}

#else

/* for not having an empty file */
static int dummy = 0;

#endif /* DUMA_NO_THREAD_SAFETY */

