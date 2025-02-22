/*
 * Philips UDA1341 mixer device driver
 * Copyright (c) 2002 Tomas Kasparek <tomas.kasparek@seznam.cz>
 *
 * Portions are Copyright (C) 2000 Lernout & Hauspie Speech Products, N.V.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 * History:
 *
 * 2002-03-13   Tomas Kasparek  initial release - based on uda1341.c from OSS
 * 2002-03-28   Tomas Kasparek  basic mixer is working (volume, bass, treble)
 * 2002-03-30   Tomas Kasparek  proc filesystem support, complete mixer and DSP
 *                              features support
 * 2002-04-12	Tomas Kasparek	proc interface update, code cleanup
 * 2002-05-12   Tomas Kasparek  another code cleanup
 */

/* $Id: //depot/sw/releases/Aquila_9.2.0_U11/linux/kernels/mips-linux-2.6.15_2MB/sound/i2c/l3/uda1341.c#1 $ */

#include <sound/driver.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/ioctl.h>

#include <asm/uaccess.h>

#include <sound/core.h>
#include <sound/control.h>
#include <sound/initval.h>
#include <sound/info.h>

#include <linux/l3/l3.h>

#include <sound/uda1341.h>

/* {{{ HW regs definition */

#define STAT0                   0x00
#define STAT1			0x80
#define STAT_MASK               0x80

#define DATA0_0			0x00
#define DATA0_1			0x40
#define DATA0_2			0x80
#define DATA_MASK               0xc0

#define IS_DATA0(x)     ((x) >= data0_0 && (x) <= data0_2)
#define IS_DATA1(x)     ((x) == data1)
#define IS_STATUS(x)    ((x) == stat0 || (x) == stat1)
#define IS_EXTEND(x)   ((x) >= ext0 && (x) <= ext6)

/* }}} */

enum uda1341_regs_names {
	stat0,
	stat1,
	data0_0,
	data0_1,
	data0_2,
	data1,
	ext0,
	ext1,
	ext2,
	empty,
	ext4,
	ext5,
	ext6,
	uda1341_reg_last,
};

const char *uda1341_reg_names[] = {
	"stat 0 ",
	"stat 1 ",
	"data 00",
	"data 01",
	"data 02",
	"data 1 ",
	"ext 0",
	"ext 1",
	"ext 2",
	"empty",
	"ext 4",
	"ext 5",
	"ext 6",
};

const int uda1341_enum_items[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	2, //peak - before/after
	4, //deemp - none/32/44.1/48
	0,
	4, //filter - flat/min/min/max
	0, 0, 0,
	4, //mixer - differ/line/mic/mixer
	0, 0, 0, 0, 0,
};

const char ** uda1341_enum_names[] = {
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	peak_names, //peak - before/after
	deemp_names, //deemp - none/32/44.1/48
	NULL,
	filter_names, //filter - flat/min/min/max
	NULL, NULL, NULL,
	mixer_names, //mixer - differ/line/mic/mixer
	NULL, NULL, NULL, NULL, NULL,
};

typedef int uda1341_cfg[CMD_LAST];

typedef struct uda1341 uda1341_t;

struct uda1341 {
	int (*write) (struct l3_client *uda1341, unsigned short reg, unsigned short val);
	int (*read) (struct l3_client *uda1341, unsigned short reg);        
	unsigned char regs[uda1341_reg_last];
	int active;
	spinlock_t reg_lock;
	snd_card_t *card;
	uda1341_cfg cfg;
#ifdef CONFIG_PM
	unsigned char suspend_regs[uda1341_reg_last];
	uda1341_cfg suspend_cfg;
#endif
};

//hack for ALSA magic casting
typedef struct l3_client l3_client_t;

/* transfer 8bit integer into string with binary representation */
void int2str_bin8(uint8_t val, char *buf){
	const int size = sizeof(val) * 8;
	int i;

	for (i= 0; i < size; i++){
		*(buf++) = (val >> (size - 1)) ? '1' : '0';
		val <<= 1;
	}
	*buf = '\0'; //end the string with zero
}

/* {{{ HW manipulation routines */

