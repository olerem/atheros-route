/*  $Id: //depot/sw/releases/Aquila_9.2.0_U11/linux/kernels/mips-linux-2.6.15/arch/sparc/kernel/setup.c#1 $
 *  linux/arch/sparc/kernel/setup.c
 *
 *  Copyright (C) 1995  David S. Miller (davem@caip.rutgers.edu)
 *  Copyright (C) 2000  Anton Blanchard (anton@samba.org)
 */

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/initrd.h>
#include <asm/smp.h>
#include <linux/user.h>
#include <linux/a.out.h>
#include <linux/tty.h>
#include <linux/delay.h>
#include <linux/config.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/syscalls.h>
#include <linux/kdev_t.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/console.h>
#include <linux/spinlock.h>
#include <linux/root_dev.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/processor.h>
#include <asm/oplib.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/traps.h>
#include <asm/vaddrs.h>
#include <asm/kdebug.h>
#include <asm/mbus.h>
#include <asm/idprom.h>
#include <asm/machines.h>
#include <asm/cpudata.h>
#include <asm/setup.h>

struct screen_info screen_info = {
	0, 0,			/* orig-x, orig-y */
	0,			/* unused */
	0,			/* orig-video-page */
	0,			/* orig-video-mode */
	128,			/* orig-video-cols */
	0,0,0,			/* ega_ax, ega_bx, ega_cx */
	54,			/* orig-video-lines */
	0,                      /* orig-video-isVGA */
	16                      /* orig-video-points */
};

/* Typing sync at the prom prompt calls the function pointed to by
 * romvec->pv_synchook which I set to the following function.
 * This should sync all filesystems and return, for now it just
 * prints out pretty messages and returns.
 */

extern unsigned long trapbase;
void (*prom_palette)(int);

/* Pretty sick eh? */
void prom_sync_me(void)
{
	unsigned long prom_tbr, flags;

	/* XXX Badly broken. FIX! - Anton */
	local_irq_save(flags);
	__asm__ __volatile__("rd %%tbr, %0\n\t" : "=r" (prom_tbr));
	__asm__ __volatile__("wr %0, 0x0, %%tbr\n\t"
			     "nop\n\t"
			     "nop\n\t"
			     "nop\n\t" : : "r" (&trapbase));

	if (prom_palette)
		prom_palette(1);
	prom_printf("PROM SYNC COMMAND...\n");
	show_free_areas();
	if(current->pid != 0) {
		local_irq_enable();
		sys_sync();
		local_irq_disable();
	}
	prom_printf("Returning to prom\n");

	__asm__ __volatile__("wr %0, 0x0, %%tbr\n\t"
			     "nop\n\t"
			     "nop\n\t"
			     "nop\n\t" : : "r" (prom_tbr));
	local_irq_restore(flags);

	return;
}

unsigned int boot_flags __initdata = 0;
#define BOOTME_DEBUG  0x1
#define BOOTME_SINGLE 0x2

/* Exported for mm/init.c:paging_init. */
unsigned long cmdline_memory_size __initdata = 0;

static void
prom_console_write(struct console *con, const char *s, unsigned n)
{
	prom_write(s, n);
}

static struct console prom_debug_console = {
	.name =		"debug",
	.write =	prom_console_write,
	.flags =	CON_PRINTBUFFER,
	.index =	-1,
};

int obp_system_intr(void)
{
	if (boot_flags & BOOTME_DEBUG) {
		printk("OBP: system interrupted\n");
		prom_halt();
		return 1;
	}
	return 0;
}

/* 
 * Process kernel command line switches that are specific to the
 * SPARC or that require special low-level processing.
 */
static void __init process_switch(char c)
{
	switch (c) {
	case 'd':
		boot_flags |= BOOTME_DEBUG;
		break;
	case 's':
		boot_flags |= BOOTME_SINGLE;
		break;
	case 'h':
		prom_printf("boot_flags_init: Halt!\n");
		prom_halt();
		break;
	case 'p':
		/* Use PROM debug console. */
		register_console(&prom_debug_console);
		break;
	default:
		printk("Unknown boot switch (-%c)\n", c);
		break;
	}
}

static void __init process_console(char *commands)
{
	serial_console = 0;
	commands += 8;
	/* Linux-style serial */
	if (!strncmp(commands, "ttyS", 4))
		serial_console = simple_strtoul(commands + 4, NULL, 10) + 1;
	else if (!strncmp(commands, "tty", 3)) {
		char c = *(commands + 3);
		/* Solaris-style serial */
		if (c == 'a' || c == 'b')
			serial_console = c - 'a' + 1;
		/* else Linux-style fbcon, not serial */
	}
#if defined(CONFIG_PROM_CONSOLE)
	if (!strncmp(commands, "prom", 4)) {
		char *p;

		for (p = commands - 8; *p && *p != ' '; p++)
			*p = ' ';
		conswitchp = &prom_con;
	}
#endif
}

