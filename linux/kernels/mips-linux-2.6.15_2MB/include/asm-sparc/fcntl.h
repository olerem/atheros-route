/* $Id: //depot/sw/releases/Aquila_9.2.0_U11/linux/kernels/mips-linux-2.6.15_2MB/include/asm-sparc/fcntl.h#1 $ */
#ifndef _SPARC_FCNTL_H
#define _SPARC_FCNTL_H

/* open/fcntl - O_SYNC is only implemented on blocks devices and on files
   located on an ext2 file system */
#define O_APPEND	0x0008
#define FASYNC		0x0040	/* fcntl, for BSD compatibility */
#define O_CREAT		0x0200	/* not fcntl */
#define O_TRUNC		0x0400	/* not fcntl */
#define O_EXCL		0x0800	/* not fcntl */
#define O_SYNC		0x2000
#define O_NONBLOCK	0x4000
#define O_NDELAY	(0x0004 | O_NONBLOCK)
#define O_NOCTTY	0x8000	/* not fcntl */
#define O_LARGEFILE	0x40000
#define O_DIRECT        0x100000 /* direct disk access hint */
#define O_NOATIME	0x200000

#define F_GETOWN	5	/*  for sockets. */
#define F_SETOWN	6	/*  for sockets. */
#define F_GETLK		7
#define F_SETLK		8
#define F_SETLKW	9

/* for posix fcntl() and lockf() */
#define F_RDLCK		1
#define F_WRLCK		2
#define F_UNLCK		3

#define __ARCH_FLOCK_PAD	short __unused;
#define __ARCH_FLOCK64_PAD	short __unused;

#include <asm-generic/fcntl.h>

#endif