int snd_uda1341_codec_write(struct l3_client *clnt, unsigned short reg, unsigned short val)
{
	struct uda1341 *uda = clnt->driver_data;
	unsigned char buf[2] = { 0xc0, 0xe0 }; // for EXT addressing
	int err = 0;

	uda->regs[reg] = val;

	if (uda->active) {
		if (IS_DATA0(reg)) {
			err = l3_write(clnt, UDA1341_DATA0, (const unsigned char *)&val, 1);
		} else if (IS_DATA1(reg)) {
			err = l3_write(clnt, UDA1341_DATA1, (const unsigned char *)&val, 1);
		} else if (IS_STATUS(reg)) {
			err = l3_write(clnt, UDA1341_STATUS, (const unsigned char *)&val, 1);
		} else if (IS_EXTEND(reg)) {
			buf[0] |= (reg - ext0) & 0x7;   //EXT address
			buf[1] |= val;                  //EXT data
			err = l3_write(clnt, UDA1341_DATA0, (const unsigned char *)buf, 2);
		}
	} else
		printk(KERN_ERR "UDA1341 codec not active!\n");
	return err;
}

int snd_uda1341_codec_read(struct l3_client *clnt, unsigned short reg)
{
	unsigned char val;
	int err;

	err = l3_read(clnt, reg, &val, 1);
	if (err == 1)
		// use just 6bits - the rest is address of the reg
		return val & 63;
	return err < 0 ? err : -EIO;
}

static inline int snd_uda1341_valid_reg(struct l3_client *clnt, unsigned short reg)
{
	return reg < uda1341_reg_last;
}

int snd_uda1341_update_bits(struct l3_client *clnt, unsigned short reg, unsigned short mask,
                            unsigned short shift, unsigned short value, int flush)
{
	int change;
	unsigned short old, new;
	struct uda1341 *uda = clnt->driver_data;

#if 0
	printk(KERN_DEBUG "update_bits: reg: %s mask: %d shift: %d val: %d\n",
	       uda1341_reg_names[reg], mask, shift, value);
#endif
        
	if (!snd_uda1341_valid_reg(clnt, reg))
		return -EINVAL;
	spin_lock(&uda->reg_lock);
	old = uda->regs[reg];
	new = (old & ~(mask << shift)) | (value << shift);
	change = old != new;
	if (change) {
		if (flush) uda->write(clnt, reg, new);
		uda->regs[reg] = new;
	}
	spin_unlock(&uda->reg_lock);
	return change;
}