static void __init boot_flags_init(char *commands)
{
	while (*commands) {
		/* Move to the start of the next "argument". */
		while (*commands && *commands == ' ')
			commands++;

		/* Process any command switches, otherwise skip it. */
		if (*commands == '\0')
			break;
		if (*commands == '-') {
			commands++;
			while (*commands && *commands != ' ')
				process_switch(*commands++);
			continue;
		}
		if (!strncmp(commands, "console=", 8)) {
			process_console(commands);
		} else if (!strncmp(commands, "mem=", 4)) {
			/*
			 * "mem=XXX[kKmM] overrides the PROM-reported
			 * memory size.
			 */
			cmdline_memory_size = simple_strtoul(commands + 4,
						     &commands, 0);
			if (*commands == 'K' || *commands == 'k') {
				cmdline_memory_size <<= 10;
				commands++;
			} else if (*commands=='M' || *commands=='m') {
				cmdline_memory_size <<= 20;
				commands++;
			}
		}
		while (*commands && *commands != ' ')
			commands++;
	}
}

/* This routine will in the future do all the nasty prom stuff
 * to probe for the mmu type and its parameters, etc. This will
 * also be where SMP things happen plus the Sparc specific memory
 * physical memory probe as on the alpha.
 */

extern int prom_probe_memory(void);
extern void sun4c_probe_vac(void);
extern char cputypval;
extern unsigned long start, end;
extern void panic_setup(char *, int *);

extern unsigned short root_flags;
extern unsigned short root_dev;
extern unsigned short ram_flags;
#define RAMDISK_IMAGE_START_MASK	0x07FF
#define RAMDISK_PROMPT_FLAG		0x8000
#define RAMDISK_LOAD_FLAG		0x4000

extern int root_mountflags;

char reboot_command[COMMAND_LINE_SIZE];
enum sparc_cpu sparc_cpu_model;

struct tt_entry *sparc_ttable;

struct pt_regs fake_swapper_regs;

void __init setup_arch(char **cmdline_p)
{
	int i;
	unsigned long highest_paddr;

	sparc_ttable = (struct tt_entry *) &start;

	/* Initialize PROM console and command line. */
	*cmdline_p = prom_getbootargs();
	strcpy(saved_command_line, *cmdline_p);

	/* Set sparc_cpu_model */
	sparc_cpu_model = sun_unknown;
	if(!strcmp(&cputypval,"sun4 ")) { sparc_cpu_model=sun4; }
	if(!strcmp(&cputypval,"sun4c")) { sparc_cpu_model=sun4c; }
	if(!strcmp(&cputypval,"sun4m")) { sparc_cpu_model=sun4m; }
	if(!strcmp(&cputypval,"sun4s")) { sparc_cpu_model=sun4m; }  /* CP-1200 with PROM 2.30 -E */
	if(!strcmp(&cputypval,"sun4d")) { sparc_cpu_model=sun4d; }
	if(!strcmp(&cputypval,"sun4e")) { sparc_cpu_model=sun4e; }
	if(!strcmp(&cputypval,"sun4u")) { sparc_cpu_model=sun4u; }

#ifdef CONFIG_SUN4
	if (sparc_cpu_model != sun4) {
		prom_printf("This kernel is for Sun4 architecture only.\n");
		prom_halt();
	}
#endif
	printk("ARCH: ");
	switch(sparc_cpu_model) {
	case sun4:
		printk("SUN4\n");
		break;
	case sun4c:
		printk("SUN4C\n");
		break;
	case sun4m:
		printk("SUN4M\n");
		break;
	case sun4d:
		printk("SUN4D\n");
		break;
	case sun4e:
		printk("SUN4E\n");
		break;
	case sun4u:
		printk("SUN4U\n");
		break;
	default:
		printk("UNKNOWN!\n");
		break;
	};

#ifdef CONFIG_DUMMY_CONSOLE
	conswitchp = &dummy_con;
#elif defined(CONFIG_PROM_CONSOLE)
	conswitchp = &prom_con;
#endif
	boot_flags_init(*cmdline_p);

	idprom_init();
	if (ARCH_SUN4C_SUN4)
		sun4c_probe_vac();
	load_mmu();
	(void) prom_probe_memory();

	phys_base = 0xffffffffUL;
	highest_paddr = 0UL;
	for (i = 0; sp_banks[i].num_bytes != 0; i++) {
		unsigned long top;

		if (sp_banks[i].base_addr < phys_base)
			phys_base = sp_banks[i].base_addr;
		top = sp_banks[i].base_addr +
			sp_banks[i].num_bytes;
		if (highest_paddr < top)
			highest_paddr = top;
	}
	pfn_base = phys_base >> PAGE_SHIFT;

	if (!root_flags)
		root_mountflags &= ~MS_RDONLY;
	ROOT_DEV = old_decode_dev(root_dev);
#ifdef CONFIG_BLK_DEV_INITRD
	rd_image_start = ram_flags & RAMDISK_IMAGE_START_MASK;
	rd_prompt = ((ram_flags & RAMDISK_PROMPT_FLAG) != 0);
	rd_doload = ((ram_flags & RAMDISK_LOAD_FLAG) != 0);	
#endif

	prom_setsync(prom_sync_me);

	if((boot_flags&BOOTME_DEBUG) && (linux_dbvec!=0) && 
	   ((*(short *)linux_dbvec) != -1)) {
		printk("Booted under KADB. Syncing trap table.\n");
		(*(linux_dbvec->teach_debugger))();
	}

	init_mm.context = (unsigned long) NO_CONTEXT;
	init_task.thread.kregs = &fake_swapper_regs;

	paging_init();
}

