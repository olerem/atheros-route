/* $Id: //depot/sw/releases/Aquila_9.2.0_U11/linux/kernels/mips-linux-2.6.15/arch/sparc/prom/tree.c#1 $
 * tree.c: Basic device tree traversal/scanning for the Linux
 *         prom library.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 */

#define PROMLIB_INTERNAL

#include <linux/string.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/ctype.h>

#include <asm/openprom.h>
#include <asm/oplib.h>

extern void restore_current(void);

static char promlib_buf[128];

/* Internal version of prom_getchild that does not alter return values. */
int __prom_getchild(int node)
{
	unsigned long flags;
	int cnode;

	spin_lock_irqsave(&prom_lock, flags);
	cnode = prom_nodeops->no_child(node);
	restore_current();
	spin_unlock_irqrestore(&prom_lock, flags);

	return cnode;
}

/* Return the child of node 'node' or zero if no this node has no
 * direct descendent.
 */
int prom_getchild(int node)
{
	int cnode;

	if (node == -1)
		return 0;

	cnode = __prom_getchild(node);
	if (cnode == 0 || cnode == -1)
		return 0;

	return cnode;
}

/* Internal version of prom_getsibling that does not alter return values. */
int __prom_getsibling(int node)
{
	unsigned long flags;
	int cnode;

	spin_lock_irqsave(&prom_lock, flags);
	cnode = prom_nodeops->no_nextnode(node);
	restore_current();
	spin_unlock_irqrestore(&prom_lock, flags);

	return cnode;
}

/* Return the next sibling of node 'node' or zero if no more siblings
 * at this level of depth in the tree.
 */
int prom_getsibling(int node)
{
	int sibnode;

	if (node == -1)
		return 0;

	sibnode = __prom_getsibling(node);
	if (sibnode == 0 || sibnode == -1)
		return 0;

	return sibnode;
}

/* Return the length in bytes of property 'prop' at node 'node'.
 * Return -1 on error.
 */
int prom_getproplen(int node, char *prop)
{
	int ret;
	unsigned long flags;

	if((!node) || (!prop))
		return -1;
		
	spin_lock_irqsave(&prom_lock, flags);
	ret = prom_nodeops->no_proplen(node, prop);
	restore_current();
	spin_unlock_irqrestore(&prom_lock, flags);
	return ret;
}

/* Acquire a property 'prop' at node 'node' and place it in
 * 'buffer' which has a size of 'bufsize'.  If the acquisition
 * was successful the length will be returned, else -1 is returned.
 */
int prom_getproperty(int node, char *prop, char *buffer, int bufsize)
{
	int plen, ret;
	unsigned long flags;

	plen = prom_getproplen(node, prop);
	if((plen > bufsize) || (plen == 0) || (plen == -1))
		return -1;
	/* Ok, things seem all right. */
	spin_lock_irqsave(&prom_lock, flags);
	ret = prom_nodeops->no_getprop(node, prop, buffer);
	restore_current();
	spin_unlock_irqrestore(&prom_lock, flags);
	return ret;
}

/* Acquire an integer property and return its value.  Returns -1
 * on failure.
 */
int prom_getint(int node, char *prop)
{
	static int intprop;

	if(prom_getproperty(node, prop, (char *) &intprop, sizeof(int)) != -1)
		return intprop;

	return -1;
}

/* Acquire an integer property, upon error return the passed default
 * integer.
 */
int prom_getintdefault(int node, char *property, int deflt)
{
	int retval;

	retval = prom_getint(node, property);
	if(retval == -1) return deflt;

	return retval;
}

/* Acquire a boolean property, 1=TRUE 0=FALSE. */
int prom_getbool(int node, char *prop)
{
	int retval;

	retval = prom_getproplen(node, prop);
	if(retval == -1) return 0;
	return 1;
}

/* Acquire a property whose value is a string, returns a null
 * string on error.  The char pointer is the user supplied string
 * buffer.
 */
void prom_getstring(int node, char *prop, char *user_buf, int ubuf_size)
{
	int len;

	len = prom_getproperty(node, prop, user_buf, ubuf_size);
	if(len != -1) return;
	user_buf[0] = 0;
	return;
}


/* Does the device at node 'node' have name 'name'?
 * YES = 1   NO = 0
 */
int prom_nodematch(int node, char *name)
{
	int error;

	static char namebuf[128];
	error = prom_getproperty(node, "name", namebuf, sizeof(namebuf));
	if (error == -1) return 0;
	if(strcmp(namebuf, name) == 0) return 1;
	return 0;
}

/* Search siblings at 'node_start' for a node with name
 * 'nodename'.  Return node if successful, zero if not.
 */