int snd_uda1341_cfg_write(struct l3_client *clnt, unsigned short what,
                          unsigned short value, int flush)
{
	struct uda1341 *uda = clnt->driver_data;
	int ret = 0;
#ifdef CONFIG_PM
	int reg;
#endif

#if 0
	printk(KERN_DEBUG "cfg_write what: %d value: %d\n", what, value);
#endif

	uda->cfg[what] = value;
        
	switch(what) {
	case CMD_RESET:
		ret = snd_uda1341_update_bits(clnt, data0_2, 1, 2, 1, flush);	// MUTE
		ret = snd_uda1341_update_bits(clnt, stat0, 1, 6, 1, flush);	// RESET
		ret = snd_uda1341_update_bits(clnt, stat0, 1, 6, 0, flush);	// RESTORE
		uda->cfg[CMD_RESET]=0;
		break;
	case CMD_FS:
		ret = snd_uda1341_update_bits(clnt, stat0, 3, 4, value, flush);
		break;
	case CMD_FORMAT:
		ret = snd_uda1341_update_bits(clnt, stat0, 7, 1, value, flush);
		break;
	case CMD_OGAIN:
		ret = snd_uda1341_update_bits(clnt, stat1, 1, 6, value, flush);
		break;
	case CMD_IGAIN:
		ret = snd_uda1341_update_bits(clnt, stat1, 1, 5, value, flush);
		break;
	case CMD_DAC:
		ret = snd_uda1341_update_bits(clnt, stat1, 1, 0, value, flush);
		break;
	case CMD_ADC:
		ret = snd_uda1341_update_bits(clnt, stat1, 1, 1, value, flush);
		break;
	case CMD_VOLUME:
		ret = snd_uda1341_update_bits(clnt, data0_0, 63, 0, value, flush);
		break;
	case CMD_BASS:
		ret = snd_uda1341_update_bits(clnt, data0_1, 15, 2, value, flush);
		break;
	case CMD_TREBBLE:
		ret = snd_uda1341_update_bits(clnt, data0_1, 3, 0, value, flush);
		break;
	case CMD_PEAK:
		ret = snd_uda1341_update_bits(clnt, data0_2, 1, 5, value, flush);
		break;
	case CMD_DEEMP:
		ret = snd_uda1341_update_bits(clnt, data0_2, 3, 3, value, flush);
		break;
	case CMD_MUTE:
		ret = snd_uda1341_update_bits(clnt, data0_2, 1, 2, value, flush);
		break;
	case CMD_FILTER:
		ret = snd_uda1341_update_bits(clnt, data0_2, 3, 0, value, flush);
		break;
	case CMD_CH1:
		ret = snd_uda1341_update_bits(clnt, ext0, 31, 0, value, flush);
		break;
	case CMD_CH2:
		ret = snd_uda1341_update_bits(clnt, ext1, 31, 0, value, flush);
		break;
	case CMD_MIC:
		ret = snd_uda1341_update_bits(clnt, ext2, 7, 2, value, flush);
		break;
	case CMD_MIXER:
		ret = snd_uda1341_update_bits(clnt, ext2, 3, 0, value, flush);
		break;
	case CMD_AGC:
		ret = snd_uda1341_update_bits(clnt, ext4, 1, 4, value, flush);
		break;
	case CMD_IG:
		ret = snd_uda1341_update_bits(clnt, ext4, 3, 0, value & 0x3, flush);
		ret = snd_uda1341_update_bits(clnt, ext5, 31, 0, value >> 2, flush);
		break;
	case CMD_AGC_TIME:
		ret = snd_uda1341_update_bits(clnt, ext6, 7, 2, value, flush);
		break;
	case CMD_AGC_LEVEL:
		ret = snd_uda1341_update_bits(clnt, ext6, 3, 0, value, flush);
		break;
#ifdef CONFIG_PM		
	case CMD_SUSPEND:
		for (reg = stat0; reg < uda1341_reg_last; reg++)
			uda->suspend_regs[reg] = uda->regs[reg];
		for (reg = 0; reg < CMD_LAST; reg++)
			uda->suspend_cfg[reg] = uda->cfg[reg];
		break;
	case CMD_RESUME:
		for (reg = stat0; reg < uda1341_reg_last; reg++)
			snd_uda1341_codec_write(clnt, reg, uda->suspend_regs[reg]);
		for (reg = 0; reg < CMD_LAST; reg++)
			uda->cfg[reg] = uda->suspend_cfg[reg];
		break;
#endif
	default:
		ret = -EINVAL;
		break;
	}
                
	if (!uda->active)
		printk(KERN_ERR "UDA1341 codec not active!\n");                
	return ret;
}

/* }}} */

/* {{{ Proc interface */

