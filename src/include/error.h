/********************************************************************************
 * MIT License
 *
 * Copyright (c) 2025 Christopher Gilliard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *******************************************************************************/

#ifndef _ERROR_H__
#define _ERROR_H__

/* Error codes for libfam */

#ifndef EPERM /* check for header already defining them */
#define SUCCESS 0
#define EPERM 1	  /* Operation not permitted */
#define ENOENT 2  /* No such file or directory */
#define ESRCH 3	  /* No such process */
#define EINTR 4	  /* Interrupted system call */
#define EIO 5	  /* I/O error */
#define ENXIO 6	  /* No such device or address */
#define E2BIG 7	  /* Argument list too long */
#define ENOEXEC 8 /* Exec format error */
#define EBADF 9	  /* Bad file number */
#define ECHILD 10 /* No child processes */
#ifdef __linux__
#define EAGAIN 11 /* Try again */
#endif		  /* __linux__ */
#ifdef __APPLE__
#define EDEADLK 11 /* Resource deadlock avoided */
#endif		   /* __APPLE__ */
#define ENOMEM 12  /* Out of memory */
#define EACCES 13  /* Permission denied */
#define EFAULT 14  /* Bad address */
#define ENOTBLK 15 /* Block device required */
#define EBUSY 16   /* Device or resource busy */
#define EEXIST 17  /* File exists */
#define EXDEV 18   /* Cross-device link */
#define ENODEV 19  /* No such device */
#define ENOTDIR 20 /* Not a directory */
#define EISDIR 21  /* Is a directory */
#define EINVAL 22  /* Invalid argument */
#define ENFILE 23  /* File table overflow */
#define EMFILE 24  /* Too many open files */
#define ENOTTY 25  /* Not a typewriter */
#define ETXTBSY 26 /* Text file busy */
#define EFBIG 27   /* File too large */
#define ENOSPC 28  /* No space left on device */
#define ESPIPE 29  /* Illegal seek */
#define EROFS 30   /* Read-only file system */
#define EMLINK 31  /* Too many links */
#define EPIPE 32   /* Broken pipe */
#define EDOM 33	   /* Math argument out of domain of func */
#define ERANGE 34  /* Math result not representable */
#ifdef __linux__
#define EDEADLK 35 /* Resource deadlock would occur */
#endif		   /* __linux__ */
#ifdef __APPLE__
#define EAGAIN 35	/* Resource deadlock would occur */
#endif			/* __APPLE__ */
#define ENAMETOOLONG 36 /* File name too long */
#define ENOLCK 37	/* No record locks available */
#define ENOSYS 38	/* Invalid system call number */
#define ENOTEMPTY 39	/* Directory not empty */
#define ELOOP 40	/* Too many symbolic links encountered */
#define EWOULDBLOCK EAGAIN

/* libfam specific codes */
#define EOVERFLOW 200
#endif /* EPERM */

extern int err;

const char *error_string(int err_code);
void perror(const char *);

#endif /* _ERROR_H__ */
