/*
 * Squashfs - a compressed read only filesystem for Linux
 *
 * Copyright (c) 2002, 2003, 2004, 2005, 2006, 2007
 * Phillip Lougher <phillip@lougher.demon.co.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * inode.c
 */

#include <linux/types.h>
#include <linux/squashfs_fs.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/zlib.h>
#include <linux/fs.h>
#include <linux/smp_lock.h>
#include <linux/slab.h>
#include <linux/squashfs_fs_sb.h>
#include <linux/squashfs_fs_i.h>
#include <linux/buffer_head.h>
#include <linux/vfs.h>
#include <linux/init.h>
#include <linux/dcache.h>
#include <linux/wait.h>
#include <linux/blkdev.h>
#include <linux/vmalloc.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>

#ifdef CONFIG_SQUASHFS_MTD
#include <linux/mtd/mtd.h>
#include <linux/namei.h>
#include <linux/mount.h>
#include <linux/ctype.h>
#endif

#include "squashfs.h"
#include "sqlzma.h"
#include "sqmagic.h"

#undef KeepPreemptive
#if defined(CONFIG_PREEMPT) && !defined(UnsquashNoPreempt)
#define KeepPreemptive
#endif

struct sqlzma {
#ifdef KeepPreemptive
	struct mutex mtx;
#endif
	unsigned char read_data[SQUASHFS_FILE_MAX_SIZE];
	struct sqlzma_un un;
};
static DEFINE_PER_CPU(struct sqlzma *, sqlzma);

#define dpri(fmt, args...)   // printk("%s:%d: " fmt, __func__, __LINE__, ##args)
#define dpri_un(un)	dpri("un{%d, {%d %p}, {%d %p}, {%d %p}}\n", \
			     (un)->un_lzma, (un)->un_a[0].sz, (un)->un_a[0].buf, \
			     (un)->un_a[1].sz, (un)->un_a[1].buf, \
			     (un)->un_a[2].sz, (un)->un_a[2].buf)

static int squashfs_cached_blks;

static void vfs_read_inode(struct inode *i);
static struct dentry *squashfs_get_parent(struct dentry *child);
static int squashfs_read_inode(struct inode *i, squashfs_inode_t inode);
static int squashfs_statfs(struct super_block *, struct kstatfs *);
static int squashfs_symlink_readpage(struct file *file, struct page *page);
static long long read_blocklist(struct inode *inode, int index,
				int readahead_blks, char *block_list,
				unsigned short **block_p, unsigned int *bsize);
static int squashfs_readpage(struct file *file, struct page *page);
static int squashfs_readdir(struct file *, void *, filldir_t);
static struct dentry *squashfs_lookup(struct inode *, struct dentry *,
				struct nameidata *);
static int squashfs_remount(struct super_block *s, int *flags, char *data);
static void squashfs_put_super(struct super_block *);
static struct super_block *squashfs_get_sb(struct file_system_type *,int, const char *, void *);
static struct inode *squashfs_alloc_inode(struct super_block *sb);
static void squashfs_destroy_inode(struct inode *inode);
static int init_inodecache(void);
static void destroy_inodecache(void);
#ifdef CONFIG_SQUASHFS_MTD
static void squashfs_kill_sb(struct super_block *sb);
#endif

static struct file_system_type squashfs_fs_type = {
	.owner = THIS_MODULE,
	.name = "squashfs",
	.get_sb = squashfs_get_sb,
#ifdef CONFIG_SQUASHFS_MTD
        .kill_sb = squashfs_kill_sb,
#else
	.kill_sb = kill_block_super,
#endif
	.fs_flags = FS_REQUIRES_DEV
};

static unsigned char squashfs_filetype_table[] = {
	DT_UNKNOWN, DT_DIR, DT_REG, DT_LNK, DT_BLK, DT_CHR, DT_FIFO, DT_SOCK
};

static struct super_operations squashfs_super_ops = {
	.alloc_inode = squashfs_alloc_inode,
	.destroy_inode = squashfs_destroy_inode,
	.statfs = squashfs_statfs,
	.put_super = squashfs_put_super,
	.remount_fs = squashfs_remount
};

static struct super_operations squashfs_export_super_ops = {
	.alloc_inode = squashfs_alloc_inode,
	.destroy_inode = squashfs_destroy_inode,
	.statfs = squashfs_statfs,
	.put_super = squashfs_put_super,
	.read_inode = vfs_read_inode
};

static struct export_operations squashfs_export_ops = {
	.get_parent = squashfs_get_parent
};

SQSH_EXTERN struct address_space_operations squashfs_symlink_aops = {
	.readpage = squashfs_symlink_readpage
};

SQSH_EXTERN struct address_space_operations squashfs_aops = {
	.readpage = squashfs_readpage
};

static struct file_operations squashfs_dir_ops = {
	.read = generic_read_dir,
	.readdir = squashfs_readdir
};

SQSH_EXTERN struct inode_operations squashfs_dir_inode_ops = {
	.lookup = squashfs_lookup
};

#ifdef CONFIG_SQUASHFS_MTD

#define SQUASHFS_MTD_OFFSET(index, msblk) (index * msblk->devblksize)
#define SQUASHFS_BUF_FREE(buf) kfree(buf)
#define SQUASHFS_GET_BLK(s, index) squashfs_getblk_mtd(s, (index))

static int squashfs_mtd_read(struct super_block *s, int index, 
                      size_t *retlen ,u_char *buf)
{
	struct squashfs_sb_info *msblk = s->s_fs_info;
        struct mtd_info *mtd = msblk->mtd;
        int ret = 0;
        loff_t off = SQUASHFS_MTD_OFFSET(index, msblk) ;

        TRACE("%s(): issuing mtd from offset: %lld on mtd->index:%d mtd->name:%s msblk=%p mtd=%p s=%p \n", __func__, off, mtd->index, mtd->name, msblk, mtd, s);

        ret = mtd->read(mtd, off, msblk->devblksize, retlen, buf);

        TRACE("%s(): mtd->read result:%d retlen:0x%x\n", __func__, ret, *retlen);

        if (ret != 0) {
            printk("%s(): mtd read failed err %d\n", __func__, ret);
            return ret;
        }

        return 0;
}

u_char *squashfs_getblk_mtd(struct super_block *s, int index)
{
	struct squashfs_sb_info *msblk = s->s_fs_info;
        u_char  *buf;
        size_t retlen;
        int ret=0;

        buf = (u_char *)NULL;

	buf = kmalloc(msblk->devblksize, GFP_KERNEL);

        if (!buf)
            return NULL;


        ret = squashfs_mtd_read(s, index, &retlen, buf);


        if (ret != 0)
        {
            return NULL;
        }

        return buf;

}

static u_char *get_block_length(struct super_block *s,
				int *cur_index, int *offset, int *c_byte)
{
	struct squashfs_sb_info *msblk = s->s_fs_info;
	unsigned short temp;
        u_char  *buf;
        size_t retlen;
        int ret=0;

        TRACE("Entering %s()\n", __func__);

        buf = (u_char *)NULL;

	buf = kzalloc(msblk->devblksize, GFP_KERNEL);

        if (!buf)
            goto out;

        ret = squashfs_mtd_read(s, *cur_index, &retlen, buf);

        if (ret != 0)
            goto out;

	if (msblk->devblksize - *offset == 1) {
		if (msblk->swap)
			((unsigned char *) &temp)[1] = *((unsigned char *)
				(buf + *offset));
		else
			((unsigned char *) &temp)[0] = *((unsigned char *)
				(buf + *offset));


		if (squashfs_mtd_read(s, ++(*cur_index), &retlen, buf) != 0)
			goto out;

		if (msblk->swap)
			((unsigned char *) &temp)[0] = *((unsigned char *)
				buf);
		else
			((unsigned char *) &temp)[1] = *((unsigned char *)
				buf);
		*c_byte = temp;
		*offset = 1;
	} else {
		if (msblk->swap) {
			((unsigned char *) &temp)[1] = *((unsigned char *)
				(buf + *offset));
			((unsigned char *) &temp)[0] = *((unsigned char *)
				(buf + *offset + 1));
		} else {
			((unsigned char *) &temp)[0] = *((unsigned char *)
				(buf + *offset));
			((unsigned char *) &temp)[1] = *((unsigned char *)
				(buf + *offset + 1));
		}
		*c_byte = temp;
		*offset += 2;
	}

	if (SQUASHFS_CHECK_DATA(msblk->sblk.flags)) {
		if (*offset == msblk->devblksize) {
			if (squashfs_mtd_read(s, ++(*cur_index), &retlen, buf) != 0)
				goto out;
			*offset = 0;
		}
		if (*((unsigned char *) (buf + *offset)) !=
						SQUASHFS_MARKER_BYTE) {
			ERROR("Metadata block marker corrupt @ %x\n",
						*cur_index);
			goto out;
		}
		(*offset)++;
	}

        return buf;

out:
        if (buf != (u_char *)NULL)
                kfree(buf);
        return NULL;

}

#else

#define SQUASHFS_BUF_FREE(buf) brelse(buf)
#define SQUASHFS_GET_BLK(s, index) sb_getblk(s, index)

static struct buffer_head *get_block_length(struct super_block *s,
				int *cur_index, int *offset, int *c_byte)
{
	struct squashfs_sb_info *msblk = s->s_fs_info;
	unsigned short temp;
	struct buffer_head *bh;

	if (!(bh = sb_bread(s, *cur_index)))
		goto out;

	if (msblk->devblksize - *offset == 1) {
		if (msblk->swap)
			((unsigned char *) &temp)[1] = *((unsigned char *)
				(bh->b_data + *offset));
		else
			((unsigned char *) &temp)[0] = *((unsigned char *)
				(bh->b_data + *offset));
		brelse(bh);
		if (!(bh = sb_bread(s, ++(*cur_index))))
			goto out;
		if (msblk->swap)
			((unsigned char *) &temp)[0] = *((unsigned char *)
				bh->b_data); 
		else
			((unsigned char *) &temp)[1] = *((unsigned char *)
				bh->b_data); 
		*c_byte = temp;
		*offset = 1;
	} else {
		if (msblk->swap) {
			((unsigned char *) &temp)[1] = *((unsigned char *)
				(bh->b_data + *offset));
			((unsigned char *) &temp)[0] = *((unsigned char *)
				(bh->b_data + *offset + 1)); 
		} else {
			((unsigned char *) &temp)[0] = *((unsigned char *)
				(bh->b_data + *offset));
			((unsigned char *) &temp)[1] = *((unsigned char *)
				(bh->b_data + *offset + 1)); 
		}
		*c_byte = temp;
		*offset += 2;
	}

	if (SQUASHFS_CHECK_DATA(msblk->sblk.flags)) {
		if (*offset == msblk->devblksize) {
			brelse(bh);
			if (!(bh = sb_bread(s, ++(*cur_index))))
				goto out;
			*offset = 0;
		}
		if (*((unsigned char *) (bh->b_data + *offset)) !=
						SQUASHFS_MARKER_BYTE) {
			ERROR("Metadata block marker corrupt @ %x\n",
						*cur_index);
			brelse(bh);
			goto out;
		}
		(*offset)++;
	}
	return bh;

out:
	return NULL;
}
#endif


