/* dhcpd.c
 *
 * udhcp Server
 * Copyright (C) 1999 Matthew Ramsay <matthewr@moreton.com.au>
 *			Chris Trew <ctrew@moreton.com.au>
 *
 * Rewrite by Russ Dill <Russ.Dill@asu.edu> July 2001
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <time.h>
#include <sys/time.h>

#include "dhcpd.h"
#include "arpping.h"
#include "socket.h"
#include "options.h"
#include "files.h"
#include "serverpacket.h"
#include "common.h"
#include "signalpipe.h"
#include "static_leases.h"


/* globals */
struct dhcpOfferedAddr *leases;
struct server_config_t server_config;

#if 0
void deal_staControl()
{
	struct staList
	{
		int id;
		char macAddr[20];
		char staDesc[80];
		char status[10];  /*on-enable-1; off-disable-0 */
		struct staList *next;
	};
	struct staList stalist;
	FILE *fp, *fp1;
	char con_buf[10];
	char buf[50];

	//system("iptables -N control_sta");
	//system("iptables -A INPUT -j control_sta");
    if ((fp = fopen("/configure_backup/.staAcl", "r")) != NULL)
	{
		if(fread(con_buf, 7, 1, fp) == 0)
		{
			fclose(fp);
			return;
		}
		//LOG(LOG_ERR, "------- OK the con_buf is %s------", con_buf);
		if(strstr(con_buf, "disable"))
		{
			fclose(fp);
			return;
		}
		else if(strstr(con_buf, "enable"))
		{
			if ((fp1 = fopen("/configure_backup/.staMac", "r")) != NULL) 
			{
				while(fread(&stalist, sizeof stalist, 1, fp1) == 1)
				{
					if(!strcmp(stalist.status, "1"))
					{
						sprintf(buf, "iwpriv ath0 addmac %s", stalist.macAddr);
						system(buf);
						//LOG(LOG_ERR, "------- OK ------ the buf is %s", buf);
						sprintf(buf, "iwpriv ath2 addmac %s", stalist.macAddr);
						system(buf);
						//memset(buf, 0, sizeof buf);
						//sprintf(buf, "iptables -A control_sta -m mac --mac-source %s -j DROP", stalist.macAddr);
						//system(buf);
					}
				}
				system("iwpriv ath0 maccmd 2");
				system("iwpriv ath2 maccmd 2");
				system("ifconfig ath0 down;ifconfig ath0 up > /dev/null 2>&1");
				system("ifconfig ath2 down;ifconfig ath2 up > /dev/null 2>&1");
				fclose(fp1);
			}
		}
	}
	fclose(fp);
}
#endif

#ifdef COMBINED_BINARY
int udhcpd_main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
	fd_set rfds;
	struct timeval tv;
	int server_socket = -1;
	int bytes, retval;
	struct dhcpMessage packet;
	uint8_t *state;
	uint8_t *server_id, *requested;
	uint32_t server_id_align, requested_align;
	unsigned long timeout_end;
	struct option_set *option;
	struct dhcpOfferedAddr *lease;
	struct dhcpOfferedAddr static_lease;
	int max_sock;
	unsigned long num_ips;

	uint32_t static_lease_ip;

	#if 0
	if(fork() == 0)
	{
		/*deal staControl*/
		deal_staControl();
		LOG(LOG_ERR, "------- OK ------");
		exit(0);
	}
	
	LOG(LOG_ERR, "------- dhcp OK------");
	#endif
	memset(&server_config, 0, sizeof(struct server_config_t));
	read_config(argc < 2 ? DHCPD_CONF_FILE : argv[1]);

	/* Start the log, sanitize fd's, and write a pid file */
	start_log_and_pid("udhcpd", server_config.pidfile);

	if ((option = find_option(server_config.options, DHCP_LEASE_TIME))) {
		memcpy(&server_config.lease, option->data + 2, 4);
		server_config.lease = ntohl(server_config.lease);
	}
	else server_config.lease = LEASE_TIME;

	/* Sanity check */
	num_ips = ntohl(server_config.end) - ntohl(server_config.start) + 1;
	if (server_config.max_leases > num_ips) {
		LOG(LOG_ERR, "max_leases value (%lu) not sane, "
			"setting to %lu instead",
			server_config.max_leases, num_ips);
		server_config.max_leases = num_ips;
	}

	leases = xcalloc(server_config.max_leases, sizeof(struct dhcpOfferedAddr));
	read_leases(server_config.lease_file);

	if (read_interface(server_config.interface, &server_config.ifindex,
			   &server_config.server, server_config.arp) < 0)
		return 1;

