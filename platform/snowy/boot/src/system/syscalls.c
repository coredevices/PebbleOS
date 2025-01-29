/* ----------------------------------------------------------------------------
 *         ATMEL Microcontroller Software Support
 * ----------------------------------------------------------------------------
 * Copyright (c) 2009, Atmel Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Atmel's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ----------------------------------------------------------------------------
 */

/**
  * \file syscalls.c
  *
  * Implementation of newlib syscall.
  *
  */

/*----------------------------------------------------------------------------
 *        Headers
 *----------------------------------------------------------------------------*/


#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "drivers/dbgserial.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/misc.h"

/*----------------------------------------------------------------------------
 *        Exported functions
 *----------------------------------------------------------------------------*/
extern void _kill( int pid, int sig ) ;
extern int _getpid ( void ) ;

void __malloc_lock (struct _reent *r) {
}

void __malloc_unlock (struct _reent *r) {
}

extern caddr_t _sbrk ( int incr ) {
  return (caddr_t) -1;
}

extern int link( char *old, char *new )
{
    (void)old;
    (void)new;
    return -1 ;
}

extern int _close( int file )
{
    (void)file;
    return -1 ;
}

extern int _open(const char *name, int flags, int mode) {
  (void) name;
  (void) flags;
  (void) mode;

  return -1;
}

extern int _fstat( int file, struct stat *st )
{
    (void)file;
    st->st_mode = S_IFCHR ;

    return 0 ;
}

extern int _isatty( int file )
{
    (void)file;
    return 1 ;
}

extern int _lseek( int file, int ptr, int dir )
{
    (void)file;
    (void)ptr;
    (void)dir;
    return 0 ;
}

extern int _read(int file, char *ptr, int len)
{
    (void)file;
    (void)ptr;
    (void)len;
    return 0 ;
}

extern int _write( int file, char *ptr, int len ) {
  (void) file;
  (void) ptr;
  (void) len;

  return 0;
}

extern void _exit( int status )
{
    PBL_CROAK("_exit");
    for ( ; ; ) ;
}

extern void _kill( int pid, int sig )
{
    PBL_CROAK("_kill");
    return;
}

extern int _getpid ( void )
{
    return -1 ;
}

// For compiling with GCC 4.8 Toolchain
// Wrapper for undefined __aeabi_memcpy and __aeabi_memcpy4
// these functions cannot be aliased, as memcpy returns int, these have no return
#if __GNUC__ >= 4 && __GNUC_MINOR__ > 7 // 4.7

void __aeabi_memcpy(void *dest, const void *src, size_t n){
  __builtin_memcpy (dest, src, n);
}

void __aeabi_memcpy4(void *dest, const void *src, size_t n){
  __builtin_memcpy (dest, src, n);
}

#endif

/* extern void _tzset_r(struct _reent *r) { */
/*   (void) r; */
/* } */