SQSH_EXTERN unsigned int squashfs_read_data(struct super_block *s, char *buffer,
			long long index, unsigned int length,
			long long *next_index, int srclength)
{
	struct squashfs_sb_info *msblk = s->s_fs_info;
	struct squashfs_super_block *sblk = &msblk->sblk;
	unsigned int offset = index & ((1 << msblk->devblksize_log2) - 1);
	unsigned int cur_index = index >> msblk->devblksize_log2;
	unsigned int bytes, avail_bytes, b = 0, k = 0;
	unsigned int compressed;
	unsigned int c_byte = length;

        TRACE("Entering %s\n", __func__);

#ifdef CONFIG_SQUASHFS_MTD
        u_char **bh;
        bh = kmalloc((((sblk->block_size >> msblk->devblksize_log2) + 1) * sizeof(u_char *)), GFP_KERNEL);
#else
	struct buffer_head **bh;
	bh = kmalloc(((sblk->block_size >> msblk->devblksize_log2) + 1) *
								sizeof(struct buffer_head *), GFP_KERNEL);
#endif
	if (bh == NULL)
		goto read_failure;

	if (c_byte) {
		bytes = msblk->devblksize - offset;
		compressed = SQUASHFS_COMPRESSED_BLOCK(c_byte);
		c_byte = SQUASHFS_COMPRESSED_SIZE_BLOCK(c_byte);

		TRACE("Block @ 0x%llx, %scompressed size %d, src size %d\n", index,
					compressed ? "" : "un", (unsigned int) c_byte, srclength);

		if (c_byte > srclength || index < 0 || (index + c_byte) > sblk->bytes_used)
			goto read_failure;


                bh[0] = SQUASHFS_GET_BLK(s, cur_index);
                //TRACE("%s(%s):%d after getblk \n", __FILE__, __func__, __LINE__);

		if (bh[0] == NULL)
			goto block_release;

		for (b = 1; bytes < c_byte; b++) {
                        bh[b] = SQUASHFS_GET_BLK(s, ++cur_index);
                        //TRACE("%s(%s):%d after getblk \n", __FILE__, __func__, __LINE__);
			if (bh[b] == NULL)
				goto block_release;
			bytes += msblk->devblksize;
		}
#ifndef CONFIG_SQUASHFS_MTD
		ll_rw_block(READ, b, bh);
#endif
	} else {
		if (index < 0 || (index + 2) > sblk->bytes_used)
			goto read_failure;
		bh[0] = get_block_length(s, &cur_index, &offset, &c_byte);
		if (bh[0] == NULL)
			goto read_failure;

		bytes = msblk->devblksize - offset;
		compressed = SQUASHFS_COMPRESSED(c_byte);
		c_byte = SQUASHFS_COMPRESSED_SIZE(c_byte);

		TRACE("Block @ 0x%llx, %scompressed size %d src size %d cur_index %d\n\n", index, compressed
					? "" : "un", (unsigned int) c_byte, srclength, cur_index);

		if (c_byte > srclength || (index + c_byte) > sblk->bytes_used)
			goto read_failure;

		for (b = 1; bytes < c_byte; b++) {
			bh[b] = SQUASHFS_GET_BLK(s, ++cur_index);
                        // TRACE("%s(%s):%d after getblk \n", __FILE__, __func__, __LINE__);
			if (bh[b] == NULL)
				goto block_release;
			bytes += msblk->devblksize;
		}
#ifndef CONFIG_SQUASHFS_MTD
		ll_rw_block(READ, b - 1, bh + 1);
#endif
	}

        // TRACE("%s(%s) before if (compressed) \n", __FILE__, __func__);

	if (compressed) {
		int zlib_err = Z_STREAM_END;
		int start;
		enum {Src, Dst};
		struct sized_buf sbuf[2];
		struct sqlzma *percpu;
		unsigned char *p;

		/*
	 	* uncompress block
	 	*/
		down(&msblk->read_data_mutex);
                start = k;
#ifndef CONFIG_SQUASHFS_MTD
	        for (; k < b; k++) {
                    wait_on_buffer(bh[k]);
                    if (!buffer_uptodate(bh[k]))
                            goto release_mutex;
                }
#endif
		/* it disables preemption */
		percpu = get_cpu_var(sqlzma);
#ifdef KeepPreemptive
		put_cpu_var(sqlzma);
		mutex_lock(&percpu->mtx);
#endif
		p = percpu->read_data;
		k = start;
		for (bytes = 0; k < b; k++) {
			avail_bytes = min(c_byte - bytes, msblk->devblksize - offset);

			if (k == 0) {
#ifndef CONFIG_SQUASHFS_LZMA
				zlib_err = zlib_inflateInit(&msblk->stream);
				if (zlib_err != Z_OK) {
					ERROR("zlib_inflateInit returned unexpected result 0x%x,"
						" srclength %d\n", zlib_err, srclength);
					goto release_mutex;
				}
#endif
				/*
				 * keep this block structture to simplify the
				 * diff.
				 */

				if (avail_bytes == 0) {
					offset = 0;
                                        SQUASHFS_BUF_FREE(bh[k]);
					continue;
				}
			}

#ifndef CONFIG_SQUASHFS_LZMA
			zlib_err = zlib_inflate(&msblk->stream, Z_NO_FLUSH);
			if (zlib_err != Z_OK && zlib_err != Z_STREAM_END) {
				ERROR("zlib_inflate returned unexpected result 0x%x,"
					" srclength %d, avail_in %d, avail_out %d\n", zlib_err,
					srclength, msblk->stream.avail_in, msblk->stream.avail_out);
				goto release_mutex;
			}
#endif
#ifdef CONFIG_SQUASHFS_MTD
			memcpy(p, bh[k] + offset, avail_bytes);
#else
			memcpy(p, bh[k]->b_data + offset, avail_bytes);

#endif
			p += avail_bytes;

			bytes += avail_bytes;
			offset = 0;
			SQUASHFS_BUF_FREE(bh[k]);
		}
#ifndef CONFIG_SQUASHFS_LZMA
		if (zlib_err != Z_STREAM_END)
			goto release_mutex;

		zlib_err = zlib_inflateEnd(&msblk->stream);
		if (zlib_err != Z_OK) {
			ERROR("zlib_inflateEnd returned unexpected result 0x%x,"
				" srclength %d\n", zlib_err, srclength);
                        goto release_mutex;
                }
#endif
                sbuf[Src].buf = percpu->read_data;
                sbuf[Src].sz = bytes;
                sbuf[Dst].buf = buffer;
                sbuf[Dst].sz = srclength;
                dpri_un(&percpu->un);
                // dpri("src %d %p, dst %d %p\n", sbuf[Src].sz, sbuf[Src].buf, sbuf[Dst].sz, sbuf[Dst].buf);
                zlib_err = sqlzma_un(&percpu->un, sbuf + Src, sbuf + Dst);
                bytes = percpu->un.un_reslen;

#ifdef KeepPreemptive
		mutex_unlock(&percpu->mtx);
#else
		put_cpu_var(sqlzma);
#endif
		if (unlikely(zlib_err)) {
			dpri("zlib_err %d\n", zlib_err);
 			goto release_mutex;
 		}
		up(&msblk->read_data_mutex);
	} else {
#ifndef CONFIG_SQUASHFS_MTD
		int i;
		for(i = 0; i < b; i++) {
			wait_on_buffer(bh[i]);
			if (!buffer_uptodate(bh[i]))
				goto block_release;
		}
#endif
		for (bytes = 0; k < b; k++) {
			avail_bytes = min(c_byte - bytes, msblk->devblksize - offset);

#ifdef CONFIG_SQUASHFS_MTD
			memcpy(buffer + bytes, bh[k] + offset, avail_bytes);
#else
			memcpy(buffer + bytes, bh[k]->b_data + offset, avail_bytes);
#endif
			bytes += avail_bytes;
			offset = 0;
                        SQUASHFS_BUF_FREE(bh[k]);
		}
	}

	if (next_index)
		*next_index = index + c_byte + (length ? 0 :
				(SQUASHFS_CHECK_DATA(msblk->sblk.flags) ? 3 : 2));

	kfree(bh);
	return bytes;

release_mutex:
	up(&msblk->read_data_mutex);

block_release:
	for (k=1; k < b; k++)
#ifdef CONFIG_SQUASHFS_MTD
            if (bh[k])
#endif
                SQUASHFS_BUF_FREE(bh[k]);
read_failure:
	ERROR("%s(): sb_bread failed reading block 0x%x\n", __func__, cur_index);
        if (bh)
            kfree(bh);
	return 0;
}


SQSH_EXTERN int squashfs_get_cached_block(struct super_block *s, void *buffer,
				long long block, unsigned int offset,
				int length, long long *next_block,
				unsigned int *next_offset)
{
	struct squashfs_sb_info *msblk = s->s_fs_info;
	int n, i, bytes, return_length = length;
	long long next_index;

	TRACE("Entered squashfs_get_cached_block [%llx:%x]\n", block, offset);

	while (1) {
		for (i = 0; i < squashfs_cached_blks; i++) 
			if (msblk->block_cache[i].block == block)
				break; 

		down(&msblk->block_cache_mutex);

		if (i == squashfs_cached_blks) {
			/* read inode header block */
			if (msblk->unused_cache_blks == 0) {
				up(&msblk->block_cache_mutex);
				wait_event(msblk->waitq, msblk->unused_cache_blks);
				continue;
			}

			i = msblk->next_cache;
			for (n = 0; n < squashfs_cached_blks; n++) {
				if (msblk->block_cache[i].block != SQUASHFS_USED_BLK)
					break;
				i = (i + 1) % squashfs_cached_blks;
			}

			msblk->next_cache = (i + 1) % squashfs_cached_blks;

			if (msblk->block_cache[i].block == SQUASHFS_INVALID_BLK) {
				msblk->block_cache[i].data = vmalloc(SQUASHFS_METADATA_SIZE);
				if (msblk->block_cache[i].data == NULL) {
					ERROR("Failed to allocate cache block\n");
					up(&msblk->block_cache_mutex);
					goto out;
				}
			}

			msblk->block_cache[i].block = SQUASHFS_USED_BLK;
			msblk->unused_cache_blks --;
			up(&msblk->block_cache_mutex);

			msblk->block_cache[i].length = squashfs_read_data(s,
				msblk->block_cache[i].data, block, 0, &next_index,
				SQUASHFS_METADATA_SIZE);

			if (msblk->block_cache[i].length == 0) {
				ERROR("Unable to read cache block [%llx:%x]\n", block, offset);
				down(&msblk->block_cache_mutex);
				msblk->block_cache[i].block = SQUASHFS_INVALID_BLK;
				msblk->unused_cache_blks ++;
				smp_mb();
				vfree(msblk->block_cache[i].data);
				wake_up(&msblk->waitq);
				up(&msblk->block_cache_mutex);
				goto out;
			}

			down(&msblk->block_cache_mutex);
			msblk->block_cache[i].block = block;
			msblk->block_cache[i].next_index = next_index;
			msblk->unused_cache_blks ++;
			smp_mb();
			wake_up(&msblk->waitq);
			TRACE("Read cache block [%llx:%x]\n", block, offset);
		}

		if (msblk->block_cache[i].block != block) {
			up(&msblk->block_cache_mutex);
			continue;
		}

		bytes = msblk->block_cache[i].length - offset;

		if (bytes < 1) {
			up(&msblk->block_cache_mutex);
			goto out;
		} else if (bytes >= length) {
			if (buffer)
				memcpy(buffer, msblk->block_cache[i].data + offset, length);
			if (msblk->block_cache[i].length - offset == length) {
				*next_block = msblk->block_cache[i].next_index;
				*next_offset = 0;
			} else {
				*next_block = block;
				*next_offset = offset + length;
			}
			up(&msblk->block_cache_mutex);
			goto finish;
		} else {
			if (buffer) {
				memcpy(buffer, msblk->block_cache[i].data + offset, bytes);
				buffer = (char *) buffer + bytes;
			}
			block = msblk->block_cache[i].next_index;
			up(&msblk->block_cache_mutex);
			length -= bytes;
			offset = 0;
		}
	}

finish:
	return return_length;
out:
	return 0;
}


static int get_fragment_location(struct super_block *s, unsigned int fragment,
				long long *fragment_start_block,
				unsigned int *fragment_size)
{
	struct squashfs_sb_info *msblk = s->s_fs_info;
	long long start_block =
		msblk->fragment_index[SQUASHFS_FRAGMENT_INDEX(fragment)];
	int offset = SQUASHFS_FRAGMENT_INDEX_OFFSET(fragment);
	struct squashfs_fragment_entry fragment_entry;

	if (msblk->swap) {
		struct squashfs_fragment_entry sfragment_entry;

		if (!squashfs_get_cached_block(s, &sfragment_entry, start_block, offset,
					 sizeof(sfragment_entry), &start_block, &offset))
			goto out;
		SQUASHFS_SWAP_FRAGMENT_ENTRY(&fragment_entry, &sfragment_entry);
	} else
		if (!squashfs_get_cached_block(s, &fragment_entry, start_block, offset,
					 sizeof(fragment_entry), &start_block, &offset))
			goto out;

	*fragment_start_block = fragment_entry.start_block;
	*fragment_size = fragment_entry.size;

	return 1;

out:
	return 0;
}