static int __init set_preferred_console(void)
{
	int idev, odev;

	/* The user has requested a console so this is already set up. */
	if (serial_console >= 0)
		return -EBUSY;

	idev = prom_query_input_device();
	odev = prom_query_output_device();
	if (idev == PROMDEV_IKBD && odev == PROMDEV_OSCREEN) {
		serial_console = 0;
	} else if (idev == PROMDEV_ITTYA && odev == PROMDEV_OTTYA) {
		serial_console = 1;
	} else if (idev == PROMDEV_ITTYB && odev == PROMDEV_OTTYB) {
		serial_console = 2;
	} else if (idev == PROMDEV_I_UNK && odev == PROMDEV_OTTYA) {
		prom_printf("MrCoffee ttya\n");
		serial_console = 1;
	} else if (idev == PROMDEV_I_UNK && odev == PROMDEV_OSCREEN) {
		serial_console = 0;
		prom_printf("MrCoffee keyboard\n");
	} else {
		prom_printf("Confusing console (idev %d, odev %d)\n",
		    idev, odev);
		serial_console = 1;
	}

	if (serial_console)
		return add_preferred_console("ttyS", serial_console - 1, NULL);

	return -ENODEV;
}
console_initcall(set_preferred_console);

extern char *sparc_cpu_type;
extern char *sparc_fpu_type;

static int show_cpuinfo(struct seq_file *m, void *__unused)
{
	seq_printf(m,
		   "cpu\t\t: %s\n"
		   "fpu\t\t: %s\n"
		   "promlib\t\t: Version %d Revision %d\n"
		   "prom\t\t: %d.%d\n"
		   "type\t\t: %s\n"
		   "ncpus probed\t: %d\n"
		   "ncpus active\t: %d\n"
#ifndef CONFIG_SMP
		   "CPU0Bogo\t: %lu.%02lu\n"
		   "CPU0ClkTck\t: %ld\n"
#endif
		   ,
		   sparc_cpu_type ? sparc_cpu_type : "undetermined",
		   sparc_fpu_type ? sparc_fpu_type : "undetermined",
		   romvec->pv_romvers,
		   prom_rev,
		   romvec->pv_printrev >> 16,
		   romvec->pv_printrev & 0xffff,
		   &cputypval,
		   num_possible_cpus(),
		   num_online_cpus()
#ifndef CONFIG_SMP
		   , cpu_data(0).udelay_val/(500000/HZ),
		   (cpu_data(0).udelay_val/(5000/HZ)) % 100,
		   cpu_data(0).clock_tick
#endif
		);

#ifdef CONFIG_SMP
	smp_bogo(m);
#endif
	mmu_info(m);
#ifdef CONFIG_SMP
	smp_info(m);
#endif
	return 0;
}

static void *c_start(struct seq_file *m, loff_t *pos)
{
	/* The pointer we are returning is arbitrary,
	 * it just has to be non-NULL and not IS_ERR
	 * in the success case.
	 */
	return *pos == 0 ? &c_start : NULL;
}

static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return c_start(m, pos);
}

static void c_stop(struct seq_file *m, void *v)
{
}

struct seq_operations cpuinfo_op = {
	.start =c_start,
	.next =	c_next,
	.stop =	c_stop,
	.show =	show_cpuinfo,
};

extern int stop_a_enabled;

void sun_do_break(void)
{
	if (!stop_a_enabled)
		return;

	printk("\n");
	flush_user_windows();

	prom_cmdline();
}

int serial_console = -1;
int stop_a_enabled = 1;
