/*
 * files.c -- DHCP server file manipulation *
 * Rewrite by Russ Dill <Russ.Dill@asu.edu> July 2001
 */

#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <netdb.h>

#include <netinet/ether.h>
#include "static_leases.h"

#include "dhcpd.h"
#include "files.h"
#include "options.h"
#include "common.h"

/*
 * Domain names may have 254 chars, and string options can be 254
 * chars long. However, 80 bytes will be enough for most, and won't
 * hog up memory. If you have a special application, change it
 */
#define READ_CONFIG_BUF_SIZE 80

#define ALL_ACCESS  "/var/run/.allAccess"
#define UDHCPD_FILE  "/var/run/udhcpd.leases"

/* on these functions, make sure you datatype matches */
static int read_ip(const char *line, void *arg)
{
	struct in_addr *addr = arg;
	struct hostent *host;
	int retval = 1;

	if (!inet_aton(line, addr)) {
		if ((host = gethostbyname(line)))
			addr->s_addr = *((unsigned long *) host->h_addr_list[0]);
		else retval = 0;
	}
	return retval;
}

static int read_mac(const char *line, void *arg)
{
	uint8_t *mac_bytes = arg;
	struct ether_addr *temp_ether_addr;
	int retval = 1;

	temp_ether_addr = ether_aton(line);

	if(temp_ether_addr == NULL)
		retval = 0;
	else
		memcpy(mac_bytes, temp_ether_addr, 6);

	return retval;
}


static int read_str(const char *line, void *arg)
{
	char **dest = arg;

	if (*dest) free(*dest);
	*dest = strdup(line);

	return 1;
}


static int read_u32(const char *line, void *arg)
{
	uint32_t *dest = arg;
	char *endptr;
	*dest = strtoul(line, &endptr, 0);
	return endptr[0] == '\0';
}


static int read_yn(const char *line, void *arg)
{
	char *dest = arg;
	int retval = 1;

	if (!strcasecmp("yes", line))
		*dest = 1;
	else if (!strcasecmp("no", line))
		*dest = 0;
	else retval = 0;

	return retval;
}


/* read a dhcp option and add it to opt_list */
static int read_opt(const char *const_line, void *arg)
{
	struct option_set **opt_list = arg;
	char *opt, *val, *endptr;
	struct dhcp_option *option;
	int retval = 0, length;
	char buffer[8];
	char *line;
	uint16_t *result_u16 = (uint16_t *) buffer;
	uint32_t *result_u32 = (uint32_t *) buffer;

	/* Cheat, the only const line we'll actually get is "" */
	line = (char *) const_line;
	if (!(opt = strtok(line, " \t="))) return 0;

	for (option = dhcp_options; option->code; option++)
		if (!strcasecmp(option->name, opt))
			break;

	if (!option->code) return 0;

	do {
		if (!(val = strtok(NULL, ", \t"))) break;
		length = option_lengths[option->flags & TYPE_MASK];
		retval = 0;
		opt = buffer; /* new meaning for variable opt */
		switch (option->flags & TYPE_MASK) {
		case OPTION_IP:
			retval = read_ip(val, buffer);
			break;
		case OPTION_IP_PAIR:
			retval = read_ip(val, buffer);
			if (!(val = strtok(NULL, ", \t/-"))) retval = 0;
			if (retval) retval = read_ip(val, buffer + 4);
			break;
		case OPTION_STRING:
			length = strlen(val);
			if (length > 0) {
				if (length > 254) length = 254;
				opt = val;
				retval = 1;
			}
			break;
		case OPTION_BOOLEAN:
			retval = read_yn(val, buffer);
			break;
		case OPTION_U8:
			buffer[0] = strtoul(val, &endptr, 0);
			retval = (endptr[0] == '\0');
			break;
		case OPTION_U16:
			*result_u16 = htons(strtoul(val, &endptr, 0));
			retval = (endptr[0] == '\0');
			break;
		case OPTION_S16:
			*result_u16 = htons(strtol(val, &endptr, 0));
			retval = (endptr[0] == '\0');
			break;
		case OPTION_U32:
			*result_u32 = htonl(strtoul(val, &endptr, 0));
			retval = (endptr[0] == '\0');
			break;
		case OPTION_S32:
			*result_u32 = htonl(strtol(val, &endptr, 0));
			retval = (endptr[0] == '\0');
			break;
		default:
			break;
		}
		if (retval)
			attach_option(opt_list, option, opt, length);
	} while (retval && option->flags & OPTION_LIST);
	return retval;
}

static int read_staticlease(const char *const_line, void *arg)
{

	char *line;
	char *mac_string;
	char *ip_string;
	uint8_t *mac_bytes;
	uint32_t *ip;


	/* Allocate memory for addresses */
	mac_bytes = xmalloc(sizeof(unsigned char) * 8);
	ip = xmalloc(sizeof(uint32_t));

	/* Read mac */
	line = (char *) const_line;
	mac_string = strtok(line, " \t");
	read_mac(mac_string, mac_bytes);

	/* Read ip */
	ip_string = strtok(NULL, " \t");
	read_ip(ip_string, ip);

	addStaticLease(arg, mac_bytes, ip);

#ifdef UDHCP_DEBUG
	printStaticLeases(arg);
#endif

	return 1;

}


