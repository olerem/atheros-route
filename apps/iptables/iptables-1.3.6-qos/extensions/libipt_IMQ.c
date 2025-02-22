/* Shared library add-on to iptables to add IMQ target support. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include "../../../../linux/net/netfilter_ipv4/ipt_IMQ.h"

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"IMQ target v%s options:\n"
"  --todev <N>		enqueue to imq<N>, defaults to 0\n", 
IPTABLES_VERSION);
}

static struct option opts[] = {
	{ "todev", 1, 0, '1' },
	{ 0 }
};

/* Initialize the target. */
static void
init(struct ipt_entry_target *t, unsigned int *nfcache)
{
	struct ipt_imq_info *mr = (struct ipt_imq_info*)t->data;

	mr->todev = 0;
	*nfcache |= NFC_UNKNOWN;
}

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      struct ipt_entry_target **target)
{
	struct ipt_imq_info *mr = (struct ipt_imq_info*)(*target)->data;
	
	switch(c) {
	case '1':
		if (check_inverse(optarg, &invert, NULL, 0))
			exit_error(PARAMETER_PROBLEM,
				   "Unexpected `!' after --todev");
		mr->todev=atoi(optarg);
		break;
	default:
		return 0;
	}
	return 1;
}

static void
final_check(unsigned int flags)
{
}

/* Prints out the targinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_target *target,
      int numeric)
{
	struct ipt_imq_info *mr = (struct ipt_imq_info*)target->data;

	printf("IMQ: todev %u ", mr->todev);
}

/* Saves the union ipt_targinfo in parsable form to stdout. */
static void
save(const struct ipt_ip *ip, const struct ipt_entry_target *target)
{
	struct ipt_imq_info *mr = (struct ipt_imq_info*)target->data;

	printf("--todev %u", mr->todev);
}

static struct iptables_target imq = {
	.next		= NULL,
	.name		= "IMQ",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_imq_info)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_imq_info)),
	.help		= &help,
	.init		= &init,
	.parse		= &parse,
	.final_check	= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts
};

/* was (Atheros): static __attribute__((constructor)) */ void _init(void)
{
	register_target(&imq);
}
