/* ref: https://sourceware.org/newlib/libc.html#Stubs */

#include <errno.h>
#include <sys/stat.h>

void _close_r(void)
{
}
void _lseek_r(void)
{
}
void _read_r(void)
{
}
void _write_r(void)
{
}

int _fstat(int file, struct stat *st)
{
	(void)file;

	st->st_mode = S_IFCHR;
	return 0;
}

int _isatty(int file)
{
	(void)file;

	return 1;
}

int _getpid(void)
{
	return 1;
}

int _kill(int pid, int sig)
{
	(void)pid;
	(void)sig;

	errno = EINVAL;
	return -1;
}