#ifndef _GDTH_PROC_H
#define _GDTH_PROC_H

/* gdth_proc.h 
 * $Id: //depot/sw/releases/Aquila_9.2.0_U11/linux/kernels/mips-linux-2.6.31/drivers/scsi/gdth_proc.h#1 $
 */

int gdth_execute(struct Scsi_Host *shost, gdth_cmd_str *gdtcmd, char *cmnd,
                 int timeout, u32 *info);

static int gdth_set_info(char *buffer,int length,struct Scsi_Host *host,
                         gdth_ha_str *ha);
static int gdth_get_info(char *buffer,char **start,off_t offset,int length,
                         struct Scsi_Host *host, gdth_ha_str *ha);

static int gdth_set_asc_info(struct Scsi_Host *host, char *buffer,
                             int length, gdth_ha_str *ha);

static char *gdth_ioctl_alloc(gdth_ha_str *ha, int size, int scratch,
                              ulong64 *paddr);
static void gdth_ioctl_free(gdth_ha_str *ha, int size, char *buf, ulong64 paddr);
static void gdth_wait_completion(gdth_ha_str *ha, int busnum, int id);

#endif

