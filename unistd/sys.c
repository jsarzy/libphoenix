/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * unistd
 *
 * Copyright 2018 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <sys/types.h>
#include <sys/threads.h>
#include <sys/msg.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>


WRAP_ERRNO_DEF(int, setpgid, (pid_t pid, pid_t pgid), (pid, pgid))
WRAP_ERRNO_DEF(int, setpgrp, (void), ())

WRAP_ERRNO_DEF(pid_t, getpgid, (pid_t pid), (pid))
WRAP_ERRNO_DEF(pid_t, getpgrp, (void), ())

WRAP_ERRNO_DEF(int, setsid, (void), ())

pid_t getsid(pid_t pid)
{
	return getpgid(pid);
}


char exec_buffer[256];


static int shebang(const char *path)
{
	int fd;
	char sb[2] = {0, 0};

	if ((fd = open(path, O_RDONLY)) < 0)
		return -1;

	if ((read(fd, sb, 2) < 0) || sb[0] != '#' || sb[1] != '!') {
		close(fd);
		return -1;
	}

	return fd;
}


int execv(const char *path, char *const argv[])
{
	return execve(path, argv, environ);
}

#define PATH_DELIM ':'

int execve(const char *file, char *const argv[], char *const envp[])
{
	int fd, noargs = 0, err;
	char *interp = exec_buffer, *end, **sb_args = NULL;
	char *canonical_path;
	char *path = getenv("PATH");
	int filename_len = strlen(file);
	oid_t dir;

	fflush(NULL);

	if (!strchr(file, '/') && path) {
		while (*path) {
			char* delim = strchrnul(path, ':');

			int path_len = delim - path;
			if (path_len + 1 + filename_len + 1 <= sizeof(exec_buffer)) {
				memcpy(exec_buffer, path, path_len);
				exec_buffer[path_len] = '/';
				memcpy(exec_buffer + path_len + 1, file, filename_len);
				exec_buffer[path_len + 1 + filename_len] = '\0';

				if (lookup(exec_buffer, NULL, &dir) >= 0) {
					file = exec_buffer;
					break;
				}
			}

			path = delim;
			if (*path == PATH_DELIM) path++;
		}
	}

	if ((fd = shebang(file)) >= 0) {
		if (read(fd, exec_buffer, sizeof(exec_buffer)) < 0) {
			close(fd);
			return SET_ERRNO(-EIO);
		}

		exec_buffer[sizeof(exec_buffer) - 1] = 0;

		while (*interp == ' ') ++interp;
		end = interp;
		while (*end && !isspace(*end)) ++end;
		*end = 0;

		while (argv[noargs++] != NULL) ;

		if ((sb_args = calloc(noargs + 1, sizeof(char *))) == NULL) {
			close(fd);
			return -ENOMEM;
		}

		while (noargs-- > 0)
			sb_args[noargs + 1] = argv[noargs];

		sb_args[0] = interp;

		close(fd);
		file = interp;
		argv = sb_args;
	}

	if ((canonical_path = canonicalize_file_name(file)) == NULL)
		return SET_ERRNO(-ENOMEM);

	err = exec(canonical_path, argv, envp);
	free(canonical_path);
	free(sb_args);

	return SET_ERRNO(err);
}


int execvp(const char *file, char *const argv[])
{
	return execvpe(file, argv, environ);
}

int execvpe(const char *file, char *const argv[], char *const envp[])
{
	/* FIXME: search for file in env PATH */
	return execve(file, argv, envp);
}


static char **argv_gather(const char *arg, va_list args)
{
	int argc = 0, max = 8;
	char **argv, **res;

	if ((argv = malloc(max * sizeof(char *))) == NULL)
		return NULL;

	argv[argc++] = (char *)arg;

	while ((argv[argc++] = va_arg(args, char *)) != NULL) {
		if (argc == max) {
			if ((res = realloc(argv, max *= 2)) == NULL) {
				free(argv);
				return NULL;
			}
			argv = res;
		}
	}

	return argv;
}


int execl(const char *path, const char *arg, ...)
{
	va_list args;
	char **argv;
	int err;

	va_start(args, arg);
	argv = argv_gather(arg, args);
	va_end(args);

	if (argv == NULL)
		return -ENOMEM;

	err = execve(path, argv, environ);
	free(argv);
	return err;
}

int execlp(const char *file, const char *arg, ...)
{
	va_list args;
	char **argv;
	int err;

	/* FIXME: search for file in env PATH */

	va_start(args, arg);
	argv = argv_gather(arg, args);
	va_end(args);

	if (argv == NULL)
		return -ENOMEM;

	err = execve(file, argv, environ);
	free(argv);
	return err;
}


int execle(const char *path, const char *arg, ...)
{
	va_list args;
	char **argv, **envp;
	int err;

	va_start(args, arg);
	argv = argv_gather(arg, args);
	envp = va_arg(args, char **);
	va_end(args);

	if (argv == NULL)
		return -ENOMEM;

	err = execve(path, argv, envp);
	free(argv);
	return err;
}


unsigned sleep(unsigned seconds)
{
	return usleep(seconds * 1000 * 1000);
}


extern pid_t sys_fork(void);
extern void release(void);


pid_t fork(void)
{
	pid_t pid;
	if (!(pid = sys_fork()))
		release();
	else if (pid < 0) {
		return SET_ERRNO(pid);
	}
	return pid;
}