SQSH_EXTERN void release_cached_fragment(struct squashfs_sb_info *msblk,
				struct squashfs_fragment_cache *fragment)
{
	down(&msblk->fragment_mutex);
	fragment->locked --;
	if (fragment->locked == 0) {
		msblk->unused_frag_blks ++;
		smp_mb();
		wake_up(&msblk->fragment_wait_queue);
	}
	up(&msblk->fragment_mutex);
}


SQSH_EXTERN
struct squashfs_fragment_cache *get_cached_fragment(struct super_block *s,
				long long start_block, int length)
{
	int i, n;
	struct squashfs_sb_info *msblk = s->s_fs_info;
	struct squashfs_super_block *sblk = &msblk->sblk;

	while (1) {
		down(&msblk->fragment_mutex);

		for (i = 0; i < SQUASHFS_CACHED_FRAGMENTS &&
				msblk->fragment[i].block != start_block; i++);

		if (i == SQUASHFS_CACHED_FRAGMENTS) {
			if (msblk->unused_frag_blks == 0) {
				up(&msblk->fragment_mutex);
				wait_event(msblk->fragment_wait_queue, msblk->unused_frag_blks);
				continue;
			}

			i = msblk->next_fragment;
			for (n = 0; n < SQUASHFS_CACHED_FRAGMENTS; n++) {
				if (msblk->fragment[i].locked == 0)
					break;
				i = (i + 1) % SQUASHFS_CACHED_FRAGMENTS;
			}

			msblk->next_fragment = (msblk->next_fragment + 1) %
				SQUASHFS_CACHED_FRAGMENTS;

			if (msblk->fragment[i].data == NULL) {
				msblk->fragment[i].data = vmalloc(sblk->block_size);
				if (msblk->fragment[i].data == NULL) {
					ERROR("Failed to allocate fragment cache block\n");
					up(&msblk->fragment_mutex);
					goto out;
				}
			}

			msblk->unused_frag_blks --;
			msblk->fragment[i].block = SQUASHFS_INVALID_BLK;
			msblk->fragment[i].locked = 1;
			up(&msblk->fragment_mutex);

			msblk->fragment[i].length = squashfs_read_data(s,
				msblk->fragment[i].data, start_block, length, NULL,
				sblk->block_size);

			if (msblk->fragment[i].length == 0) {
				ERROR("Unable to read fragment cache block [%llx]\n", start_block);
				msblk->fragment[i].locked = 0;
				msblk->unused_frag_blks ++;
				smp_mb();
				wake_up(&msblk->fragment_wait_queue);
				goto out;
			}

			down(&msblk->fragment_mutex);
			msblk->fragment[i].block = start_block;
			TRACE("New fragment %d, start block %lld, locked %d\n",
				i, msblk->fragment[i].block, msblk->fragment[i].locked);
			up(&msblk->fragment_mutex);
			break;
		}

		if (msblk->fragment[i].locked == 0)
			msblk->unused_frag_blks --;
		msblk->fragment[i].locked++;
		up(&msblk->fragment_mutex);
		TRACE("Got fragment %d, start block %lld, locked %d\n", i,
			msblk->fragment[i].block, msblk->fragment[i].locked);
		break;
	}

	return &msblk->fragment[i];

out:
	return NULL;
}


static void squashfs_new_inode(struct squashfs_sb_info *msblk, struct inode *i,
				struct squashfs_base_inode_header *inodeb)
{
	i->i_ino = inodeb->inode_number;
	i->i_mtime.tv_sec = inodeb->mtime;
	i->i_atime.tv_sec = inodeb->mtime;
	i->i_ctime.tv_sec = inodeb->mtime;
	i->i_uid = msblk->uid[inodeb->uid];
	i->i_mode = inodeb->mode;
	i->i_size = 0;

	if (inodeb->guid == SQUASHFS_GUIDS)
		i->i_gid = i->i_uid;
	else
		i->i_gid = msblk->guid[inodeb->guid];
}


static squashfs_inode_t squashfs_inode_lookup(struct super_block *s, int ino)
{
	struct squashfs_sb_info *msblk = s->s_fs_info;
	long long start = msblk->inode_lookup_table[SQUASHFS_LOOKUP_BLOCK(ino - 1)];
	int offset = SQUASHFS_LOOKUP_BLOCK_OFFSET(ino - 1);
	squashfs_inode_t inode;

	TRACE("Entered squashfs_inode_lookup, inode_number = %d\n", ino);

	if (msblk->swap) {
		squashfs_inode_t sinode;

		if (!squashfs_get_cached_block(s, &sinode, start, offset,
					sizeof(sinode), &start, &offset))
			goto out;
		SQUASHFS_SWAP_INODE_T((&inode), &sinode);
	} else if (!squashfs_get_cached_block(s, &inode, start, offset,
					sizeof(inode), &start, &offset))
			goto out;

	TRACE("squashfs_inode_lookup, inode = 0x%llx\n", inode);

	return inode;

out:
	return SQUASHFS_INVALID_BLK;
}


static void vfs_read_inode(struct inode *i)
{
	struct squashfs_sb_info *msblk = i->i_sb->s_fs_info;
	squashfs_inode_t inode = squashfs_inode_lookup(i->i_sb, i->i_ino);

	TRACE("Entered vfs_read_inode\n");

	if(inode != SQUASHFS_INVALID_BLK)
		(msblk->read_inode)(i, inode);
}


static struct dentry *squashfs_get_parent(struct dentry *child)
{
	struct inode *i = child->d_inode;
	struct inode *parent = iget(i->i_sb, SQUASHFS_I(i)->u.s2.parent_inode);
	struct dentry *rv;

	TRACE("Entered squashfs_get_parent\n");

	if(parent == NULL) {
		rv = ERR_PTR(-EACCES);
		goto out;
	}

	rv = d_alloc_anon(parent);
	if(rv == NULL)
		rv = ERR_PTR(-ENOMEM);

out:
	return rv;
}


SQSH_EXTERN struct inode *squashfs_iget(struct super_block *s,
				squashfs_inode_t inode, unsigned int inode_number)
{
	struct squashfs_sb_info *msblk = s->s_fs_info;
	struct inode *i = iget_locked(s, inode_number);

	TRACE("Entered squashfs_iget\n");

	if(i && (i->i_state & I_NEW)) {
		(msblk->read_inode)(i, inode);
		unlock_new_inode(i);
	}

	return i;
}


