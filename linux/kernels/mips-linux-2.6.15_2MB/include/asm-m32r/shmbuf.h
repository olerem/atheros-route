#ifndef _ASM_M32R_SHMBUF_H
#define _ASM_M32R_SHMBUF_H

/* $Id: //depot/sw/releases/Aquila_9.2.0_U11/linux/kernels/mips-linux-2.6.15_2MB/include/asm-m32r/shmbuf.h#1 $ */

/* orig : i386 2.4.18 */

/*
 * The shmid64_ds structure for M32R architecture.
 * Note extra padding because this structure is passed back and forth
 * between kernel and user space.
 *
 * Pad space is left for:
 * - 64-bit time_t to solve y2038 problem
 * - 2 miscellaneous 32-bit values
 */

struct shmid64_ds {
	struct ipc64_perm	shm_perm;	/* operation perms */
	size_t			shm_segsz;	/* size of segment (bytes) */
	__kernel_time_t		shm_atime;	/* last attach time */
	unsigned long		__unused1;
	__kernel_time_t		shm_dtime;	/* last detach time */
	unsigned long		__unused2;
	__kernel_time_t		shm_ctime;	/* last change time */
	unsigned long		__unused3;
	__kernel_pid_t		shm_cpid;	/* pid of creator */
	__kernel_pid_t		shm_lpid;	/* pid of last operator */
	unsigned long		shm_nattch;	/* no. of current attaches */
	unsigned long		__unused4;
	unsigned long		__unused5;
};

struct shminfo64 {
	unsigned long	shmmax;
	unsigned long	shmmin;
	unsigned long	shmmni;
	unsigned long	shmseg;
	unsigned long	shmall;
	unsigned long	__unused1;
	unsigned long	__unused2;
	unsigned long	__unused3;
	unsigned long	__unused4;
};

#endif /* _ASM_M32R_SHMBUF_H */