static void snd_uda1341_proc_read(snd_info_entry_t *entry, 
				  snd_info_buffer_t * buffer)
{
	struct l3_client *clnt = entry->private_data;
	struct uda1341 *uda = clnt->driver_data;
	int peak;

	peak = snd_uda1341_codec_read(clnt, UDA1341_DATA1);
	if (peak < 0)
		peak = 0;
	
	snd_iprintf(buffer, "%s\n\n", uda->card->longname);

	// for information about computed values see UDA1341TS datasheet pages 15 - 21
	snd_iprintf(buffer, "DAC power           : %s\n", uda->cfg[CMD_DAC] ? "on" : "off");
	snd_iprintf(buffer, "ADC power           : %s\n", uda->cfg[CMD_ADC] ? "on" : "off");
 	snd_iprintf(buffer, "Clock frequency     : %s\n", fs_names[uda->cfg[CMD_FS]]);
	snd_iprintf(buffer, "Data format         : %s\n\n", format_names[uda->cfg[CMD_FORMAT]]);

	snd_iprintf(buffer, "Filter mode         : %s\n", filter_names[uda->cfg[CMD_FILTER]]);
	snd_iprintf(buffer, "Mixer mode          : %s\n", mixer_names[uda->cfg[CMD_MIXER]]);
	snd_iprintf(buffer, "De-emphasis         : %s\n", deemp_names[uda->cfg[CMD_DEEMP]]);	
	snd_iprintf(buffer, "Peak detection pos. : %s\n", uda->cfg[CMD_PEAK] ? "after" : "before");
	snd_iprintf(buffer, "Peak value          : %s\n\n", peak_value[peak]);		
	
	snd_iprintf(buffer, "Automatic Gain Ctrl : %s\n", uda->cfg[CMD_AGC] ? "on" : "off");
	snd_iprintf(buffer, "AGC attack time     : %d ms\n", AGC_atime[uda->cfg[CMD_AGC_TIME]]);
	snd_iprintf(buffer, "AGC decay time      : %d ms\n", AGC_dtime[uda->cfg[CMD_AGC_TIME]]);
	snd_iprintf(buffer, "AGC output level    : %s dB\n\n", AGC_level[uda->cfg[CMD_AGC_LEVEL]]);

	snd_iprintf(buffer, "Mute                : %s\n", uda->cfg[CMD_MUTE] ? "on" : "off");

	if (uda->cfg[CMD_VOLUME] == 0)
		snd_iprintf(buffer, "Volume              : 0 dB\n");
	else if (uda->cfg[CMD_VOLUME] < 62)
		snd_iprintf(buffer, "Volume              : %d dB\n", -1*uda->cfg[CMD_VOLUME] +1);
	else
		snd_iprintf(buffer, "Volume              : -INF dB\n");
	snd_iprintf(buffer, "Bass                : %s\n", bass_values[uda->cfg[CMD_FILTER]][uda->cfg[CMD_BASS]]);
	snd_iprintf(buffer, "Trebble             : %d dB\n", uda->cfg[CMD_FILTER] ? 2*uda->cfg[CMD_TREBBLE] : 0);
	snd_iprintf(buffer, "Input Gain (6dB)    : %s\n", uda->cfg[CMD_IGAIN] ? "on" : "off");
	snd_iprintf(buffer, "Output Gain (6dB)   : %s\n", uda->cfg[CMD_OGAIN] ? "on" : "off");
	snd_iprintf(buffer, "Mic sensitivity     : %s\n", mic_sens_value[uda->cfg[CMD_MIC]]);

	
	if(uda->cfg[CMD_CH1] < 31)
		snd_iprintf(buffer, "Mixer gain channel 1: -%d.%c dB\n",
			    ((uda->cfg[CMD_CH1] >> 1) * 3) + (uda->cfg[CMD_CH1] & 1),
			    uda->cfg[CMD_CH1] & 1 ? '5' : '0');
	else
		snd_iprintf(buffer, "Mixer gain channel 1: -INF dB\n");
	if(uda->cfg[CMD_CH2] < 31)
		snd_iprintf(buffer, "Mixer gain channel 2: -%d.%c dB\n",
			    ((uda->cfg[CMD_CH2] >> 1) * 3) + (uda->cfg[CMD_CH2] & 1),
			    uda->cfg[CMD_CH2] & 1 ? '5' : '0');
	else
		snd_iprintf(buffer, "Mixer gain channel 2: -INF dB\n");

	if(uda->cfg[CMD_IG] > 5)
		snd_iprintf(buffer, "Input Amp. Gain ch 2: %d.%c dB\n",
			    (uda->cfg[CMD_IG] >> 1) -3, uda->cfg[CMD_IG] & 1 ? '5' : '0');
	else
		snd_iprintf(buffer, "Input Amp. Gain ch 2: %s dB\n",  ig_small_value[uda->cfg[CMD_IG]]);
}

static void snd_uda1341_proc_regs_read(snd_info_entry_t *entry, 
				       snd_info_buffer_t * buffer)
{
	struct l3_client *clnt = entry->private_data;
	struct uda1341 *uda = clnt->driver_data;		
	int reg;
	char buf[12];

	spin_lock(&uda->reg_lock);
	for (reg = 0; reg < uda1341_reg_last; reg ++) {
		if (reg == empty)
			continue;
		int2str_bin8(uda->regs[reg], buf);
		snd_iprintf(buffer, "%s = %s\n", uda1341_reg_names[reg], buf);
	}

	int2str_bin8(snd_uda1341_codec_read(clnt, UDA1341_DATA1), buf);
	snd_iprintf(buffer, "DATA1 = %s\n", buf);
	
	spin_unlock(&uda->reg_lock);       
}

static void __devinit snd_uda1341_proc_init(snd_card_t *card, struct l3_client *clnt)
{
	snd_info_entry_t *entry;

	if (! snd_card_proc_new(card, "uda1341", &entry))
		snd_info_set_text_ops(entry, clnt, 1024, snd_uda1341_proc_read);
	if (! snd_card_proc_new(card, "uda1341-regs", &entry))
		snd_info_set_text_ops(entry, clnt, 1024, snd_uda1341_proc_regs_read);
}