static int squashfs_read_inode(struct inode *i, squashfs_inode_t inode)
{
	struct super_block *s = i->i_sb;
	struct squashfs_sb_info *msblk = s->s_fs_info;
	struct squashfs_super_block *sblk = &msblk->sblk;
	long long block = SQUASHFS_INODE_BLK(inode) + sblk->inode_table_start;
	unsigned int offset = SQUASHFS_INODE_OFFSET(inode);
	long long next_block;
	unsigned int next_offset;
	union squashfs_inode_header id, sid;
	struct squashfs_base_inode_header *inodeb = &id.base, *sinodeb = &sid.base;

	TRACE("Entered squashfs_read_inode\n");

	if (msblk->swap) {
		if (!squashfs_get_cached_block(s, sinodeb, block, offset,
					sizeof(*sinodeb), &next_block, &next_offset))
			goto failed_read;
		SQUASHFS_SWAP_BASE_INODE_HEADER(inodeb, sinodeb, sizeof(*sinodeb));
	} else
		if (!squashfs_get_cached_block(s, inodeb, block, offset,
					sizeof(*inodeb), &next_block, &next_offset))
			goto failed_read;

	squashfs_new_inode(msblk, i, inodeb);

	switch(inodeb->inode_type) {
		case SQUASHFS_FILE_TYPE: {
			unsigned int frag_size;
			long long frag_blk;
			struct squashfs_reg_inode_header *inodep = &id.reg;
			struct squashfs_reg_inode_header *sinodep = &sid.reg;

			if (msblk->swap) {
				if (!squashfs_get_cached_block(s, sinodep, block, offset,
						sizeof(*sinodep), &next_block, &next_offset))
					goto failed_read;
				SQUASHFS_SWAP_REG_INODE_HEADER(inodep, sinodep);
			} else
				if (!squashfs_get_cached_block(s, inodep, block, offset,
						sizeof(*inodep), &next_block, &next_offset))
					goto failed_read;

			frag_blk = SQUASHFS_INVALID_BLK;

			if (inodep->fragment != SQUASHFS_INVALID_FRAG)
					if(!get_fragment_location(s, inodep->fragment, &frag_blk,
												&frag_size))
						goto failed_read;

			i->i_nlink = 1;
			i->i_size = inodep->file_size;
			i->i_fop = &generic_ro_fops;
			i->i_mode |= S_IFREG;
			i->i_blocks = ((i->i_size - 1) >> 9) + 1;
			i->i_blksize = PAGE_CACHE_SIZE;
			SQUASHFS_I(i)->u.s1.fragment_start_block = frag_blk;
			SQUASHFS_I(i)->u.s1.fragment_size = frag_size;
			SQUASHFS_I(i)->u.s1.fragment_offset = inodep->offset;
			SQUASHFS_I(i)->start_block = inodep->start_block;
			SQUASHFS_I(i)->u.s1.block_list_start = next_block;
			SQUASHFS_I(i)->offset = next_offset;
			i->i_data.a_ops = &squashfs_aops;

			TRACE("File inode %x:%x, start_block %llx, "
					"block_list_start %llx, offset %x "
                                        "frag_start_block %x "
                                        "frag_size %x\n",
					SQUASHFS_INODE_BLK(inode), offset,
					inodep->start_block, next_block,
					next_offset, frag_blk, frag_size);
			break;
		}
		case SQUASHFS_LREG_TYPE: {
			unsigned int frag_size;
			long long frag_blk;
			struct squashfs_lreg_inode_header *inodep = &id.lreg;
			struct squashfs_lreg_inode_header *sinodep = &sid.lreg;

			if (msblk->swap) {
				if (!squashfs_get_cached_block(s, sinodep, block, offset,
						sizeof(*sinodep), &next_block, &next_offset))
					goto failed_read;
				SQUASHFS_SWAP_LREG_INODE_HEADER(inodep, sinodep);
			} else
				if (!squashfs_get_cached_block(s, inodep, block, offset,
						sizeof(*inodep), &next_block, &next_offset))
					goto failed_read;

			frag_blk = SQUASHFS_INVALID_BLK;

			if (inodep->fragment != SQUASHFS_INVALID_FRAG)
				if (!get_fragment_location(s, inodep->fragment, &frag_blk,
												 &frag_size))
					goto failed_read;

			i->i_nlink = inodep->nlink;
			i->i_size = inodep->file_size;
			i->i_fop = &generic_ro_fops;
			i->i_mode |= S_IFREG;
			i->i_blocks = ((i->i_size - 1) >> 9) + 1;
			i->i_blksize = PAGE_CACHE_SIZE;
			SQUASHFS_I(i)->u.s1.fragment_start_block = frag_blk;
			SQUASHFS_I(i)->u.s1.fragment_size = frag_size;
			SQUASHFS_I(i)->u.s1.fragment_offset = inodep->offset;
			SQUASHFS_I(i)->start_block = inodep->start_block;
			SQUASHFS_I(i)->u.s1.block_list_start = next_block;
			SQUASHFS_I(i)->offset = next_offset;
			i->i_data.a_ops = &squashfs_aops;

			TRACE("File inode %x:%x, start_block %llx, "
					"block_list_start %llx, offset %x\n",
					SQUASHFS_INODE_BLK(inode), offset,
					inodep->start_block, next_block,
					next_offset);
			break;
		}
		case SQUASHFS_DIR_TYPE: {
			struct squashfs_dir_inode_header *inodep = &id.dir;
			struct squashfs_dir_inode_header *sinodep = &sid.dir;

			if (msblk->swap) {
				if (!squashfs_get_cached_block(s, sinodep, block, offset,
						sizeof(*sinodep), &next_block, &next_offset))
					goto failed_read;
				SQUASHFS_SWAP_DIR_INODE_HEADER(inodep, sinodep);
			} else
				if (!squashfs_get_cached_block(s, inodep, block, offset,
						sizeof(*inodep), &next_block, &next_offset))
					goto failed_read;

			i->i_nlink = inodep->nlink;
			i->i_size = inodep->file_size;
			i->i_op = &squashfs_dir_inode_ops;
			i->i_fop = &squashfs_dir_ops;
			i->i_mode |= S_IFDIR;
			SQUASHFS_I(i)->start_block = inodep->start_block;
			SQUASHFS_I(i)->offset = inodep->offset;
			SQUASHFS_I(i)->u.s2.directory_index_count = 0;
			SQUASHFS_I(i)->u.s2.parent_inode = inodep->parent_inode;

			TRACE("Directory inode %x:%x, start_block %x, offset "
					"%x\n", SQUASHFS_INODE_BLK(inode),
					offset, inodep->start_block,
					inodep->offset);
			break;
		}
		case SQUASHFS_LDIR_TYPE: {
			struct squashfs_ldir_inode_header *inodep = &id.ldir;
			struct squashfs_ldir_inode_header *sinodep = &sid.ldir;

			if (msblk->swap) {
				if (!squashfs_get_cached_block(s, sinodep, block, offset,
						sizeof(*sinodep), &next_block, &next_offset))
					goto failed_read;
				SQUASHFS_SWAP_LDIR_INODE_HEADER(inodep, sinodep);
			} else
				if (!squashfs_get_cached_block(s, inodep, block, offset,
						sizeof(*inodep), &next_block, &next_offset))
					goto failed_read;

			i->i_nlink = inodep->nlink;
			i->i_size = inodep->file_size;
			i->i_op = &squashfs_dir_inode_ops;
			i->i_fop = &squashfs_dir_ops;
			i->i_mode |= S_IFDIR;
			SQUASHFS_I(i)->start_block = inodep->start_block;
			SQUASHFS_I(i)->offset = inodep->offset;
			SQUASHFS_I(i)->u.s2.directory_index_start = next_block;
			SQUASHFS_I(i)->u.s2.directory_index_offset = next_offset;
			SQUASHFS_I(i)->u.s2.directory_index_count = inodep->i_count;
			SQUASHFS_I(i)->u.s2.parent_inode = inodep->parent_inode;

			TRACE("Long directory inode %x:%x, start_block %x, offset %x\n",
					SQUASHFS_INODE_BLK(inode), offset,
					inodep->start_block, inodep->offset);
			break;
		}
		case SQUASHFS_SYMLINK_TYPE: {
			struct squashfs_symlink_inode_header *inodep = &id.symlink;
			struct squashfs_symlink_inode_header *sinodep = &sid.symlink;

			if (msblk->swap) {
				if (!squashfs_get_cached_block(s, sinodep, block, offset,
						sizeof(*sinodep), &next_block, &next_offset))
					goto failed_read;
				SQUASHFS_SWAP_SYMLINK_INODE_HEADER(inodep, sinodep);
			} else
				if (!squashfs_get_cached_block(s, inodep, block, offset,
						sizeof(*inodep), &next_block, &next_offset))
					goto failed_read;

			i->i_nlink = inodep->nlink;
			i->i_size = inodep->symlink_size;
			i->i_op = &page_symlink_inode_operations;
			i->i_data.a_ops = &squashfs_symlink_aops;
			i->i_mode |= S_IFLNK;
			SQUASHFS_I(i)->start_block = next_block;
			SQUASHFS_I(i)->offset = next_offset;

			TRACE("Symbolic link inode %x:%x, start_block %llx, offset %x\n",
					SQUASHFS_INODE_BLK(inode), offset,
					next_block, next_offset);
			break;
		 }
		 case SQUASHFS_BLKDEV_TYPE:
		 case SQUASHFS_CHRDEV_TYPE: {
			struct squashfs_dev_inode_header *inodep = &id.dev;
			struct squashfs_dev_inode_header *sinodep = &sid.dev;

			if (msblk->swap) {
				if (!squashfs_get_cached_block(s, sinodep, block, offset,
						sizeof(*sinodep), &next_block, &next_offset))
					goto failed_read;
				SQUASHFS_SWAP_DEV_INODE_HEADER(inodep, sinodep);
			} else	
				if (!squashfs_get_cached_block(s, inodep, block, offset,
						sizeof(*inodep), &next_block, &next_offset))
					goto failed_read;

			i->i_nlink = inodep->nlink;
			i->i_mode |= (inodeb->inode_type == SQUASHFS_CHRDEV_TYPE) ?
					S_IFCHR : S_IFBLK;
			init_special_inode(i, i->i_mode, old_decode_dev(inodep->rdev));

			TRACE("Device inode %x:%x, rdev %x\n",
					SQUASHFS_INODE_BLK(inode), offset, inodep->rdev);
			break;
		 }
		 case SQUASHFS_FIFO_TYPE:
		 case SQUASHFS_SOCKET_TYPE: {
			struct squashfs_ipc_inode_header *inodep = &id.ipc;
			struct squashfs_ipc_inode_header *sinodep = &sid.ipc;

			if (msblk->swap) {
				if (!squashfs_get_cached_block(s, sinodep, block, offset,
						sizeof(*sinodep), &next_block, &next_offset))
					goto failed_read;
				SQUASHFS_SWAP_IPC_INODE_HEADER(inodep, sinodep);
			} else
				if (!squashfs_get_cached_block(s, inodep, block, offset,
						sizeof(*inodep), &next_block, &next_offset))
					goto failed_read;

			i->i_nlink = inodep->nlink;
			i->i_mode |= (inodeb->inode_type == SQUASHFS_FIFO_TYPE)
							? S_IFIFO : S_IFSOCK;
			init_special_inode(i, i->i_mode, 0);
			break;
		 }
		 default:
			ERROR("Unknown inode type %d in squashfs_iget!\n",
					inodeb->inode_type);
			goto failed_read1;
	}

	return 1;

failed_read:
	ERROR("Unable to read inode [%llx:%x]\n", block, offset);

failed_read1:
	make_bad_inode(i);
	return 0;
}


static int read_inode_lookup_table(struct super_block *s)
{
	struct squashfs_sb_info *msblk = s->s_fs_info;
	struct squashfs_super_block *sblk = &msblk->sblk;
	unsigned int length = SQUASHFS_LOOKUP_BLOCK_BYTES(sblk->inodes);

	TRACE("In read_inode_lookup_table, length %d\n", length);

	/* Allocate inode lookup table */
	msblk->inode_lookup_table = kmalloc(length, GFP_KERNEL);
	if (msblk->inode_lookup_table == NULL) {
		ERROR("Failed to allocate inode lookup table\n");
		return 0;
	}

	if (!squashfs_read_data(s, (char *) msblk->inode_lookup_table,
			sblk->lookup_table_start, length |
			SQUASHFS_COMPRESSED_BIT_BLOCK, NULL, length)) {
		ERROR("unable to read inode lookup table\n");
		return 0;
	}

	if (msblk->swap) {
		int i;
		long long block;

		for (i = 0; i < SQUASHFS_LOOKUP_BLOCKS(sblk->inodes); i++) {
			/* XXX */
			SQUASHFS_SWAP_LOOKUP_BLOCKS((&block),
						&msblk->inode_lookup_table[i], 1);
			msblk->inode_lookup_table[i] = block;
		}
	}

	return 1;
}


static int read_fragment_index_table(struct super_block *s)
{
	struct squashfs_sb_info *msblk = s->s_fs_info;
	struct squashfs_super_block *sblk = &msblk->sblk;
	unsigned int length = SQUASHFS_FRAGMENT_INDEX_BYTES(sblk->fragments);

	if(length == 0)
		return 1;

	/* Allocate fragment index table */
	msblk->fragment_index = kmalloc(length, GFP_KERNEL);
	if (msblk->fragment_index == NULL) {
		ERROR("Failed to allocate fragment index table\n");
		return 0;
	}

	if (!squashfs_read_data(s, (char *) msblk->fragment_index,
			sblk->fragment_table_start, length |
			SQUASHFS_COMPRESSED_BIT_BLOCK, NULL, length)) {
		ERROR("unable to read fragment index table\n");
		return 0;
	}

	if (msblk->swap) {
		int i;
		long long fragment;

		for (i = 0; i < SQUASHFS_FRAGMENT_INDEXES(sblk->fragments); i++) {
			/* XXX */
			SQUASHFS_SWAP_FRAGMENT_INDEXES((&fragment),
						&msblk->fragment_index[i], 1);
			msblk->fragment_index[i] = fragment;
		}
	}

	return 1;
}


static int readahead_metadata(struct super_block *s)
{
	struct squashfs_sb_info *msblk = s->s_fs_info;
	int i;

	squashfs_cached_blks = SQUASHFS_CACHED_BLKS;

	/* Init inode_table block pointer array */
	msblk->block_cache = kmalloc(sizeof(struct squashfs_cache) *
					squashfs_cached_blks, GFP_KERNEL);
	if (msblk->block_cache == NULL) {
		ERROR("Failed to allocate block cache\n");
		goto failed;
	}

	for (i = 0; i < squashfs_cached_blks; i++)
		msblk->block_cache[i].block = SQUASHFS_INVALID_BLK;

	msblk->next_cache = 0;
	msblk->unused_cache_blks = squashfs_cached_blks;

	return 1;

failed:
	return 0;
}


static int supported_squashfs_filesystem(struct squashfs_sb_info *msblk, int silent)
{
	struct squashfs_super_block *sblk = &msblk->sblk;

	msblk->read_inode = squashfs_read_inode;
	msblk->read_blocklist = read_blocklist;
	msblk->read_fragment_index_table = read_fragment_index_table;

	if (sblk->s_major == 1) {
		if (!squashfs_1_0_supported(msblk)) {
			SERROR("Major/Minor mismatch, Squashfs 1.0 filesystems "
				"are unsupported\n");
			SERROR("Please recompile with Squashfs 1.0 support enabled\n");
			return 0;
		}
	} else if (sblk->s_major == 2) {
		if (!squashfs_2_0_supported(msblk)) {
			SERROR("Major/Minor mismatch, Squashfs 2.0 filesystems "
				"are unsupported\n");
			SERROR("Please recompile with Squashfs 2.0 support enabled\n");
			return 0;
		}
	} else if(sblk->s_major != SQUASHFS_MAJOR || sblk->s_minor >
			SQUASHFS_MINOR) {
		SERROR("Major/Minor mismatch, trying to mount newer %d.%d "
				"filesystem\n", sblk->s_major, sblk->s_minor);
		SERROR("Please update your kernel\n");
		return 0;
	}

	return 1;
}


