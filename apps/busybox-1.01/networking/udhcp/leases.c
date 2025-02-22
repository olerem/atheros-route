/*
 * leases.c -- tools to manage DHCP leases
 * Russ Dill <Russ.Dill@asu.edu> July 2001
 */

#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "dhcpd.h"
#include "files.h"
#include "options.h"
#include "leases.h"
#include "arpping.h"
#include "common.h"

#include "static_leases.h"

#define STA_MAC "/configure_backup/.staMac"
#define STA_ACL "/configure_backup/.staAcl"

uint8_t blank_chaddr[] = {[0 ... 15] = 0};
struct staList
{
	int id;
	char macAddr[20];
	char staDesc[80];
	char status[10];  /*on-enable-1; off-disable-0 */
	struct staList *next;
};

/* clear every lease out that chaddr OR yiaddr matches and is nonzero */
void clear_lease(uint8_t *chaddr, uint32_t yiaddr)
{
	unsigned int i, j;

	for (j = 0; j < 16 && !chaddr[j]; j++);

	for (i = 0; i < server_config.max_leases; i++)
		if ((j != 16 && !memcmp(leases[i].chaddr, chaddr, 16)) ||
		    (yiaddr && leases[i].yiaddr == yiaddr)) {
			memset(&(leases[i]), 0, sizeof(struct dhcpOfferedAddr));
		}
}


/* add a lease into the table, clearing out any old ones */
struct dhcpOfferedAddr *add_lease(uint8_t *hostname, uint8_t *chaddr, uint32_t yiaddr, unsigned long lease)
{
	struct dhcpOfferedAddr *oldest;

	/* clean out any old ones */
	clear_lease(chaddr, yiaddr);

	oldest = oldest_expired_lease();

	if (oldest) {
		if (hostname) {
			uint8_t length = *(hostname-1); 
			if (length>15) length = 15; 
			memcpy(oldest->hostname, hostname, length); 
			oldest->hostname[length] = 0; 
		}
		memcpy(oldest->chaddr, chaddr, 16);
		oldest->yiaddr = yiaddr;
		oldest->expires = time(0) + lease;
	}

	return oldest;
}


/* true if a lease has expired */
int lease_expired(struct dhcpOfferedAddr *lease)
{
	return (lease->expires < (unsigned long) time(0));
}


/* Find the oldest expired lease, NULL if there are no expired leases */
struct dhcpOfferedAddr *oldest_expired_lease(void)
{
	struct dhcpOfferedAddr *oldest = NULL;
	unsigned long oldest_lease = time(0);
	unsigned int i;


	for (i = 0; i < server_config.max_leases; i++)
		if (oldest_lease > leases[i].expires) {
			oldest_lease = leases[i].expires;
			oldest = &(leases[i]);
		}
	return oldest;

}


/* Find the first lease that matches chaddr, NULL if no match */
struct dhcpOfferedAddr *find_lease_by_chaddr(uint8_t *chaddr)
{
	unsigned int i;

	for (i = 0; i < server_config.max_leases; i++)
		if (!memcmp(leases[i].chaddr, chaddr, 16)) return &(leases[i]);

	return NULL;
}


/* Find the first lease that matches yiaddr, NULL is no match */
struct dhcpOfferedAddr *find_lease_by_yiaddr(uint32_t yiaddr)
{
	unsigned int i;

	for (i = 0; i < server_config.max_leases; i++)
		if (leases[i].yiaddr == yiaddr) return &(leases[i]);

	return NULL;
}


/* check is an IP is taken, if it is, add it to the lease table */
static int check_ip(uint32_t addr)
{
	struct in_addr temp;
	char gateway_ip[50];
	FILE *fp;
	system("cfg -e | grep AP_IPADDR= | awk -F '=' '{print $2}' > /tmp/GateWay");
	temp.s_addr = addr;
	fp = fopen("/tmp/GateWay", "r");
	fgets(gateway_ip, 30, fp);
	fclose(fp);
	LOG(LOG_INFO, " the gateway is [%s];the send ip is %[s] \n", gateway_ip, inet_ntoa(temp));
	if(strstr(gateway_ip, inet_ntoa(temp)))
	{LOG(LOG_INFO, "22222 the gateway is [%s];the send ip is %[s] \n", gateway_ip, inet_ntoa(temp));
		LOG(LOG_INFO, "%s belongs to someone, reserving it for %ld seconds",
			inet_ntoa(temp), server_config.conflict_time);
		add_lease(blank_chaddr, blank_chaddr, addr, server_config.conflict_time);
				return 1;
	}
	system("rm -rf /tmp/GateWay");
	LOG(LOG_INFO, "3333333 the gateway is [%s];the send ip is %[s] \n", gateway_ip, inet_ntoa(temp));
	
	if (arpping(addr, server_config.server, server_config.arp, server_config.interface) == 0) {
		temp.s_addr = addr;
		LOG(LOG_INFO, "%s belongs to someone, reserving it for %ld seconds",
			inet_ntoa(temp), server_config.conflict_time);
		add_lease(blank_chaddr, blank_chaddr, addr, server_config.conflict_time);
		return 1;
	}else return 0;
}