/* }}} */

/* {{{ Mixer controls setting */

/* {{{ UDA1341 single functions */

#define UDA1341_SINGLE(xname, where, reg, shift, mask, invert) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .info = snd_uda1341_info_single, \
  .get = snd_uda1341_get_single, .put = snd_uda1341_put_single, \
  .private_value = where | (reg << 5) | (shift << 9) | (mask << 12) | (invert << 18) \
}

static int snd_uda1341_info_single(snd_kcontrol_t *kcontrol, snd_ctl_elem_info_t * uinfo)
{
	int mask = (kcontrol->private_value >> 12) & 63;

	uinfo->type = mask == 1 ? SNDRV_CTL_ELEM_TYPE_BOOLEAN : SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = mask;
	return 0;
}

static int snd_uda1341_get_single(snd_kcontrol_t * kcontrol, snd_ctl_elem_value_t * ucontrol)
{
	struct l3_client *clnt = snd_kcontrol_chip(kcontrol);
	uda1341_t *uda = clnt->driver_data;
	int where = kcontrol->private_value & 31;        
	int mask = (kcontrol->private_value >> 12) & 63;
	int invert = (kcontrol->private_value >> 18) & 1;
        
	ucontrol->value.integer.value[0] = uda->cfg[where];
	if (invert)
		ucontrol->value.integer.value[0] = mask - ucontrol->value.integer.value[0];

	return 0;
}

static int snd_uda1341_put_single(snd_kcontrol_t * kcontrol, snd_ctl_elem_value_t * ucontrol)
{
	struct l3_client *clnt = snd_kcontrol_chip(kcontrol);
	uda1341_t *uda = clnt->driver_data;
	int where = kcontrol->private_value & 31;        
	int reg = (kcontrol->private_value >> 5) & 15;
	int shift = (kcontrol->private_value >> 9) & 7;
	int mask = (kcontrol->private_value >> 12) & 63;
	int invert = (kcontrol->private_value >> 18) & 1;
	unsigned short val;

	val = (ucontrol->value.integer.value[0] & mask);
	if (invert)
		val = mask - val;

	uda->cfg[where] = val;
	return snd_uda1341_update_bits(clnt, reg, mask, shift, val, FLUSH);
}

/* }}} */

/* {{{ UDA1341 enum functions */

#define UDA1341_ENUM(xname, where, reg, shift, mask, invert) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .info = snd_uda1341_info_enum, \
  .get = snd_uda1341_get_enum, .put = snd_uda1341_put_enum, \
  .private_value = where | (reg << 5) | (shift << 9) | (mask << 12) | (invert << 18) \
}

static int snd_uda1341_info_enum(snd_kcontrol_t *kcontrol, snd_ctl_elem_info_t * uinfo)
{
	int where = kcontrol->private_value & 31;
	const char **texts;
	
	// this register we don't handle this way
	if (!uda1341_enum_items[where])
		return -EINVAL;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = uda1341_enum_items[where];

	if (uinfo->value.enumerated.item >= uda1341_enum_items[where])
		uinfo->value.enumerated.item = uda1341_enum_items[where] - 1;

	texts = uda1341_enum_names[where];
	strcpy(uinfo->value.enumerated.name, texts[uinfo->value.enumerated.item]);
	return 0;
}

static int snd_uda1341_get_enum(snd_kcontrol_t * kcontrol, snd_ctl_elem_value_t * ucontrol)
{
	struct l3_client *clnt = snd_kcontrol_chip(kcontrol);
	uda1341_t *uda = clnt->driver_data;
	int where = kcontrol->private_value & 31;        
        
	ucontrol->value.enumerated.item[0] = uda->cfg[where];	
	return 0;
}

static int snd_uda1341_put_enum(snd_kcontrol_t * kcontrol, snd_ctl_elem_value_t * ucontrol)
{
	struct l3_client *clnt = snd_kcontrol_chip(kcontrol);
	uda1341_t *uda = clnt->driver_data;
	int where = kcontrol->private_value & 31;        
	int reg = (kcontrol->private_value >> 5) & 15;
	int shift = (kcontrol->private_value >> 9) & 7;
	int mask = (kcontrol->private_value >> 12) & 63;

	uda->cfg[where] = (ucontrol->value.enumerated.item[0] & mask);
	
	return snd_uda1341_update_bits(clnt, reg, mask, shift, uda->cfg[where], FLUSH);
}

