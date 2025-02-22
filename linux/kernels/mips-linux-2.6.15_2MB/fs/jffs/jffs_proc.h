/*
 * JFFS -- Journaling Flash File System, Linux implementation.
 *
 * Copyright (C) 2000  Axis Communications AB.
 *
 * Created by Simon Kagstrom <simonk@axis.com>.
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Id: //depot/sw/releases/Aquila_9.2.0_U11/linux/kernels/mips-linux-2.6.15_2MB/fs/jffs/jffs_proc.h#1 $
 */

/* jffs_proc.h defines a structure for inclusion in the proc-file system.  */
#ifndef __LINUX_JFFS_PROC_H__
#define __LINUX_JFFS_PROC_H__

#include <linux/proc_fs.h>

/* The proc_dir_entry for jffs (defined in jffs_proc.c).  */
extern struct proc_dir_entry *jffs_proc_root;

int jffs_register_jffs_proc_dir(int mtd, struct jffs_control *c);
int jffs_unregister_jffs_proc_dir(struct jffs_control *c);

#endif /* __LINUX_JFFS_PROC_H__ */
