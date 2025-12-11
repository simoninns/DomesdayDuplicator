/*
 ## Cypress USB 3.0 Platform source file (cyfxcppsyscall.cpp)
 ## ===========================
 ##
 ##  Copyright Cypress Semiconductor Corporation, 2010-2023,
 ##  All Rights Reserved
 ##  UNPUBLISHED, LICENSED SOFTWARE.
 ##
 ##  CONFIDENTIAL AND PROPRIETARY INFORMATION
 ##  WHICH IS THE PROPERTY OF CYPRESS.
 ##
 ##  Use of this file is governed
 ##  by the license agreement included in the file
 ##
 ##     <install>/license/license.txt
 ##
 ##  where <install> is the Cypress software
 ##  installation root directory path.
 ##
 ## ===========================
*/
/* 
 * This file defines the stub functions required by libc.
 */

#include <stdlib.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <errno.h>
#include "cyu3externcstart.h"

#undef errno
extern int errno;

char *__env[1] = { 0 };
char **environ = __env;

int _close(int file)
{
    return -1;
}

int _execve(char *name, char **argv, char **env)
{
    errno = ENOMEM;
    return -1;
}

void _exit( int code )
{
    /* Should we force a system reset? */
    while( 1 )
    {
        ;
    }
}

int _fork(void)
{
    errno = EAGAIN;
    return -1;
}

int _fstat(int file, struct stat *st)
{
    st->st_mode = S_IFCHR;
    return 0;
}

int _getpid(void)
{
    return 1;
}

int _isatty(int file)
{
    return 1;
}

int _kill(int pid, int sig)
{
    errno = EINVAL;
    return -1;
}

int _link(char *oldc, char *newc)
{
    errno = EMLINK;
    return -1;
}

int _lseek(int file, int ptr, int dir)
{
    return 0;
}

int _open(const char *name, int flags, int mode)
{
    return -1;
}

int _read(int file, char *ptr, int len)
{
    return 0;
}

char *heap_end = 0;;
caddr_t _sbrk(int incr)
{
    extern char __heap_start;
    extern char __heap_end;
    char *prev_heap_end;

    if (heap_end == 0) 
    {
        heap_end = (char *)&__heap_start;
    }
    prev_heap_end = heap_end;

    if (heap_end + incr > &__heap_end)
    {
        return (caddr_t) 0;
    }
    heap_end += incr;

    return (caddr_t) prev_heap_end;
}

int _stat(char *file, struct stat *st)
{
    st->st_mode = S_IFCHR;
    return 0;
}

int _times(struct tms *buf)
{
    return -1;
}

int _unlink(char *name)
{
    errno = ENOENT;
    return -1;
}

int _wait(int *status)
{
    errno = ECHILD;
    return -1;
}

int _write_r( void * reent, int file, char * ptr, int len )
{
    return 0;
}

void abort(void)
{
  while (1);
}

#include "cyu3externcend.h"