/* }}} */

/* {{{ UDA1341 2regs functions */

#define UDA1341_2REGS(xname, where, reg_1, reg_2, shift_1, shift_2, mask_1, mask_2, invert) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = (xname), .info = snd_uda1341_info_2regs, \
  .get = snd_uda1341_get_2regs, .put = snd_uda1341_put_2regs, \
  .private_value = where | (reg_1 << 5) | (reg_2 << 9) | (shift_1 << 13) | (shift_2 << 16) | \
                         (mask_1 << 19) | (mask_2 << 25) | (invert << 31) \
}


static int snd_uda1341_info_2regs(snd_kcontrol_t *kcontrol, snd_ctl_elem_info_t * uinfo)
{
	int mask_1 = (kcontrol->private_value >> 19) & 63;
	int mask_2 = (kcontrol->private_value >> 25) & 63;
	int mask;
        
	mask = (mask_2 + 1) * (mask_1 + 1) - 1;
	uinfo->type = mask == 1 ? SNDRV_CTL_ELEM_TYPE_BOOLEAN : SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = mask;
	return 0;
}

static int snd_uda1341_get_2regs(snd_kcontrol_t * kcontrol, snd_ctl_elem_value_t * ucontrol)
{
	struct l3_client *clnt = snd_kcontrol_chip(kcontrol);
	uda1341_t *uda = clnt->driver_data;
	int where = kcontrol->private_value & 31;
	int mask_1 = (kcontrol->private_value >> 19) & 63;
	int mask_2 = (kcontrol->private_value >> 25) & 63;        
	int invert = (kcontrol->private_value >> 31) & 1;
	int mask;

	mask = (mask_2 + 1) * (mask_1 + 1) - 1;

	ucontrol->value.integer.value[0] = uda->cfg[where];
	if (invert)
		ucontrol->value.integer.value[0] = mask - ucontrol->value.integer.value[0];
	return 0;
}

static int snd_uda1341_put_2regs(snd_kcontrol_t * kcontrol, snd_ctl_elem_value_t * ucontrol)
{
	struct l3_client *clnt = snd_kcontrol_chip(kcontrol);
	uda1341_t *uda = clnt->driver_data;        
	int where = kcontrol->private_value & 31;        
	int reg_1 = (kcontrol->private_value >> 5) & 15;
	int reg_2 = (kcontrol->private_value >> 9) & 15;        
	int shift_1 = (kcontrol->private_value >> 13) & 7;
	int shift_2 = (kcontrol->private_value >> 16) & 7;
	int mask_1 = (kcontrol->private_value >> 19) & 63;
	int mask_2 = (kcontrol->private_value >> 25) & 63;        
	int invert = (kcontrol->private_value >> 31) & 1;
	int mask;
	unsigned short val1, val2, val;

	val = ucontrol->value.integer.value[0];
         
	mask = (mask_2 + 1) * (mask_1 + 1) - 1;

	val1 = val & mask_1;
	val2 = (val / (mask_1 + 1)) & mask_2;        

	if (invert) {
		val1 = mask_1 - val1;
		val2 = mask_2 - val2;
	}

	uda->cfg[where] = invert ? mask - val : val;
        
	//FIXME - return value
	snd_uda1341_update_bits(clnt, reg_1, mask_1, shift_1, val1, FLUSH);
	return snd_uda1341_update_bits(clnt, reg_2, mask_2, shift_2, val2, FLUSH);
}

/* }}} */
  