static const struct config_keyword keywords[] = {
	/* keyword	handler   variable address		default */
	{"start",	read_ip,  &(server_config.start),	"192.168.0.20"},
	{"end",		read_ip,  &(server_config.end),		"192.168.0.254"},
	{"interface",	read_str, &(server_config.interface),	"eth0"},
	{"option",	read_opt, &(server_config.options),	""},
	{"opt",		read_opt, &(server_config.options),	""},
	{"max_leases",	read_u32, &(server_config.max_leases),	"254"},
	{"remaining",	read_yn,  &(server_config.remaining),	"yes"},
	{"auto_time",	read_u32, &(server_config.auto_time),	"7200"},
	{"decline_time",read_u32, &(server_config.decline_time),"3600"},
	{"conflict_time",read_u32,&(server_config.conflict_time),"3600"},
	{"offer_time",	read_u32, &(server_config.offer_time),	"60"},
	{"min_lease",	read_u32, &(server_config.min_lease),	"60"},
	{"lease_file",	read_str, &(server_config.lease_file),	LEASES_FILE},
	{"pidfile",	read_str, &(server_config.pidfile),	"/var/run/udhcpd.pid"},
	{"notify_file", read_str, &(server_config.notify_file),	""},
	{"siaddr",	read_ip,  &(server_config.siaddr),	"0.0.0.0"},
	{"sname",	read_str, &(server_config.sname),	""},
	{"boot_file",	read_str, &(server_config.boot_file),	""},
	{"static_lease",read_staticlease, &(server_config.static_leases),	""},
	/*ADDME: static lease */
	{"",		NULL, 	  NULL,				""}
};


int deal_offline_sta(uint8_t *hostname, uint8_t *chaddr, uint32_t yiaddr)
{
	#if 0
	uint32_t addr, ret;
	struct in_addr addr2;
	//struct dhcpOfferedAddr *lease = NULL;
	int i, j, k;
	char mac_buf[20];

	for(j = 0, k = 0 ; j < 6; j++, k+=3)
	{
        sprintf(&mac_buf[k], "%02x:", chaddr[j]);
	}

	addr2.s_addr = yiaddr;
	LOG(LOG_INFO, "[deal_offline_sta] hostname is %s ip is %s", hostname, inet_ntoa(addr2));
	LOG(LOG_INFO, "[deal_offline_sta] the mac is %s", mac_buf);

	addr = ntohl(yiaddr);
	/* ie, 192.168.55.0 */
	if (!(addr & 0xFF)) 
		return 1;
	/* ie, 192.168.55.255 */
	if ((addr & 0xFF) == 0xFF) 
		return 1;

	ret = htonl(addr);
	#endif

	uint32_t ret;
	struct in_addr addr;
	addr.s_addr = yiaddr;
	LOG(LOG_INFO, "[deal_offline_sta] hostname is %s ip is %s", hostname, inet_ntoa(addr));
	
	//ret = yiaddr;
	if (/*it isn't on the network */
		(arpping(yiaddr, server_config.server, server_config.arp, server_config.interface) == 0) //&&
		/*it expired and we are checking for expired leases*/
		/*(!lease_expired(leases))*/ )
	{
		return 0;
	}
	return 1;

}

int read_config(const char *file)
{
	FILE *in;
	char buffer[READ_CONFIG_BUF_SIZE], *token, *line;
#ifdef UDHCP_DEBUG
	char orig[READ_CONFIG_BUF_SIZE];
#endif
	int i, lm = 0;

	for (i = 0; keywords[i].keyword[0]; i++)
		if (keywords[i].def[0])
			keywords[i].handler(keywords[i].def, keywords[i].var);

	if (!(in = fopen(file, "r"))) {
		LOG(LOG_ERR, "unable to open config file: %s", file);
		return 0;
	}

	while (fgets(buffer, READ_CONFIG_BUF_SIZE, in)) {
		lm++;
		if (strchr(buffer, '\n')) *(strchr(buffer, '\n')) = '\0';
#ifdef UDHCP_DEBUG
		strcpy(orig, buffer);
#endif
		if (strchr(buffer, '#')) *(strchr(buffer, '#')) = '\0';

		if (!(token = strtok(buffer, " \t"))) continue;
		if (!(line = strtok(NULL, ""))) continue;

		/* eat leading whitespace */
		line = line + strspn(line, " \t=");
		/* eat trailing whitespace */
		for (i = strlen(line); i > 0 && isspace(line[i - 1]); i--);
		line[i] = '\0';

		for (i = 0; keywords[i].keyword[0]; i++)
			if (!strcasecmp(token, keywords[i].keyword))
				if (!keywords[i].handler(line, keywords[i].var)) {
					LOG(LOG_ERR, "Failure parsing line %d of %s", lm, file);
					DEBUG(LOG_ERR, "unable to parse '%s'", orig);
					/* reset back to the default value */
					keywords[i].handler(keywords[i].def, keywords[i].var);
				}
	}
	fclose(in);
	return 1;
}