static int squashfs_fill_super(struct super_block *s, void *data, int silent)
{
	struct squashfs_sb_info *msblk;
	struct squashfs_super_block *sblk;
	int i;
	struct inode *root;
        int err;
#ifndef CONFIG_SQUASHFS_MTD
	char b[BDEVNAME_SIZE];
#endif
	err = -ENOMEM;

	TRACE("Entered squashfs_fill_superblock\n");
#ifndef CONFIG_SQUASHFS_MTD
	s->s_fs_info = kzalloc(sizeof(struct squashfs_sb_info), GFP_KERNEL);
	if (s->s_fs_info == NULL) {
		ERROR("Failed to allocate superblock\n");
		goto failure;
	}
#endif
	msblk = s->s_fs_info;
#ifndef CONFIG_SQUASHFS_LZMA
	msblk->stream.workspace = vmalloc(zlib_inflate_workspacesize());
	if (msblk->stream.workspace == NULL) {
		ERROR("Failed to allocate zlib workspace\n");
		goto failure;
	}
#endif
	sblk = &msblk->sblk;
#ifdef CONFIG_SQUASHFS_MTD
	msblk->devblksize = 1024;
        s->s_blocksize = msblk->devblksize;
        s->s_blocksize_bits = 10;
#else
	msblk->devblksize = sb_min_blocksize(s, BLOCK_SIZE);
#endif
	msblk->devblksize_log2 = ffz(~msblk->devblksize);

	init_MUTEX(&msblk->read_data_mutex);
	init_MUTEX(&msblk->read_page_mutex);
	init_MUTEX(&msblk->block_cache_mutex);
	init_MUTEX(&msblk->fragment_mutex);
	init_MUTEX(&msblk->meta_index_mutex);

	init_waitqueue_head(&msblk->waitq);
	init_waitqueue_head(&msblk->fragment_wait_queue);

	/* sblk->bytes_used is checked in squashfs_read_data to ensure reads are not
 	 * beyond filesystem end.  As we're using squashfs_read_data to read sblk here,
 	 * first set sblk->bytes_used to a useful value */
	err = -EINVAL;
	sblk->bytes_used = sizeof(struct squashfs_super_block);
	if (!squashfs_read_data(s, (char *) sblk, SQUASHFS_START,
					sizeof(struct squashfs_super_block) |
					SQUASHFS_COMPRESSED_BIT_BLOCK, NULL, sizeof(struct squashfs_super_block))) {
		printk("%s(): unable to read superblock\n", __func__);
		goto failed_mount;
	}

	/* Check it is a SQUASHFS superblock */
	s->s_magic = sblk->s_magic;
	msblk->swap = 0;
	dpri("magic 0x%x\n", sblk->s_magic);
	switch (sblk->s_magic) {
		struct squashfs_super_block ssblk;

	case SQUASHFS_MAGIC_SWAP:
		/*FALLTHROUGH*/
	case SQUASHFS_MAGIC_LZMA_SWAP:
		WARNING("Mounting a different endian SQUASHFS filesystem on %s\n", 
#ifdef CONFIG_SQUASHFS_MTD
                        msblk->mtd->name);
#else
                        bdevname(s->s_bdev, b));
#endif

		SQUASHFS_SWAP_SUPER_BLOCK(&ssblk, sblk);

                //printk("squashfs: %s(%s) after the SQUASHFS_SWAP_SUPER_BLOCK macro \n", __FILE__,  __func__);
		memcpy(sblk, &ssblk, sizeof(struct squashfs_super_block));
                //printk("squashfs: %s(%s) after the memcpy \n", __FILE__, __func__);
		msblk->swap = 1;
		/*FALLTHROUGH*/
	case SQUASHFS_MAGIC:
	case SQUASHFS_MAGIC_LZMA:
		break;
	default:
		printk("%s(): Can't find a SQUASHFS superblock on %s\n", __func__,
#ifdef CONFIG_SQUASHFS_MTD
                        msblk->mtd->name);
#else
                        bdevname(s->s_bdev, b));
#endif
		goto failed_mount;
	}

	{
		struct sqlzma *p;
		dpri("block_size %d, devblksize %d\n",
		     sblk->block_size, msblk->devblksize);
		BUG_ON(sblk->block_size > sizeof(p->read_data));
	}

	/* Check the MAJOR & MINOR versions */
	if(!supported_squashfs_filesystem(msblk, silent))
		goto failed_mount;

	/* Check the filesystem does not extend beyond the end of the
	   block device */
#ifdef CONFIG_SQUASHFS_MTD
	if(sblk->bytes_used < 0  || sblk->bytes_used > msblk->mtd->size)
#else
	if(sblk->bytes_used < 0 || sblk->bytes_used > i_size_read(s->s_bdev->bd_inode))
#endif
		goto failed_mount;

	/* Check the root inode for sanity */
	if (SQUASHFS_INODE_OFFSET(sblk->root_inode) > SQUASHFS_METADATA_SIZE)
		goto failed_mount;

	TRACE("Found valid superblock on %s\n", 
#ifdef CONFIG_SQUASHFS_MTD
                        msblk->mtd->name);
#else
                        bdevname(s->s_bdev, b));
#endif
	TRACE("Inodes are %scompressed\n", SQUASHFS_UNCOMPRESSED_INODES(sblk->flags)
					? "un" : "");
	TRACE("Data is %scompressed\n", SQUASHFS_UNCOMPRESSED_DATA(sblk->flags)
					? "un" : "");
	TRACE("Check data is %spresent in the filesystem\n",
					SQUASHFS_CHECK_DATA(sblk->flags) ?  "" : "not ");
	TRACE("Filesystem size %lld bytes\n", sblk->bytes_used);
	TRACE("Block size %d\n", sblk->block_size);
	TRACE("Number of inodes %d\n", sblk->inodes);
	if (sblk->s_major > 1)
		TRACE("Number of fragments %d\n", sblk->fragments);
	TRACE("Number of uids %d\n", sblk->no_uids);
	TRACE("Number of gids %d\n", sblk->no_guids);
	TRACE("sblk->inode_table_start %llx\n", sblk->inode_table_start);
	TRACE("sblk->directory_table_start %llx\n", sblk->directory_table_start);
	if (sblk->s_major > 1)
		TRACE("sblk->fragment_table_start %llx\n", sblk->fragment_table_start);
	TRACE("sblk->uid_start %llx\n", sblk->uid_start);

	s->s_maxbytes = MAX_LFS_FILESIZE;
	s->s_flags |= MS_RDONLY;
	s->s_op = &squashfs_super_ops;

	if (readahead_metadata(s) == 0)
		goto failed_mount;

	/* Allocate read_page block */
	err = -ENOMEM;
	msblk->read_page = vmalloc(sblk->block_size);
	if (msblk->read_page == NULL) {
		ERROR("Failed to allocate read_page block\n");
		goto failed_mount;
	}

	/* Allocate uid and gid tables */
	msblk->uid = kmalloc((sblk->no_uids + sblk->no_guids) *
					sizeof(unsigned int), GFP_KERNEL);
	if (msblk->uid == NULL) {
		ERROR("Failed to allocate uid/gid table\n");
		goto failed_mount;
	}
	msblk->guid = msblk->uid + sblk->no_uids;

	dpri("swap %d\n", msblk->swap);
	err = -EINVAL;
	if (msblk->swap) {
		unsigned int *suid;

		err = -ENOMEM;
		suid = kmalloc(sizeof(*suid) * (sblk->no_uids + sblk->no_guids),
			       GFP_KERNEL);
		if (unlikely(!suid))
			goto failed_mount;

		err = -EINVAL;
		if (!squashfs_read_data(s, (char *) &suid, sblk->uid_start,
					((sblk->no_uids + sblk->no_guids) *
					 sizeof(unsigned int)) |
					SQUASHFS_COMPRESSED_BIT_BLOCK, NULL, (sblk->no_uids + sblk->no_guids) * sizeof(unsigned int))) {
			ERROR("unable to read uid/gid table\n");
			kfree(suid);
			goto failed_mount;
		}
                TRACE("%s(%s) after squashfs_read_data \n", __FILE__, __func__);
		SQUASHFS_SWAP_DATA(msblk->uid, suid, (sblk->no_uids +
			sblk->no_guids), (sizeof(unsigned int) * 8));
                // printk("squashfs: %s(%s) after the SQUASHFS_SWAP_DATA macro \n", __FILE__,  __func__);
		kfree(suid);
	} else
		if (!squashfs_read_data(s, (char *) msblk->uid, sblk->uid_start,
					((sblk->no_uids + sblk->no_guids) *
					 sizeof(unsigned int)) |
					SQUASHFS_COMPRESSED_BIT_BLOCK, NULL, (sblk->no_uids + sblk->no_guids) * sizeof(unsigned int))) {
			ERROR("unable to read uid/gid table\n");
			goto failed_mount;
		}


	if (sblk->s_major == 1 && squashfs_1_0_supported(msblk))
		goto allocate_root;

	err = -ENOMEM;
	msblk->fragment = kzalloc(sizeof(struct squashfs_fragment_cache) *
				SQUASHFS_CACHED_FRAGMENTS, GFP_KERNEL);
	if (msblk->fragment == NULL) {
		ERROR("Failed to allocate fragment block cache\n");
		goto failed_mount;
	}

        TRACE("%s(%s) after msblk->fragment kzalloc \n",  __FILE__, __func__);

	for (i = 0; i < SQUASHFS_CACHED_FRAGMENTS; i++) {
		msblk->fragment[i].block = SQUASHFS_INVALID_BLK;
	}

	msblk->next_fragment = 0;
	msblk->unused_frag_blks = SQUASHFS_CACHED_FRAGMENTS;

	/* Allocate and read fragment index table */
	if (msblk->read_fragment_index_table(s) == 0)
		goto failed_mount;

	if(sblk->s_major < 3 || sblk->lookup_table_start == SQUASHFS_INVALID_BLK)
		goto allocate_root;

	/* Allocate and read inode lookup table */
	if (read_inode_lookup_table(s) == 0)
		goto failed_mount;

	s->s_op = &squashfs_export_super_ops;
	s->s_export_op = &squashfs_export_ops;

allocate_root:
	dpri("alloate_root\n");
	root = new_inode(s);
	if ((msblk->read_inode)(root, sblk->root_inode) == 0) {
		iput(root);
		goto failed_mount;
	}
	insert_inode_hash(root);

	s->s_root = d_alloc_root(root);
	if (s->s_root == NULL) {
		ERROR("Root inode create failed\n");
		iput(root);
		goto failed_mount;
	}

	TRACE("Leaving squashfs_fill_super\n");
	return 0;

failed_mount:
        printk("%s(): failed mount err = %d\n", __func__, err);
	kfree(msblk->inode_lookup_table);
	kfree(msblk->fragment_index);
	kfree(msblk->fragment);
	kfree(msblk->uid);
	vfree(msblk->read_page);
	kfree(msblk->block_cache);
	kfree(msblk->fragment_index_2);
	kfree(s->s_fs_info);
	s->s_fs_info = NULL;
failure:
	return err;
}


static int squashfs_statfs(struct super_block *s, struct kstatfs *buf)
{
	struct squashfs_sb_info *msblk = s->s_fs_info;
	struct squashfs_super_block *sblk = &msblk->sblk;

	TRACE("Entered squashfs_statfs\n");

	buf->f_type = sblk->s_magic;
	buf->f_bsize = sblk->block_size;
	buf->f_blocks = ((sblk->bytes_used - 1) >> sblk->block_log) + 1;
	buf->f_bfree = buf->f_bavail = 0;
	buf->f_files = sblk->inodes;
	buf->f_ffree = 0;
	buf->f_namelen = SQUASHFS_NAME_LEN;

	return 0;
}


static int squashfs_symlink_readpage(struct file *file, struct page *page)
{
	struct inode *inode = page->mapping->host;
	int index = page->index << PAGE_CACHE_SHIFT, length, bytes, avail_bytes;
	long long block = SQUASHFS_I(inode)->start_block;
	int offset = SQUASHFS_I(inode)->offset;
	void *pageaddr = kmap(page);

	TRACE("Entered squashfs_symlink_readpage, page index %ld, start block "
				"%llx, offset %x\n", page->index,
				SQUASHFS_I(inode)->start_block,
				SQUASHFS_I(inode)->offset);

	for (length = 0; length < index; length += bytes) {
		bytes = squashfs_get_cached_block(inode->i_sb, NULL, block,
				offset, PAGE_CACHE_SIZE, &block, &offset);
		if (bytes == 0) {
			ERROR("Unable to read symbolic link [%llx:%x]\n", block, offset);
			goto skip_read;
		}
	}

	if (length != index) {
		ERROR("(squashfs_symlink_readpage) length != index\n");
		bytes = 0;
		goto skip_read;
	}

	avail_bytes = min_t(int, i_size_read(inode) - length, PAGE_CACHE_SIZE);

	bytes = squashfs_get_cached_block(inode->i_sb, pageaddr, block, offset,
		avail_bytes, &block, &offset);
	if (bytes == 0)
		ERROR("Unable to read symbolic link [%llx:%x]\n", block, offset);

skip_read:
	memset(pageaddr + bytes, 0, PAGE_CACHE_SIZE - bytes);
	kunmap(page);
	flush_dcache_page(page);
	SetPageUptodate(page);
	unlock_page(page);

	return 0;
}