static snd_kcontrol_new_t snd_uda1341_controls[] = {
	UDA1341_SINGLE("Master Playback Switch", CMD_MUTE, data0_2, 2, 1, 1),
	UDA1341_SINGLE("Master Playback Volume", CMD_VOLUME, data0_0, 0, 63, 1),

	UDA1341_SINGLE("Bass Playback Volume", CMD_BASS, data0_1, 2, 15, 0),
	UDA1341_SINGLE("Treble Playback Volume", CMD_TREBBLE, data0_1, 0, 3, 0),

	UDA1341_SINGLE("Input Gain Switch", CMD_IGAIN, stat1, 5, 1, 0),
	UDA1341_SINGLE("Output Gain Switch", CMD_OGAIN, stat1, 6, 1, 0),

	UDA1341_SINGLE("Mixer Gain Channel 1 Volume", CMD_CH1, ext0, 0, 31, 1),
	UDA1341_SINGLE("Mixer Gain Channel 2 Volume", CMD_CH2, ext1, 0, 31, 1),

	UDA1341_SINGLE("Mic Sensitivity Volume", CMD_MIC, ext2, 2, 7, 0),

	UDA1341_SINGLE("AGC Output Level", CMD_AGC_LEVEL, ext6, 0, 3, 0),
	UDA1341_SINGLE("AGC Time Constant", CMD_AGC_TIME, ext6, 2, 7, 0),
	UDA1341_SINGLE("AGC Time Constant Switch", CMD_AGC, ext4, 4, 1, 0),

	UDA1341_SINGLE("DAC Power", CMD_DAC, stat1, 0, 1, 0),
	UDA1341_SINGLE("ADC Power", CMD_ADC, stat1, 1, 1, 0),

	UDA1341_ENUM("Peak detection", CMD_PEAK, data0_2, 5, 1, 0),
	UDA1341_ENUM("De-emphasis", CMD_DEEMP, data0_2, 3, 3, 0),
	UDA1341_ENUM("Mixer mode", CMD_MIXER, ext2, 0, 3, 0),
	UDA1341_ENUM("Filter mode", CMD_FILTER, data0_2, 0, 3, 0),

	UDA1341_2REGS("Gain Input Amplifier Gain (channel 2)", CMD_IG, ext4, ext5, 0, 0, 3, 31, 0),
};

static void uda1341_free(struct l3_client *uda1341)
{
	l3_detach_client(uda1341); // calls kfree for driver_data (uda1341_t)
	kfree(uda1341);
}

static int uda1341_dev_free(snd_device_t *device)
{
	struct l3_client *clnt = device->device_data;
	uda1341_free(clnt);
	return 0;
}

int __init snd_chip_uda1341_mixer_new(snd_card_t *card, struct l3_client **clnt)
{
	static snd_device_ops_t ops = {
		.dev_free =     uda1341_dev_free,
	};
	struct l3_client *uda1341;
	int idx, err;

	snd_assert(card != NULL, return -EINVAL);

	uda1341 = kzalloc(sizeof(*uda1341), GFP_KERNEL);
	if (uda1341 == NULL)
		return -ENOMEM;
         
	if ((err = l3_attach_client(uda1341, "l3-bit-sa1100-gpio", "snd-uda1341"))) {
		kfree(uda1341);
		return err;
	}

	if ((err = snd_device_new(card, SNDRV_DEV_CODEC, uda1341, &ops)) < 0) {
		l3_detach_client(uda1341);
		kfree(uda1341);
		return err;
	}

	for (idx = 0; idx < ARRAY_SIZE(snd_uda1341_controls); idx++) {
		if ((err = snd_ctl_add(card, snd_ctl_new1(&snd_uda1341_controls[idx], uda1341))) < 0)
			return err;
	}

	*clnt = uda1341;
	strcpy(card->mixername, "UDA1341TS Mixer");
	((uda1341_t *)uda1341->driver_data)->card = card;
        
	snd_uda1341_proc_init(card, uda1341);
        
	return 0;
}

/* }}} */

/* {{{ L3 operations */

static int uda1341_attach(struct l3_client *clnt)
{
	struct uda1341 *uda;

	uda = kzalloc(sizeof(*uda), 0, GFP_KERNEL);
	if (!uda)
		return -ENOMEM;

	/* init fixed parts of my copy of registers */
	uda->regs[stat0]   = STAT0;
	uda->regs[stat1]   = STAT1;

	uda->regs[data0_0] = DATA0_0;
	uda->regs[data0_1] = DATA0_1;
	uda->regs[data0_2] = DATA0_2;

	uda->write = snd_uda1341_codec_write;
	uda->read = snd_uda1341_codec_read;
  
	spin_lock_init(&uda->reg_lock);
        
	clnt->driver_data = uda;
	return 0;
}

static void uda1341_detach(struct l3_client *clnt)
{
	kfree(clnt->driver_data);
}