void write_leases(void)
{
	FILE *fp, *fpp;
	unsigned int i;
	char buf[255];
	time_t curr = time(0);
	unsigned long tmp_time;
	int ret;
	int open = 0;
	struct dhcpOfferedAddr access_lease;
	struct dhcpOfferedAddr access_lease2;

	if (!(fp = fopen(server_config.lease_file, "w"))) {
		LOG(LOG_ERR, "Unable to open %s for writing", server_config.lease_file);
		return;
	}

	for (i = 0; i < server_config.max_leases; i++) {
		if (leases[i].yiaddr != 0) {

			/* screw with the time in the struct, for easier writing */
			tmp_time = leases[i].expires;

			if (server_config.remaining) {
				if (lease_expired(&(leases[i])))
					leases[i].expires = 0;
				else leases[i].expires -= curr;
			} /* else stick with the time we got */
			leases[i].expires = htonl(leases[i].expires);

			#if 1
			if((fpp = fopen(ALL_ACCESS, "r")) != NULL)     /*  /var/run/.allAccess  */
			{
		        while(fread(&access_lease, sizeof access_lease, 1, fpp) == 1)
		        {
					if(!strcmp(access_lease.chaddr, leases[i].chaddr))
					{
						if(strlen(leases[i].hostname) == 0)
							strcpy(leases[i].hostname, access_lease.hostname);
					}
		        }
				fclose(fpp);
			}
			#endif
			
			fwrite(&leases[i], sizeof(struct dhcpOfferedAddr), 1, fp);

			/* Then restore it when done. */
			leases[i].expires = tmp_time;
		}
	}
	fclose(fp);
    fp=NULL;

	if (server_config.notify_file) {
		sprintf(buf, "%s %s", server_config.notify_file, server_config.lease_file);
		system(buf);
	}

	/*write all dhcp client to /var/run/.allAccess*/
	if((fp = fopen(ALL_ACCESS, "r")) == NULL)     /*  /var/run/.allAccess  */
	{
		open = 1;
		fp = fopen(ALL_ACCESS, "at");
		fpp = fopen(UDHCPD_FILE, "r");    /*  /var/run/udhcpd.leases   */
        
        while(fread(&access_lease, sizeof access_lease, 1, fpp) == 1)
        {
			fwrite(&access_lease, sizeof(access_lease), 1, fp);
        }
		fclose(fpp);
		fclose(fp);
        fp=NULL;
	}
	fpp = fopen(UDHCPD_FILE, "r");    /*  /var/run/udhcpd.leases   */
    while(fread(&access_lease, sizeof access_lease, 1, fpp) == 1)
    {
    	int ret = 0;

		if(open == 1)
		{
			fp = fopen(ALL_ACCESS, "r");
		}
		while(fread(&access_lease2, sizeof access_lease2, 1, fp) == 1)
		{
			if(strcmp(access_lease.chaddr, access_lease2.chaddr) == 0)
			{
				ret = 1;
				break;
			}
		}
		LOG(LOG_INFO, "ret is %d", ret);
		if(ret == 0)
		{
			fclose(fp);
			fp = fopen(ALL_ACCESS, "at");
			memset(&access_lease2, 0, sizeof(access_lease2));
        	strcpy(access_lease2.chaddr, access_lease.chaddr);
			strcpy(access_lease2.hostname, access_lease.hostname);
			access_lease2.yiaddr = access_lease.yiaddr;
			access_lease2.expires = access_lease.expires;
			fwrite(&access_lease2, sizeof(access_lease2), 1, fp);
		}
		fclose(fp);
        fp=NULL;
		open =  1;
    }
    if(fp != NULL) //add by mingyue
    {
        fclose(fp);
    }
	fclose(fpp);
	LOG(LOG_INFO, "write OK");
}


void read_leases(const char *file)
{
	FILE *fp;
	unsigned int i = 0;
	struct dhcpOfferedAddr lease;

	if (!(fp = fopen(file, "r"))) {
		LOG(LOG_ERR, "Unable to open %s for reading", file);
		return;
	}

	while (i < server_config.max_leases && (fread(&lease, sizeof lease, 1, fp) == 1)) {
		/* ADDME: is it a static lease */
		if (lease.yiaddr >= server_config.start && lease.yiaddr <= server_config.end) {
			lease.expires = ntohl(lease.expires);
			if (!server_config.remaining) lease.expires -= time(0);
			if (!(add_lease(lease.hostname, lease.chaddr, lease.yiaddr, lease.expires))) {
				LOG(LOG_WARNING, "Too many leases while loading %s\n", file);
				break;
			}
			i++;
		}
	}
	DEBUG(LOG_INFO, "Read %d leases", i);
	fclose(fp);
}