#ifndef UDHCP_DEBUG
	background(server_config.pidfile); /* hold lock during fork. */
#endif

	/* Setup the signal pipe */
	udhcp_sp_setup();

	timeout_end = time(0) + server_config.auto_time;
	while(1) { /* loop until universe collapses */

		if (server_socket < 0)
			if ((server_socket = listen_socket(INADDR_ANY, SERVER_PORT, server_config.interface)) < 0) {
				LOG(LOG_ERR, "FATAL: couldn't create server socket, %m");
				return 2;
			}

		max_sock = udhcp_sp_fd_set(&rfds, server_socket);
		if (server_config.auto_time) {
			tv.tv_sec = timeout_end - time(0);
			tv.tv_usec = 0;
		}
		if (!server_config.auto_time || tv.tv_sec > 0) {
			retval = select(max_sock + 1, &rfds, NULL, NULL,
					server_config.auto_time ? &tv : NULL);
		} else retval = 0; /* If we already timed out, fall through */

		if (retval == 0) {
			write_leases();
			timeout_end = time(0) + server_config.auto_time;
			continue;
		} else if (retval < 0 && errno != EINTR) {
			DEBUG(LOG_INFO, "error on select");
			continue;
		}

		switch (udhcp_sp_read(&rfds)) {
		case SIGUSR1:
			LOG(LOG_INFO, "Received a SIGUSR1");

			deal_control_staMac();
			
			//deal_offline_sta();
			
			write_leases();
			/* why not just reset the timeout, eh */
			timeout_end = time(0) + server_config.auto_time;
			continue;
		case SIGTERM:
			LOG(LOG_INFO, "Received a SIGTERM");
			return 0;
		case 0: break;		/* no signal */
		default: continue;	/* signal or error (probably EINTR) */
		}

		if ((bytes = get_packet(&packet, server_socket)) < 0) { /* this waits for a packet - idle */
			if (bytes == -1 && errno != EINTR) {
				DEBUG(LOG_INFO, "error on read, %m, reopening socket");
				close(server_socket);
				server_socket = -1;
			}
			continue;
		}

		if ((state = get_option(&packet, DHCP_MESSAGE_TYPE)) == NULL) {
			DEBUG(LOG_ERR, "couldn't get option from packet, ignoring");
			continue;
		}

		/* Look for a static lease */
		static_lease_ip = getIpByMac(server_config.static_leases, &packet.chaddr);

		if(static_lease_ip)
		{
			printf("Found static lease: %x\n", static_lease_ip);

			memcpy(&static_lease.chaddr, &packet.chaddr, 16);
			static_lease.yiaddr = static_lease_ip;
			static_lease.expires = 0;

			lease = &static_lease;

		}
		else
		{
		lease = find_lease_by_chaddr(packet.chaddr);
		}

		switch (state[0]) {
		case DHCPDISCOVER:
			DEBUG(LOG_INFO,"received DISCOVER");

			if (sendOffer(&packet) < 0) {
				LOG(LOG_ERR, "send OFFER failed");
			}
			break;
 		case DHCPREQUEST:
			DEBUG(LOG_INFO, "received REQUEST");

			char gateway_ip[50];
			FILE *fp;
			struct in_addr addr;
		
			system("cfg -e | grep AP_IPADDR= | awk -F '=' '{print $2}' > /tmp/GateWay");
			fp = fopen("/tmp/GateWay", "r");
			fgets(gateway_ip, 30, fp);
            fclose(fp);

			requested = get_option(&packet, DHCP_REQUESTED_IP);
			server_id = get_option(&packet, DHCP_SERVER_ID);

			if (requested) memcpy(&requested_align, requested, 4);
			if (server_id) memcpy(&server_id_align, server_id, 4);

			addr.s_addr=htonl(requested_align);
			DEBUG(LOG_INFO, "the addr is %s, the gateway is %s", inet_ntoa(addr), gateway_ip);
			//addr.s_addr = requested_align;
			//LOG(LOG_ERR, "the host ip is %s", inet_ntoa(addr));
			//deal_addIp(&packet);

			if (lease) {
				DEBUG(LOG_INFO, "case DHCPREQUEST: the lease is exit");
				/*the ip can not equal to gateway ip  by zzw*/
				if(strstr(gateway_ip, inet_ntoa(addr)))
				{
					sendNAK(&packet);
				}
				
				if (server_id) {
					/*the ip can not equal to gateway ip  by zzw*/
					if(strstr(gateway_ip, inet_ntoa(addr)))
					{
						sendNAK(&packet);
					}
					/* SELECTING State */
					DEBUG(LOG_INFO, "server_id = %08x", ntohl(server_id_align));
					if (server_id_align == server_config.server && requested &&
					    requested_align == lease->yiaddr) {
						sendACK(&packet, lease->yiaddr);
					}
				} else {
					/*the ip can not equal to gateway ip  by zzw*/
					if(strstr(gateway_ip, inet_ntoa(addr)))
					{
						sendNAK(&packet);
					}
					else if (requested) {
						/* INIT-REBOOT State */
						if (lease->yiaddr == requested_align)
							sendACK(&packet, lease->yiaddr);
						else sendNAK(&packet);
					} else {
						/* RENEWING or REBINDING State */
						if (lease->yiaddr == packet.ciaddr)
							sendACK(&packet, lease->yiaddr);
						else {
							/* don't know what to do!!!! */
							sendNAK(&packet);
						}
					}
				}

			/* what to do if we have no record of the client */
			} else if (server_id) {
				DEBUG(LOG_INFO, "case DHCPREQUEST: the server id is %08x", server_id);
				/* SELECTING State */

			} else if (requested) {
				DEBUG(LOG_INFO, "case DHCPREQUEST: the requested is exit");
				/* INIT-REBOOT State */
				if ((lease = find_lease_by_yiaddr(requested_align))) {
					if (lease_expired(lease)) {
						/* probably best if we drop this lease */
						memset(lease->chaddr, 0, 16);
					/* make some contention for this address */
					} else sendNAK(&packet);
				} else if (requested_align < server_config.start ||
					   requested_align > server_config.end) {
					sendNAK(&packet);
				} /* else remain silent */
				else 
				{/*if the client has ip, make it send request again*/
					if (requested)
					{
						sendNAK(&packet);
					}
				}

			} else {
				DEBUG(LOG_INFO, "do nothing");
				 /* RENEWING or REBINDING State */
			}
			break;
		case DHCPDECLINE:
			DEBUG(LOG_INFO,"received DECLINE");
			if (lease) {
				memset(lease->chaddr, 0, 16);
				lease->expires = time(0) + server_config.decline_time;
			}
			break;
		case DHCPRELEASE:
			DEBUG(LOG_INFO,"received RELEASE");
			/*if receive RELEASE, clear the host*/
			clear_dhcpIp(&packet);
			if (lease) lease->expires = time(0);
			break;
		case DHCPINFORM:
			DEBUG(LOG_INFO,"received INFORM");
			send_inform(&packet);
			break;
		default:
			LOG(LOG_WARNING, "unsupported DHCP message (%02x) -- ignoring", state[0]);
		}
	}

	return 0;
}