int prom_searchsiblings(int node_start, char *nodename)
{

	int thisnode, error;

	for(thisnode = node_start; thisnode;
	    thisnode=prom_getsibling(thisnode)) {
		error = prom_getproperty(thisnode, "name", promlib_buf,
					 sizeof(promlib_buf));
		/* Should this ever happen? */
		if(error == -1) continue;
		if(strcmp(nodename, promlib_buf)==0) return thisnode;
	}

	return 0;
}

/* Gets name in the form prom v2+ uses it (name@x,yyyyy or name (if no reg)) */
int prom_getname (int node, char *buffer, int len)
{
	int i;
	struct linux_prom_registers reg[PROMREG_MAX];
	
	i = prom_getproperty (node, "name", buffer, len);
	if (i <= 0) return -1;
	buffer [i] = 0;
	len -= i;
	i = prom_getproperty (node, "reg", (char *)reg, sizeof (reg));
	if (i <= 0) return 0;
	if (len < 11) return -1;
	buffer = strchr (buffer, 0);
	sprintf (buffer, "@%x,%x", reg[0].which_io, (uint)reg[0].phys_addr);
	return 0;
}

/* Interal version of nextprop that does not alter return values. */
char * __prom_nextprop(int node, char * oprop)
{
	unsigned long flags;
	char *prop;

	spin_lock_irqsave(&prom_lock, flags);
	prop = prom_nodeops->no_nextprop(node, oprop);
	restore_current();
	spin_unlock_irqrestore(&prom_lock, flags);

	return prop;
}

/* Return the first property name for node 'node'. */
/* buffer is unused argument, but as v9 uses it, we need to have the same interface */
char * prom_firstprop(int node, char *bufer)
{
	if (node == 0 || node == -1)
		return "";

	return __prom_nextprop(node, "");
}

/* Return the property type string after property type 'oprop'
 * at node 'node' .  Returns empty string if no more
 * property types for this node.
 */
char * prom_nextprop(int node, char *oprop, char *buffer)
{
	if (node == 0 || node == -1)
		return "";

	return __prom_nextprop(node, oprop);
}

int prom_finddevice(char *name)
{
	char nbuf[128];
	char *s = name, *d;
	int node = prom_root_node, node2;
	unsigned int which_io, phys_addr;
	struct linux_prom_registers reg[PROMREG_MAX];

	while (*s++) {
		if (!*s) return node; /* path '.../' is legal */
		node = prom_getchild(node);

		for (d = nbuf; *s != 0 && *s != '@' && *s != '/';)
			*d++ = *s++;
		*d = 0;
		
		node = prom_searchsiblings(node, nbuf);
		if (!node)
			return 0;

		if (*s == '@') {
			if (isxdigit(s[1]) && s[2] == ',') {
				which_io = simple_strtoul(s+1, NULL, 16);
				phys_addr = simple_strtoul(s+3, &d, 16);
				if (d != s + 3 && (!*d || *d == '/')
				    && d <= s + 3 + 8) {
					node2 = node;
					while (node2 && node2 != -1) {
						if (prom_getproperty (node2, "reg", (char *)reg, sizeof (reg)) > 0) {
							if (which_io == reg[0].which_io && phys_addr == reg[0].phys_addr) {
								node = node2;
								break;
							}
						}
						node2 = prom_getsibling(node2);
						if (!node2 || node2 == -1)
							break;
						node2 = prom_searchsiblings(prom_getsibling(node2), nbuf);
					}
				}
			}
			while (*s != 0 && *s != '/') s++;
		}
	}
	return node;
}

int prom_node_has_property(int node, char *prop)
{
	char *current_property = "";

	do {
		current_property = prom_nextprop(node, current_property, NULL);
		if(!strcmp(current_property, prop))
		   return 1;
	} while (*current_property);
	return 0;
}

/* Set property 'pname' at node 'node' to value 'value' which has a length
 * of 'size' bytes.  Return the number of bytes the prom accepted.
 */
int prom_setprop(int node, char *pname, char *value, int size)
{
	unsigned long flags;
	int ret;

	if(size == 0) return 0;
	if((pname == 0) || (value == 0)) return 0;
	spin_lock_irqsave(&prom_lock, flags);
	ret = prom_nodeops->no_setprop(node, pname, value, size);
	restore_current();
	spin_unlock_irqrestore(&prom_lock, flags);
	return ret;
}

int prom_inst2pkg(int inst)
{
	int node;
	unsigned long flags;
	
	spin_lock_irqsave(&prom_lock, flags);
	node = (*romvec->pv_v2devops.v2_inst2pkg)(inst);
	restore_current();
	spin_unlock_irqrestore(&prom_lock, flags);
	if (node == -1) return 0;
	return node;
}

/* Return 'node' assigned to a particular prom 'path'
 * FIXME: Should work for v0 as well
 */
int prom_pathtoinode(char *path)
{
	int node, inst;
	
	inst = prom_devopen (path);
	if (inst == -1) return 0;
	node = prom_inst2pkg (inst);
	prom_devclose (inst);
	if (node == -1) return 0;
	return node;
}