struct meta_index *locate_meta_index(struct inode *inode, int index, int offset)
{
	struct meta_index *meta = NULL;
	struct squashfs_sb_info *msblk = inode->i_sb->s_fs_info;
	int i;

	down(&msblk->meta_index_mutex);

	TRACE("locate_meta_index: index %d, offset %d\n", index, offset);

	if (msblk->meta_index == NULL)
		goto not_allocated;

	for (i = 0; i < SQUASHFS_META_NUMBER; i ++) {
		if (msblk->meta_index[i].inode_number == inode->i_ino &&
				msblk->meta_index[i].offset >= offset &&
				msblk->meta_index[i].offset <= index &&
				msblk->meta_index[i].locked == 0) {
			TRACE("locate_meta_index: entry %d, offset %d\n", i,
					msblk->meta_index[i].offset);
			meta = &msblk->meta_index[i];
			offset = meta->offset;
		}
	}

	if (meta)
		meta->locked = 1;

not_allocated:
	up(&msblk->meta_index_mutex);

	return meta;
}


struct meta_index *empty_meta_index(struct inode *inode, int offset, int skip)
{
	struct squashfs_sb_info *msblk = inode->i_sb->s_fs_info;
	struct meta_index *meta = NULL;
	int i;

	down(&msblk->meta_index_mutex);

	TRACE("empty_meta_index: offset %d, skip %d\n", offset, skip);

	if (msblk->meta_index == NULL) {
		msblk->meta_index = kmalloc(sizeof(struct meta_index) *
					SQUASHFS_META_NUMBER, GFP_KERNEL);
		if (msblk->meta_index == NULL) {
			ERROR("Failed to allocate meta_index\n");
			goto failed;
		}
		for (i = 0; i < SQUASHFS_META_NUMBER; i++) {
			msblk->meta_index[i].inode_number = 0;
			msblk->meta_index[i].locked = 0;
		}
		msblk->next_meta_index = 0;
	}

	for (i = SQUASHFS_META_NUMBER; i &&
			msblk->meta_index[msblk->next_meta_index].locked; i --)
		msblk->next_meta_index = (msblk->next_meta_index + 1) %
			SQUASHFS_META_NUMBER;

	if (i == 0) {
		TRACE("empty_meta_index: failed!\n");
		goto failed;
	}

	TRACE("empty_meta_index: returned meta entry %d, %p\n",
			msblk->next_meta_index,
			&msblk->meta_index[msblk->next_meta_index]);

	meta = &msblk->meta_index[msblk->next_meta_index];
	msblk->next_meta_index = (msblk->next_meta_index + 1) %
			SQUASHFS_META_NUMBER;

	meta->inode_number = inode->i_ino;
	meta->offset = offset;
	meta->skip = skip;
	meta->entries = 0;
	meta->locked = 1;

failed:
	up(&msblk->meta_index_mutex);
	return meta;
}


void release_meta_index(struct inode *inode, struct meta_index *meta)
{
	meta->locked = 0;
	smp_mb();
}


static int read_block_index(struct super_block *s, int blocks, char *block_list,
				long long *start_block, int *offset)
{
	struct squashfs_sb_info *msblk = s->s_fs_info;
	unsigned int *block_listp;
	int block = 0;

	if (msblk->swap) {
		char *sblock_list;

		sblock_list = kmalloc(blocks << 2, GFP_KERNEL);
		if (unlikely(!sblock_list))
			goto failure;

		if (!squashfs_get_cached_block(s, sblock_list, *start_block,
				*offset, blocks << 2, start_block, offset)) {
			ERROR("Fail reading block list [%llx:%x]\n", *start_block, *offset);
			kfree(sblock_list);
			goto failure;
		}
		SQUASHFS_SWAP_INTS(((unsigned int *)block_list),
				((unsigned int *)sblock_list), blocks);
		kfree(sblock_list);
	} else {
		if (!squashfs_get_cached_block(s, block_list, *start_block,
				*offset, blocks << 2, start_block, offset)) {
			ERROR("Fail reading block list [%llx:%x]\n", *start_block, *offset);
			goto failure;
		}
	}

	for (block_listp = (unsigned int *) block_list; blocks;
				block_listp++, blocks --)
		block += SQUASHFS_COMPRESSED_SIZE_BLOCK(*block_listp);

	return block;

failure:
	return -1;
}


#define SIZE 256

static inline int calculate_skip(int blocks) {
	int skip = (blocks - 1) / ((SQUASHFS_SLOTS * SQUASHFS_META_ENTRIES + 1) * SQUASHFS_META_INDEXES);
	return skip >= 7 ? 7 : skip + 1;
}


static int get_meta_index(struct inode *inode, int index,
		long long *index_block, int *index_offset,
		long long *data_block, char *block_list)
{
	struct squashfs_sb_info *msblk = inode->i_sb->s_fs_info;
	struct squashfs_super_block *sblk = &msblk->sblk;
	int skip = calculate_skip(i_size_read(inode) >> sblk->block_log);
	int offset = 0;
	struct meta_index *meta;
	struct meta_entry *meta_entry;
	long long cur_index_block = SQUASHFS_I(inode)->u.s1.block_list_start;
	int cur_offset = SQUASHFS_I(inode)->offset;
	long long cur_data_block = SQUASHFS_I(inode)->start_block;
	int i;

	index /= SQUASHFS_META_INDEXES * skip;

	while (offset < index) {
		meta = locate_meta_index(inode, index, offset + 1);

		if (meta == NULL) {
			meta = empty_meta_index(inode, offset + 1, skip);
			if (meta == NULL)
				goto all_done;
		} else {
			if(meta->entries == 0)
				goto failed;
			/* XXX */
			offset = index < meta->offset + meta->entries ? index :
				meta->offset + meta->entries - 1;
			/* XXX */
			meta_entry = &meta->meta_entry[offset - meta->offset];
			cur_index_block = meta_entry->index_block + sblk->inode_table_start;
			cur_offset = meta_entry->offset;
			cur_data_block = meta_entry->data_block;
			TRACE("get_meta_index: offset %d, meta->offset %d, "
				"meta->entries %d\n", offset, meta->offset, meta->entries);
			TRACE("get_meta_index: index_block 0x%llx, offset 0x%x"
				" data_block 0x%llx\n", cur_index_block,
				cur_offset, cur_data_block);
		}

		for (i = meta->offset + meta->entries; i <= index &&
				i < meta->offset + SQUASHFS_META_ENTRIES; i++) {
			int blocks = skip * SQUASHFS_META_INDEXES;

			while (blocks) {
				int block = blocks > (SIZE >> 2) ? (SIZE >> 2) : blocks;
				int res = read_block_index(inode->i_sb, block, block_list,
					&cur_index_block, &cur_offset);

				if (res == -1)
					goto failed;

				cur_data_block += res;
				blocks -= block;
			}

			meta_entry = &meta->meta_entry[i - meta->offset];
			meta_entry->index_block = cur_index_block - sblk->inode_table_start;
			meta_entry->offset = cur_offset;
			meta_entry->data_block = cur_data_block;
			meta->entries ++;
			offset ++;
		}

		TRACE("get_meta_index: meta->offset %d, meta->entries %d\n",
				meta->offset, meta->entries);

		release_meta_index(inode, meta);
	}

all_done:
	*index_block = cur_index_block;
	*index_offset = cur_offset;
	*data_block = cur_data_block;

	return offset * SQUASHFS_META_INDEXES * skip;

failed:
	release_meta_index(inode, meta);
	return -1;
}


static long long read_blocklist(struct inode *inode, int index,
				int readahead_blks, char *block_list,
				unsigned short **block_p, unsigned int *bsize)
{
	long long block_ptr;
	int offset;
	long long block;
	int res = get_meta_index(inode, index, &block_ptr, &offset, &block,
		block_list);

	TRACE("read_blocklist: res %d, index %d, block_ptr 0x%llx, offset"
		       " 0x%x, block 0x%llx\n", res, index, block_ptr, offset, block);

	if(res == -1)
		goto failure;

	index -= res;

	while (index) {
		int blocks = index > (SIZE >> 2) ? (SIZE >> 2) : index;
		int res = read_block_index(inode->i_sb, blocks, block_list,
			&block_ptr, &offset);
		if (res == -1)
			goto failure;
		block += res;
		index -= blocks;
	}

	if (read_block_index(inode->i_sb, 1, block_list, &block_ptr, &offset) == -1)
		goto failure;
	*bsize = *((unsigned int *) block_list);

	return block;

failure:
	return 0;
}


static int squashfs_readpage(struct file *file, struct page *page)
{
	struct inode *inode = page->mapping->host;
	struct squashfs_sb_info *msblk = inode->i_sb->s_fs_info;
	struct squashfs_super_block *sblk = &msblk->sblk;
	unsigned char *block_list = NULL;
	long long block;
	unsigned int bsize, i;
	int bytes;
	int index = page->index >> (sblk->block_log - PAGE_CACHE_SHIFT);
 	void *pageaddr;
	struct squashfs_fragment_cache *fragment = NULL;
	char *data_ptr = msblk->read_page;

	int mask = (1 << (sblk->block_log - PAGE_CACHE_SHIFT)) - 1;
	int start_index = page->index & ~mask;
	int end_index = start_index | mask;
	int file_end = i_size_read(inode) >> sblk->block_log;
	int sparse = 0;

	TRACE("Entered squashfs_readpage, page index %lx, start block %llx\n",
					page->index, SQUASHFS_I(inode)->start_block);

	if (page->index >= ((i_size_read(inode) + PAGE_CACHE_SIZE - 1) >>
					PAGE_CACHE_SHIFT))
		goto out;

	if (SQUASHFS_I(inode)->u.s1.fragment_start_block == SQUASHFS_INVALID_BLK
					|| index < file_end) {
		block_list = kmalloc(SIZE, GFP_KERNEL);
		if (block_list == NULL) {
			ERROR("Failed to allocate block_list\n");
			goto error_out;
		}

		block = (msblk->read_blocklist)(inode, index, 1, block_list, NULL, &bsize);
		if (block == 0)
			goto error_out;

		if (bsize == 0) { /* hole */
			bytes = index == file_end ?
				(i_size_read(inode) & (sblk->block_size - 1)) : sblk->block_size;
			sparse = 1;
		} else {
			down(&msblk->read_page_mutex);

			bytes = squashfs_read_data(inode->i_sb, msblk->read_page, block,
				bsize, NULL, sblk->block_size);

			if (bytes == 0) {
				ERROR("Unable to read page, block %llx, size %x\n", block, bsize);
				up(&msblk->read_page_mutex);
				goto error_out;
			}
		}
	} else {
		fragment = get_cached_fragment(inode->i_sb,
					SQUASHFS_I(inode)-> u.s1.fragment_start_block,
					SQUASHFS_I(inode)->u.s1.fragment_size);

		if (fragment == NULL) {
			ERROR("Unable to read page, block %llx, size %x\n",
					SQUASHFS_I(inode)->u.s1.fragment_start_block,
					(int) SQUASHFS_I(inode)->u.s1.fragment_size);
			goto error_out;
		}
		bytes = i_size_read(inode) & (sblk->block_size - 1);
		data_ptr = fragment->data + SQUASHFS_I(inode)->u.s1.fragment_offset;
	}

	for (i = start_index; i <= end_index && bytes > 0; i++,
						bytes -= PAGE_CACHE_SIZE, data_ptr += PAGE_CACHE_SIZE) {
		struct page *push_page;
		int avail = sparse ? 0 : min_t(unsigned int, bytes, PAGE_CACHE_SIZE);

		TRACE("bytes %d, i %d, available_bytes %d\n", bytes, i, avail);

		push_page = (i == page->index) ? page :
			grab_cache_page_nowait(page->mapping, i);

		if (!push_page)
			continue;

		if (PageUptodate(push_page))
			goto skip_page;

 		pageaddr = kmap_atomic(push_page, KM_USER0);
		memcpy(pageaddr, data_ptr, avail);
		memset(pageaddr + avail, 0, PAGE_CACHE_SIZE - avail);
		kunmap_atomic(pageaddr, KM_USER0);
		flush_dcache_page(push_page);
		SetPageUptodate(push_page);
skip_page:
		unlock_page(push_page);
		if(i != page->index)
			page_cache_release(push_page);
	}

	if (SQUASHFS_I(inode)->u.s1.fragment_start_block == SQUASHFS_INVALID_BLK
					|| index < file_end) {
		if (!sparse)
			up(&msblk->read_page_mutex);
		kfree(block_list);
	} else
		release_cached_fragment(msblk, fragment);

	return 0;

error_out:
	SetPageError(page);
out:
	pageaddr = kmap_atomic(page, KM_USER0);
	memset(pageaddr, 0, PAGE_CACHE_SIZE);
	kunmap_atomic(pageaddr, KM_USER0);
	flush_dcache_page(page);
	if (!PageError(page))
		SetPageUptodate(page);
	unlock_page(page);

	kfree(block_list);
	return 0;
}