static int
uda1341_command(struct l3_client *clnt, int cmd, void *arg)
{
	if (cmd != CMD_READ_REG)
		return snd_uda1341_cfg_write(clnt, cmd, (int) arg, FLUSH);

	return snd_uda1341_codec_read(clnt, (int) arg);
}

static int uda1341_open(struct l3_client *clnt)
{
	struct uda1341 *uda = clnt->driver_data;

	uda->active = 1;

	/* init default configuration */
	snd_uda1341_cfg_write(clnt, CMD_RESET, 0, REGS_ONLY);
	snd_uda1341_cfg_write(clnt, CMD_FS, F256, FLUSH);       // unknown state after reset
	snd_uda1341_cfg_write(clnt, CMD_FORMAT, LSB16, FLUSH);  // unknown state after reset
	snd_uda1341_cfg_write(clnt, CMD_OGAIN, ON, FLUSH);      // default off after reset
	snd_uda1341_cfg_write(clnt, CMD_IGAIN, ON, FLUSH);      // default off after reset
	snd_uda1341_cfg_write(clnt, CMD_DAC, ON, FLUSH);	// ??? default value after reset
	snd_uda1341_cfg_write(clnt, CMD_ADC, ON, FLUSH);	// ??? default value after reset
	snd_uda1341_cfg_write(clnt, CMD_VOLUME, 20, FLUSH);     // default 0dB after reset
	snd_uda1341_cfg_write(clnt, CMD_BASS, 0, REGS_ONLY);    // default value after reset
	snd_uda1341_cfg_write(clnt, CMD_TREBBLE, 0, REGS_ONLY); // default value after reset
	snd_uda1341_cfg_write(clnt, CMD_PEAK, AFTER, REGS_ONLY);// default value after reset
	snd_uda1341_cfg_write(clnt, CMD_DEEMP, NONE, REGS_ONLY);// default value after reset
	//at this moment should be QMUTED by h3600_audio_init
	snd_uda1341_cfg_write(clnt, CMD_MUTE, OFF, REGS_ONLY);  // default value after reset
	snd_uda1341_cfg_write(clnt, CMD_FILTER, MAX, FLUSH);    // defaul flat after reset
	snd_uda1341_cfg_write(clnt, CMD_CH1, 31, FLUSH);        // default value after reset
	snd_uda1341_cfg_write(clnt, CMD_CH2, 4, FLUSH);         // default value after reset
	snd_uda1341_cfg_write(clnt, CMD_MIC, 4, FLUSH);         // default 0dB after reset
	snd_uda1341_cfg_write(clnt, CMD_MIXER, MIXER, FLUSH);   // default doub.dif.mode          
	snd_uda1341_cfg_write(clnt, CMD_AGC, OFF, FLUSH);       // default value after reset
	snd_uda1341_cfg_write(clnt, CMD_IG, 0, FLUSH);          // unknown state after reset
	snd_uda1341_cfg_write(clnt, CMD_AGC_TIME, 0, FLUSH);    // default value after reset
	snd_uda1341_cfg_write(clnt, CMD_AGC_LEVEL, 0, FLUSH);   // default value after reset

	return 0;
}

static void uda1341_close(struct l3_client *clnt)
{
	struct uda1341 *uda = clnt->driver_data;

	uda->active = 0;
}

/* }}} */

/* {{{ Module and L3 initialization */

static struct l3_ops uda1341_ops = {
	.open =		uda1341_open,
	.command =	uda1341_command,
	.close =	uda1341_close,
};

static struct l3_driver uda1341_driver = {
	.name =		UDA1341_ALSA_NAME,
	.attach_client = uda1341_attach,
	.detach_client = uda1341_detach,
	.ops =		&uda1341_ops,
	.owner =	THIS_MODULE,
};

static int __init uda1341_init(void)
{
	return l3_add_driver(&uda1341_driver);
}

static void __exit uda1341_exit(void)
{
	l3_del_driver(&uda1341_driver);
}

module_init(uda1341_init);
module_exit(uda1341_exit);

MODULE_AUTHOR("Tomas Kasparek <tomas.kasparek@seznam.cz>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Philips UDA1341 CODEC driver for ALSA");
MODULE_SUPPORTED_DEVICE("{{UDA1341,UDA1341TS}}");

EXPORT_SYMBOL(snd_chip_uda1341_mixer_new);

/* }}} */

/*
 * Local variables:
 * indent-tabs-mode: t
 * End:
 */