/* find an assignable address, it check_expired is true, we check all the expired leases as well.
 * Maybe this should try expired leases by age... */
uint32_t find_address(int check_expired)
{
	uint32_t addr, ret;
	struct dhcpOfferedAddr *lease = NULL;

	addr = ntohl(server_config.start); /* addr is in host order here */
	for (;addr <= ntohl(server_config.end); addr++) {

		/* ie, 192.168.55.0 */
		if (!(addr & 0xFF)) continue;

		/* ie, 192.168.55.255 */
		if ((addr & 0xFF) == 0xFF) continue;

		/* Only do if it isn't an assigned as a static lease */
		if(!reservedIp(server_config.static_leases, htonl(addr)))
		{

		/* lease is not taken */
		ret = htonl(addr);
		if ((!(lease = find_lease_by_yiaddr(ret)) ||

		     /* or it expired and we are checking for expired leases */
		     (check_expired  && lease_expired(lease))) &&

		     /* and it isn't on the network */
	    	     !check_ip(ret)) {
			return ret;
			break;
		}
	}
	}
	return 0;
}

#if 0
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
	
	ret = yiaddr;
	if (/*it isn't on the network */
		(arpping(ret, server_config.server, server_config.arp, server_config.interface) == 0) &&
		/*it expired and we are checking for expired leases*/
		(!lease_expired(leases)) )
	{
		return 0;
	}
	return 1;

}
#endif

/* Find the lease that matches chaddr, NULL if no match */
struct dhcpOfferedAddr *deal_control_staMac()
{
	unsigned int i, j, k, id;
	FILE *fp, *fpp;
	struct staList stalist;
	char mac_buf[20];
	char buf[10];
	//const char *staFile = "/configure_backup/.staMac";
	//const char *staFile1 = "/configure_backup/.staAcl";

	if((fpp = fopen(STA_ACL, "r")) == NULL)
	{
		//LOG(LOG_ERR, "Unable to open %s for reading", staFile1);
		return NULL;
	}
	if(fread(buf, 7, 1, fpp) == 1)
	{
		//LOG(LOG_ERR, "******the %s's buf is %s", staFile1, buf);
		if(strncmp(buf, "enable", 6) == 0)
		{
			if ((fp = fopen(STA_MAC, "r")) != NULL)
			{
				while(fread(&stalist, sizeof stalist, 1, fp) == 1)
				{
					if(strcmp(stalist.status, "1"))
						continue;
					for (i = 0; i < server_config.max_leases; i++)
					{
						if(strlen(leases[i].hostname) > 0)
						{
							for(j = 0, k = 0 ; j < 6; j++, k+=3)
							{
			                    sprintf(&mac_buf[k], "%02x:", leases[i].chaddr[j]);
							}
							
							//LOG(LOG_ERR, "******the lease's is %s, the read is %s", mac_buf, stalist.macAddr);
							
							if (!strncmp(mac_buf, stalist.macAddr, 17))
							{
								//LOG(LOG_ERR, "the MAC %s is exit", stalist.macAddr);
								leases[i].expires = time(0);
								/* clean out any old ones */
								memset(&(leases[i]), 0, sizeof(struct dhcpOfferedAddr));
								//clear_lease(leases[i].chaddr, leases[i].yiaddr);
							}
						}
					}
				}
				fclose(fp);
			}
		}
		fclose(fpp);
	}
	else
	{
		//LOG(LOG_ERR, "the read %s for reading is not disable", staFile1);
		fclose(fpp);
	}

	return NULL;
}