static int get_dir_index_using_offset(struct super_block *s,
				long long *next_block, unsigned int *next_offset,
				long long index_start, unsigned int index_offset, int i_count,
				long long f_pos)
{
	struct squashfs_sb_info *msblk = s->s_fs_info;
	struct squashfs_super_block *sblk = &msblk->sblk;
	int i, length = 0;
	struct squashfs_dir_index index;

	TRACE("Entered get_dir_index_using_offset, i_count %d, f_pos %d\n",
					i_count, (unsigned int) f_pos);

	f_pos =- 3;
	if (f_pos == 0)
		goto finish;

	for (i = 0; i < i_count; i++) {
		if (msblk->swap) {
			struct squashfs_dir_index sindex;
			squashfs_get_cached_block(s, &sindex, index_start, index_offset,
					sizeof(sindex), &index_start, &index_offset);
			SQUASHFS_SWAP_DIR_INDEX(&index, &sindex);
		} else
			squashfs_get_cached_block(s, &index, index_start, index_offset,
					sizeof(index), &index_start, &index_offset);

		if (index.index > f_pos)
			break;

		squashfs_get_cached_block(s, NULL, index_start, index_offset,
					index.size + 1, &index_start, &index_offset);

		length = index.index;
		*next_block = index.start_block + sblk->directory_table_start;
	}

	*next_offset = (length + *next_offset) % SQUASHFS_METADATA_SIZE;

finish:
	return length + 3;
}


static int get_dir_index_using_name(struct super_block *s,
				long long *next_block, unsigned int *next_offset,
				long long index_start, unsigned int index_offset, int i_count,
				const char *name, int size)
{
	struct squashfs_sb_info *msblk = s->s_fs_info;
	struct squashfs_super_block *sblk = &msblk->sblk;
	int i, length = 0;
	struct squashfs_dir_index *index;
	char *str;

	TRACE("Entered get_dir_index_using_name, i_count %d\n", i_count);

	str = kmalloc(sizeof(struct squashfs_dir_index) +
		(SQUASHFS_NAME_LEN + 1) * 2, GFP_KERNEL);
	if (str == NULL) {
		ERROR("Failed to allocate squashfs_dir_index\n");
		goto failure;
	}

	index = (struct squashfs_dir_index *) (str + SQUASHFS_NAME_LEN + 1);
	strncpy(str, name, size);
	str[size] = '\0';

	for (i = 0; i < i_count; i++) {
		if (msblk->swap) {
			struct squashfs_dir_index sindex;
			squashfs_get_cached_block(s, &sindex, index_start, index_offset,
				sizeof(sindex), &index_start, &index_offset);
			SQUASHFS_SWAP_DIR_INDEX(index, &sindex);
		} else
			squashfs_get_cached_block(s, index, index_start, index_offset,
				sizeof(struct squashfs_dir_index), &index_start, &index_offset);

		squashfs_get_cached_block(s, index->name, index_start, index_offset,
					index->size + 1, &index_start, &index_offset);

		index->name[index->size + 1] = '\0';

		if (strcmp(index->name, str) > 0)
			break;

		length = index->index;
		*next_block = index->start_block + sblk->directory_table_start;
	}

	*next_offset = (length + *next_offset) % SQUASHFS_METADATA_SIZE;
	kfree(str);

failure:
	return length + 3;
}


static int squashfs_readdir(struct file *file, void *dirent, filldir_t filldir)
{
	struct inode *i = file->f_dentry->d_inode;
	struct squashfs_sb_info *msblk = i->i_sb->s_fs_info;
	struct squashfs_super_block *sblk = &msblk->sblk;
	long long next_block = SQUASHFS_I(i)->start_block +
		sblk->directory_table_start;
	int next_offset = SQUASHFS_I(i)->offset, length = 0, dir_count;
	struct squashfs_dir_header dirh;
	struct squashfs_dir_entry *dire;

	TRACE("Entered squashfs_readdir [%llx:%x]\n", next_block, next_offset);

	dire = kmalloc(sizeof(struct squashfs_dir_entry) +
		SQUASHFS_NAME_LEN + 1, GFP_KERNEL);
	if (dire == NULL) {
		ERROR("Failed to allocate squashfs_dir_entry\n");
		goto finish;
	}

	while(file->f_pos < 3) {
		char *name;
		int size, i_ino;

		if(file->f_pos == 0) {
			name = ".";
			size = 1;
			i_ino = i->i_ino;
		} else {
			name = "..";
			size = 2;
			i_ino = SQUASHFS_I(i)->u.s2.parent_inode;
		}
		TRACE("Calling filldir(%x, %s, %d, %d, %d, %d)\n",
				(unsigned int) dirent, name, size, (int)
				file->f_pos, i_ino, squashfs_filetype_table[1]);

		if (filldir(dirent, name, size, file->f_pos, i_ino,
				squashfs_filetype_table[1]) < 0) {
				TRACE("Filldir returned less than 0\n");
			goto finish;
		}
		file->f_pos += size;
	}

	length = get_dir_index_using_offset(i->i_sb, &next_block, &next_offset,
				SQUASHFS_I(i)->u.s2.directory_index_start,
				SQUASHFS_I(i)->u.s2.directory_index_offset,
				SQUASHFS_I(i)->u.s2.directory_index_count, file->f_pos);

	while (length < i_size_read(i)) {
		/* read directory header */
		if (msblk->swap) {
			struct squashfs_dir_header sdirh;

			if (!squashfs_get_cached_block(i->i_sb, &sdirh, next_block,
					 next_offset, sizeof(sdirh), &next_block, &next_offset))
				goto failed_read;

			length += sizeof(sdirh);
			SQUASHFS_SWAP_DIR_HEADER(&dirh, &sdirh);
		} else {
			if (!squashfs_get_cached_block(i->i_sb, &dirh, next_block,
					next_offset, sizeof(dirh), &next_block, &next_offset))
				goto failed_read;

			length += sizeof(dirh);
		}

		dir_count = dirh.count + 1;
		while (dir_count--) {
			if (msblk->swap) {
				struct squashfs_dir_entry sdire;
				if (!squashfs_get_cached_block(i->i_sb, &sdire, next_block,
						next_offset, sizeof(sdire), &next_block, &next_offset))
					goto failed_read;

				length += sizeof(sdire);
				SQUASHFS_SWAP_DIR_ENTRY(dire, &sdire);
			} else {
				if (!squashfs_get_cached_block(i->i_sb, dire, next_block,
						next_offset, sizeof(*dire), &next_block, &next_offset))
					goto failed_read;

				length += sizeof(*dire);
			}

			if (!squashfs_get_cached_block(i->i_sb, dire->name, next_block,
						next_offset, dire->size + 1, &next_block, &next_offset))
				goto failed_read;

			length += dire->size + 1;

			if (file->f_pos >= length)
				continue;

			dire->name[dire->size + 1] = '\0';

			TRACE("Calling filldir(%x, %s, %d, %d, %x:%x, %d, %d)\n",
					(unsigned int) dirent, dire->name, dire->size + 1,
					(int) file->f_pos, dirh.start_block, dire->offset,
					dirh.inode_number + dire->inode_number,
					squashfs_filetype_table[dire->type]);

			if (filldir(dirent, dire->name, dire->size + 1, file->f_pos,
					dirh.inode_number + dire->inode_number,
					squashfs_filetype_table[dire->type]) < 0) {
				TRACE("Filldir returned less than 0\n");
				goto finish;
			}
			file->f_pos = length;
		}
	}

finish:
	kfree(dire);
	return 0;

failed_read:
	ERROR("Unable to read directory block [%llx:%x]\n", next_block,
		next_offset);
	kfree(dire);
	return 0;
}


static struct dentry *squashfs_lookup(struct inode *i, struct dentry *dentry,
				struct nameidata *nd)
{
	const unsigned char *name = dentry->d_name.name;
	int len = dentry->d_name.len;
	struct inode *inode = NULL;
	struct squashfs_sb_info *msblk = i->i_sb->s_fs_info;
	struct squashfs_super_block *sblk = &msblk->sblk;
	long long next_block = SQUASHFS_I(i)->start_block +
				sblk->directory_table_start;
	int next_offset = SQUASHFS_I(i)->offset, length = 0, dir_count;
	struct squashfs_dir_header dirh;
	struct squashfs_dir_entry *dire;

	TRACE("Entered squashfs_lookup [%llx:%x]\n", next_block, next_offset);

	dire = kmalloc(sizeof(struct squashfs_dir_entry) +
		SQUASHFS_NAME_LEN + 1, GFP_KERNEL);
	if (dire == NULL) {
		ERROR("Failed to allocate squashfs_dir_entry\n");
		goto exit_lookup;
	}

	if (len > SQUASHFS_NAME_LEN)
		goto exit_lookup;

	length = get_dir_index_using_name(i->i_sb, &next_block, &next_offset,
				SQUASHFS_I(i)->u.s2.directory_index_start,
				SQUASHFS_I(i)->u.s2.directory_index_offset,
				SQUASHFS_I(i)->u.s2.directory_index_count, name, len);

	while (length < i_size_read(i)) {
		/* read directory header */
		if (msblk->swap) {
			struct squashfs_dir_header sdirh;
			if (!squashfs_get_cached_block(i->i_sb, &sdirh, next_block,
					 next_offset, sizeof(sdirh), &next_block, &next_offset))
				goto failed_read;

			length += sizeof(sdirh);
			SQUASHFS_SWAP_DIR_HEADER(&dirh, &sdirh);
		} else {
			if (!squashfs_get_cached_block(i->i_sb, &dirh, next_block,
					next_offset, sizeof(dirh), &next_block, &next_offset))
				goto failed_read;

			length += sizeof(dirh);
		}

		dir_count = dirh.count + 1;
		while (dir_count--) {
			if (msblk->swap) {
				struct squashfs_dir_entry sdire;
				if (!squashfs_get_cached_block(i->i_sb, &sdire, next_block,
						next_offset, sizeof(sdire), &next_block, &next_offset))
					goto failed_read;

				length += sizeof(sdire);
				SQUASHFS_SWAP_DIR_ENTRY(dire, &sdire);
			} else {
				if (!squashfs_get_cached_block(i->i_sb, dire, next_block,
						next_offset, sizeof(*dire), &next_block, &next_offset))
					goto failed_read;

				length += sizeof(*dire);
			}

			if (!squashfs_get_cached_block(i->i_sb, dire->name, next_block,
					next_offset, dire->size + 1, &next_block, &next_offset))
				goto failed_read;

			length += dire->size + 1;

			if (name[0] < dire->name[0])
				goto exit_lookup;

			if ((len == dire->size + 1) && !strncmp(name, dire->name, len)) {
				squashfs_inode_t ino = SQUASHFS_MKINODE(dirh.start_block,
								dire->offset);

				TRACE("calling squashfs_iget for directory entry %s, inode"
					"  %x:%x, %d\n", name, dirh.start_block, dire->offset,
					dirh.inode_number + dire->inode_number);

				inode = squashfs_iget(i->i_sb, ino, dirh.inode_number + dire->inode_number);

				goto exit_lookup;
			}
		}
	}

exit_lookup:
	kfree(dire);
	if (inode)
		return d_splice_alias(inode, dentry);
	d_add(dentry, inode);
	return ERR_PTR(0);

failed_read:
	ERROR("Unable to read directory block [%llx:%x]\n", next_block,
		next_offset);
	goto exit_lookup;
}


