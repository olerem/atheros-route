/* $Id: //depot/sw/releases/Aquila_9.2.0_U11/linux/kernels/mips-linux-2.6.15_2MB/include/asm-sparc/sbus.h#1 $
 * sbus.h:  Defines for the Sun SBus.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 */

#ifndef _SPARC_SBUS_H
#define _SPARC_SBUS_H

#include <linux/dma-mapping.h>
#include <linux/ioport.h>

#include <asm/oplib.h>
/* #include <asm/iommu.h> */ /* Unused since we use opaque iommu (|io-unit) */
#include <asm/scatterlist.h>

/* We scan which devices are on the SBus using the PROM node device
 * tree.  SBus devices are described in two different ways.  You can
 * either get an absolute address at which to access the device, or
 * you can get a SBus 'slot' number and an offset within that slot.
 */

/* The base address at which to calculate device OBIO addresses. */
#define SUN_SBUS_BVADDR        0xf8000000
#define SBUS_OFF_MASK          0x01ffffff

/* These routines are used to calculate device address from slot
 * numbers + offsets, and vice versa.
 */

static inline unsigned long sbus_devaddr(int slotnum, unsigned long offset)
{
  return (unsigned long) (SUN_SBUS_BVADDR+((slotnum)<<25)+(offset));
}

static inline int sbus_dev_slot(unsigned long dev_addr)
{
  return (int) (((dev_addr)-SUN_SBUS_BVADDR)>>25);
}

struct sbus_bus;

/* Linux SBUS device tables */
struct sbus_dev {
	struct sbus_bus	*bus;       /* Back ptr to sbus */
	struct sbus_dev	*next;      /* next device on this SBus or null */
	struct sbus_dev	*child;     /* For ledma and espdma on sun4m */
	struct sbus_dev	*parent;    /* Parent device if not toplevel */
	int prom_node;              /* PROM device tree node for this device */
	char prom_name[64];         /* PROM device name */
	int slot;

	struct resource resource[PROMREG_MAX];

	struct linux_prom_registers reg_addrs[PROMREG_MAX];
	int num_registers, ranges_applied;

	struct linux_prom_ranges device_ranges[PROMREG_MAX];
	int num_device_ranges;

	unsigned int irqs[4];
	int num_irqs;
};

/* This struct describes the SBus(s) found on this machine. */
struct sbus_bus {
	void			*iommu;		/* Opaque IOMMU cookie */
	struct sbus_dev		*devices;	/* Link to devices on this SBus */
	struct sbus_bus		*next;		/* next SBus, if more than one SBus */
	int			prom_node;	/* PROM device tree node for this SBus */
	char			prom_name[64];  /* Usually "sbus" or "sbi" */
	int			clock_freq;

	struct linux_prom_ranges sbus_ranges[PROMREG_MAX];
	int num_sbus_ranges;

	int devid;
	int board;
};

extern struct sbus_bus *sbus_root;

static inline int
sbus_is_slave(struct sbus_dev *dev)
{
	/* XXX Have to write this for sun4c's */
	return 0;
}

/* Device probing routines could find these handy */
#define for_each_sbus(bus) \
        for((bus) = sbus_root; (bus); (bus)=(bus)->next)

#define for_each_sbusdev(device, bus) \
        for((device) = (bus)->devices; (device); (device)=(device)->next)
        
#define for_all_sbusdev(device, bus) \
	for ((bus) = sbus_root; (bus); (bus) = (bus)->next) \
		for ((device) = (bus)->devices; (device); (device) = (device)->next)

/* Driver DVMA interfaces. */
#define sbus_can_dma_64bit(sdev)	(0) /* actually, sparc_cpu_model==sun4d */
#define sbus_can_burst64(sdev)		(0) /* actually, sparc_cpu_model==sun4d */
extern void sbus_set_sbus64(struct sbus_dev *, int);

/* These yield IOMMU mappings in consistent mode. */
extern void *sbus_alloc_consistent(struct sbus_dev *, long, u32 *dma_addrp);
extern void sbus_free_consistent(struct sbus_dev *, long, void *, u32);
void prom_adjust_ranges(struct linux_prom_ranges *, int,
			struct linux_prom_ranges *, int);

#define SBUS_DMA_BIDIRECTIONAL	DMA_BIDIRECTIONAL
#define SBUS_DMA_TODEVICE	DMA_TO_DEVICE
#define SBUS_DMA_FROMDEVICE	DMA_FROM_DEVICE
#define	SBUS_DMA_NONE		DMA_NONE

/* All the rest use streaming mode mappings. */
extern dma_addr_t sbus_map_single(struct sbus_dev *, void *, size_t, int);
extern void sbus_unmap_single(struct sbus_dev *, dma_addr_t, size_t, int);
extern int sbus_map_sg(struct sbus_dev *, struct scatterlist *, int, int);
extern void sbus_unmap_sg(struct sbus_dev *, struct scatterlist *, int, int);

/* Finally, allow explicit synchronization of streamable mappings. */
extern void sbus_dma_sync_single_for_cpu(struct sbus_dev *, dma_addr_t, size_t, int);
#define sbus_dma_sync_single sbus_dma_sync_single_for_cpu
extern void sbus_dma_sync_single_for_device(struct sbus_dev *, dma_addr_t, size_t, int);
extern void sbus_dma_sync_sg_for_cpu(struct sbus_dev *, struct scatterlist *, int, int);
#define sbus_dma_sync_sg sbus_dma_sync_sg_for_cpu
extern void sbus_dma_sync_sg_for_device(struct sbus_dev *, struct scatterlist *, int, int);

/* Eric Brower (ebrower@usa.net)
 * Translate SBus interrupt levels to ino values--
 * this is used when converting sbus "interrupts" OBP 
 * node values to "intr" node values, and is platform 
 * dependent.  If only we could call OBP with 
 * "sbus-intr>cpu (sbint -- ino)" from kernel...
 * See .../drivers/sbus/sbus.c for details.
 */
BTFIXUPDEF_CALL(unsigned int, sbint_to_irq, struct sbus_dev *sdev, unsigned int)
#define sbint_to_irq(sdev, sbint) BTFIXUP_CALL(sbint_to_irq)(sdev, sbint)

#endif /* !(_SPARC_SBUS_H) */