static int squashfs_remount(struct super_block *s, int *flags, char *data)
{
	*flags |= MS_RDONLY;
	return 0;
}


static void squashfs_put_super(struct super_block *s)
{
	int i;

	if (s->s_fs_info) {
		struct squashfs_sb_info *sbi = s->s_fs_info;
		if (sbi->block_cache)
			for (i = 0; i < squashfs_cached_blks; i++)
				if (sbi->block_cache[i].block != SQUASHFS_INVALID_BLK)
					vfree(sbi->block_cache[i].data);
		if (sbi->fragment)
			for (i = 0; i < SQUASHFS_CACHED_FRAGMENTS; i++) 
				vfree(sbi->fragment[i].data);
		kfree(sbi->fragment);
		kfree(sbi->block_cache);
		vfree(sbi->read_page);
		kfree(sbi->uid);
		kfree(sbi->fragment_index);
		kfree(sbi->fragment_index_2);
		kfree(sbi->meta_index);
		kfree(s->s_fs_info);
		s->s_fs_info = NULL;
	}
}


#ifdef CONFIG_SQUASHFS_MTD

static void squashfs_kill_sb(struct super_block *sb)
{
    struct squashfs_sb_info *c = sb->s_fs_info;
    generic_shutdown_super(sb);
    put_mtd_device(c->mtd);
    kfree(c);
}

static int squashfs_sb_compare(struct super_block *sb, void *data)
{
        struct squashfs_sb_info *p = (struct squashfs_sb_info *)data;
        struct squashfs_sb_info *c = sb->s_fs_info;

        /* The superblocks are considered to be equivalent if the underlying MTD
           device is the same one */
        if (c->mtd == p->mtd) {
                printk("%s: match on device %d (\"%s\")\n", __func__, p->mtd->index, p->mtd->name);
                return 1;
        } else {
                printk("%s: No match, device %d (\"%s\"), device %d (\"%s\")\n",
                          __func__,c->mtd->index, c->mtd->name, p->mtd->index, p->mtd->name);
                return 0;
        }
}

static int squashfs_sb_set(struct super_block *sb, void *data)
{
        struct squashfs_sb_info *p = data;

        /* For persistence of NFS exports etc. we use the same s_dev
           each time we mount the device, don't just use an anonymous
           device */
        printk("%s(): assigning p to sb->s_fs_info p = 0x%p p->mtd = 0x%p p->mtd->name:%s\n", __func__, p, p->mtd, p->mtd->name);
        sb->s_fs_info = p;
        sb->s_dev = MKDEV(MTD_BLOCK_MAJOR, p->mtd->index);

        return 0;
}

static struct super_block *squashfs_get_sb_mtd(struct file_system_type *fs_type, int flags,
                                const char *dev_name, void *data, struct mtd_info *mtd)
{
        struct super_block *sb;
        struct squashfs_sb_info *c;
        int ret;

        c = kmalloc(sizeof(struct squashfs_sb_info), GFP_KERNEL);

        if (!c)
                return ERR_PTR(-ENOMEM);
        memset(c, 0, sizeof(*c));

        c->mtd = mtd;

        printk("%s(): c = 0x%p c->mtd = 0x%p\n", __func__, c, c->mtd);

        sb = sget(fs_type, squashfs_sb_compare, squashfs_sb_set, c); 

        if (IS_ERR(sb))
                goto out_put;

        if (sb->s_root) {
                /* New mountpoint for JFFS2 which is already mounted */
                printk(KERN_DEBUG "%s(): Device %d (\"%s\") is already mounted\n",
                          __func__, c->mtd->index, c->mtd->name);
                goto out_put;
        }

        printk("%s(): New superblock for device %d (\"%s\")\n",
                  __func__, mtd->index, mtd->name);

        /* Initialize squashfs superblock locks, the further initialization will be
         * done later */

        ret = squashfs_fill_super(sb, data, (flags & MS_VERBOSE) ? 1 : 0);

        if (ret) {
                /* Failure case... */
                up_write(&sb->s_umount);
                deactivate_super(sb);
                return ERR_PTR(ret);
        }

        sb->s_flags |= MS_ACTIVE;
        return sb;

 out_put:
        kfree(c);
        put_mtd_device(mtd);
        return sb;
}

static struct super_block *squashfs_get_sb_mtdnr(struct file_system_type *fs_type,
                                              int flags, const char *dev_name,
                                              void *data, int mtdnr)
{
        struct mtd_info *mtd;

        //printk("%s(): dev_name = %s mtdnr=%d\n", __func__, dev_name, mtdnr);
        mtd = get_mtd_device(NULL, mtdnr);
        if (!mtd) {
                printk(KERN_DEBUG "%s(): MTD device #%u doesn't appear to exist\n", __func__,  mtdnr);
                return ERR_PTR(-EINVAL);
        }

        //printk("%s(): calling squashfs_get_sb_mtd, mtd->name: %s mtd->index: %d\n", __func__, mtd->name, mtd->index);
        return squashfs_get_sb_mtd(fs_type, flags, dev_name, data, mtd);
}
#endif

static struct super_block *squashfs_get_sb(struct file_system_type *fs_type, int flags,
				const char *dev_name, void *data)
{
#ifdef CONFIG_SQUASHFS_MTD
        int err;
        int mtdnr;
        struct nameidata nd;
        struct mtd_info *mtd;


        if (dev_name[0] == 'm' && dev_name[1] == 't' && dev_name[2] == 'd') {
                /* Probably mounting without the blkdev crap */
                if (dev_name[3] == ':') {

                    for (mtdnr = 0; mtdnr < MAX_MTD_DEVICES; mtdnr++) {
                        mtd = get_mtd_device(NULL, mtdnr);
                        if (mtd) {
                                if (!strcmp(mtd->name, dev_name+4))
                                {
                                       // printk("%s(): mtd name : %s, dev_name  %s\n", __func__, mtd->name, dev_name+4);
                                        return squashfs_get_sb_mtd(fs_type, flags, dev_name, data, mtd);
                                }
                                put_mtd_device(mtd);
                        }
                    }
                    printk(KERN_NOTICE "%s(): MTD device with name \"%s\" not found.\n",__func__, dev_name+4);
                } else if (isdigit(dev_name[3])) {
                        /* Mount by MTD device number name */
                        char *endptr;

                        mtdnr = simple_strtoul(dev_name+3, &endptr, 0);
                        if (!*endptr) {
                                /* It was a valid number */
                                printk("%s(): mtd%%d, mtdnr %d\n", __func__, mtdnr);
                                return squashfs_get_sb_mtdnr(fs_type, flags, dev_name, data, mtdnr);
                        }
                }
        }

        /* Try the old way - the hack where we allowed users to mount
           /dev/mtdblock$(n) but didn't actually _use_ the blkdev */

        err = path_lookup(dev_name, LOOKUP_FOLLOW, &nd);

       /* printk(KERN_DEBUG "%s(): path_lookup() returned %d, inode %p\n",
                  __func__, err, nd.dentry->d_inode); */

        if (err)
                return ERR_PTR(err);

        err = -EINVAL;

        if (!S_ISBLK(nd.dentry->d_inode->i_mode))
                goto out;

        if (nd.mnt->mnt_flags & MNT_NODEV) {
                err = -EACCES;
                goto out;
        }

        if (imajor(nd.dentry->d_inode) != MTD_BLOCK_MAJOR) {
                if (!(flags & MS_VERBOSE)) /* Yes I mean this. Strangely */
                        printk(KERN_NOTICE "Attempt to mount non-MTD device \"%s\" as squashfs\n",
                               dev_name);
                goto out;
        }

        mtdnr = iminor(nd.dentry->d_inode);
        path_release(&nd);

        return squashfs_get_sb_mtdnr(fs_type, flags, dev_name, data, mtdnr);

out:
        path_release(&nd);
        return ERR_PTR(err);
#else
	return get_sb_bdev(fs_type, flags, dev_name, data, squashfs_fill_super);
#endif

}

static void free_sqlzma(void)
{
	int cpu;
	struct sqlzma *p;

	for_each_online_cpu(cpu) {
		p = per_cpu(sqlzma, cpu);
		if (p) {
#ifdef KeepPreemptive
			mutex_destroy(&p->mtx);
#endif
			sqlzma_fin(&p->un);
			kfree(p);
		}
	}
}

static int __init init_squashfs_fs(void)
{
	int err = 0;
	struct sqlzma *p;
	int cpu;

        err = init_inodecache();

	if (err)
		goto out;

	for_each_online_cpu(cpu) {
		dpri("%d: %p\n", cpu, per_cpu(sqlzma, cpu));
		err = -ENOMEM;
		p = kmalloc(sizeof(struct sqlzma), GFP_KERNEL);
		if (p) {
#ifdef KeepPreemptive
			mutex_init(&p->mtx);
#endif
			err = sqlzma_init(&p->un, 1, 0);
			if (unlikely(err)) {
				ERROR("Failed to intialize uncompress workspace\n");
				break;
			}
			per_cpu(sqlzma, cpu) = p;
			err = 0;
		} else
			break;
	}

	if (unlikely(err)) {
		free_sqlzma();
		goto out;
	}

	printk(KERN_INFO "squashfs: version 3.3 (2007/10/31) "
		"Phillip Lougher\n"
		"squashfs: LZMA suppport for slax.org by jro\n");

	err = register_filesystem(&squashfs_fs_type);
	if (err) {
		free_sqlzma();
		destroy_inodecache();
	}

out:
	return err;
}


static void __exit exit_squashfs_fs(void)
{
	unregister_filesystem(&squashfs_fs_type);
	free_sqlzma();
	destroy_inodecache();
}


static kmem_cache_t * squashfs_inode_cachep;


static struct inode *squashfs_alloc_inode(struct super_block *sb)
{
	struct squashfs_inode_info *ei;
	ei = kmem_cache_alloc(squashfs_inode_cachep, SLAB_KERNEL);
	return ei ? &ei->vfs_inode : NULL;
}


static void squashfs_destroy_inode(struct inode *inode)
{
	kmem_cache_free(squashfs_inode_cachep, SQUASHFS_I(inode));
}


static void init_once(void * foo, kmem_cache_t * cachep, unsigned long flags)
{
	struct squashfs_inode_info *ei = foo;

	if ((flags & (SLAB_CTOR_VERIFY|SLAB_CTOR_CONSTRUCTOR)) == SLAB_CTOR_CONSTRUCTOR)
		inode_init_once(&ei->vfs_inode);
}


static int __init init_inodecache(void)
{
	squashfs_inode_cachep = kmem_cache_create("squashfs_inode_cache",
	    sizeof(struct squashfs_inode_info), 0,
		SLAB_HWCACHE_ALIGN|SLAB_RECLAIM_ACCOUNT, init_once, NULL);
	if (squashfs_inode_cachep == NULL)
		return -ENOMEM;
	return 0;
}


static void destroy_inodecache(void)
{
	if (kmem_cache_destroy(squashfs_inode_cachep))
		printk(KERN_INFO "squashfs_inode_cache: not all structures were freed\n");

}


module_init(init_squashfs_fs);
module_exit(exit_squashfs_fs);
MODULE_DESCRIPTION("squashfs 3.2-r2-CVS, a compressed read-only filesystem, and LZMA suppport for slax.org");
MODULE_AUTHOR("Phillip Lougher <phillip@lougher.demon.co.uk>, and LZMA suppport for slax.org by jro");
MODULE_LICENSE("GPL");
