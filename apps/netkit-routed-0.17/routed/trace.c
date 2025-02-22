/*
 * Copyright (c) 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * From: @(#)trace.c	5.11 (Berkeley) 2/28/91
 * From: @(#)trace.c	8.1 (Berkeley) 6/5/93
 */
char trace_rcsid[] = 
  "$Id: //depot/sw/releases/Aquila_9.2.0_U11/apps/netkit-routed-0.17/routed/trace.c#1 $";


/*
 * Routing Table Management Daemon
 */
#define	RIPCMDS
#include "defs.h"
#include <sys/stat.h>
/* #include <signal.h> <--- unused? */
#include <fcntl.h>
#include <syslog.h>
#include <errno.h>
#include "pathnames.h"

#define	NRECORDS	50		/* size of circular trace buffer */

FILE	*ftrace = NULL /*stdout*/;	/* output trace file */
int traceactions=0;			/* on/off */
int tracepackets;		/* watch packets as they go by */
int tracehistory;		/* on/off */
static int tracecontents;	/* watch packet contents as they go by */

static	struct timeval lastlog;
static	char *savetracename;


static int iftraceinit(struct interface *, struct ifdebug *);
void dumpif(FILE *, struct interface *);
void dumptrace(FILE *, char *, struct ifdebug *);

void traceinit(struct interface *ifp)
{

	if (iftraceinit(ifp, &ifp->int_input) &&
	    iftraceinit(ifp, &ifp->int_output))
		return;
	tracehistory = 0;
	fprintf(stderr, "traceinit: can't init %s\n", ifp->int_name);
}

static int iftraceinit(struct interface *ifp, struct ifdebug *ifd)
{
	struct iftrace *t;

	ifd->ifd_records =
	  (struct iftrace *)malloc(NRECORDS * sizeof (struct iftrace));
	if (ifd->ifd_records == 0)
		return (0);
	ifd->ifd_front = ifd->ifd_records;
	ifd->ifd_count = 0;
	for (t = ifd->ifd_records; t < ifd->ifd_records + NRECORDS; t++) {
		t->ift_size = 0;
		t->ift_packet = 0;
	}
	ifd->ifd_if = ifp;
	return (1);
}

void traceon(char *file)
{
	struct stat stbuf;
	int fd;
	const char *logdir = _PATH_LOGDIR;

	if (ftrace != NULL)
		return;

	/*
	 * Trace file requests could come from anywhere...
	 */
        
	if (strncmp(file, logdir, strlen(logdir)) || strstr(file, "/../")) {
		syslog(LOG_ERR, "Cannot log to %s: not under %s\n", 
		       file, logdir);
		return;
	}

	/*
	 * Also check that the environment is sane.
	 */
	if (stat(logdir, &stbuf)<0 || !S_ISDIR(stbuf.st_mode)) {
		syslog(LOG_ERR, "Configuration problem: %s does not exist "
		       "or is not a directory", logdir);
		syslog(LOG_ERR, "Logging disabled");
		return;
	}

	if ((stbuf.st_mode & 022)!=0 || stbuf.st_uid!=0) {
		syslog(LOG_ERR, "Configuration problem: wrong permissions for "
		       "%s (should be mode 700 and owned by root)",
		       logdir);
		syslog(LOG_ERR, "Logging disabled");
		return;
	}
        
	/*
	 * 7/22/00 took out O_EXCL on the grounds that since we're
	 * checking that the file is in the log dir, and the log 
	 * dir is supposed to be root-only, appending to an
	 * existing file is not particularly evil.
	 *
	 * If Linux ever supports O_NOLINK that should go in here though.
	 */
	fd = open(file, O_WRONLY|O_APPEND|O_CREAT|O_NDELAY, 0600);
	
	if (fd == -1) {
		syslog(LOG_ERR, "Cannot log to %s: %s\n",
		       file, strerror(errno));
		return;
	}
	if (fstat(fd, &stbuf) >= 0 && !S_ISREG(stbuf.st_mode)) {
		syslog(LOG_ERR, "Cannot log to %s: not a regular file\n",
		       file);
		return;
	}
       
	fcntl(fd, F_SETFL, 0);  /* Turn NDELAY off */

	savetracename = file;
	(void) gettimeofday(&now, (struct timezone *)NULL);
	ftrace = fdopen(fd, "a");
	if (ftrace == NULL)
		return;
	fflush(stdout);
	fflush(stderr);
	dup2(fileno(ftrace), 1);
	dup2(fileno(ftrace), 2);
	traceactions = 1;
	fprintf(ftrace, "Tracing enabled %s\n", ctime(&now.tv_sec));
}

void traceoff(void)
{
	if (!traceactions)
		return;
	if (ftrace != NULL) {
		int fd = open(_PATH_DEVNULL, O_RDWR);

		if (fd<0) {
			fprintf(ftrace, "Disable tracing: /dev/null: %s\n",
				strerror(errno));
			return;
		}

		fprintf(ftrace, "Tracing disabled %s\n",
		    ctime(&now.tv_sec));
		fflush(ftrace);
		(void) dup2(fd, 1);
		(void) dup2(fd, 2);
		(void) close(fd);
		fclose(ftrace);
		ftrace = NULL;
	}
	traceactions = 0;
	tracehistory = 0;
	tracepackets = 0;
	tracecontents = 0;
}

void sigtrace(int s)
{

	if (s == SIGUSR2)
		traceoff();
	else if (ftrace == NULL && savetracename)
		traceon(savetracename);
	else
		bumploglevel();
}

/*
 * Move to next higher level of tracing when -t option processed or
 * SIGUSR1 is received.  Successive levels are:
 *	traceactions
 *	traceactions + tracepackets
 *	traceactions + tracehistory (packets and contents after change)
 *	traceactions + tracepackets + tracecontents
 */

void bumploglevel(void)
{

	(void) gettimeofday(&now, NULL);
	if (traceactions == 0) {
		traceactions++;
		if (ftrace)
			fprintf(ftrace, "Tracing actions started %s\n",
			    ctime(&now.tv_sec));
	} else if (tracepackets == 0) {
		tracepackets++;
		tracehistory = 0;
		tracecontents = 0;
		if (ftrace)
			fprintf(ftrace, "Tracing packets started %s\n",
			    ctime(&now.tv_sec));
	} else if (tracehistory == 0) {
		tracehistory++;
		if (ftrace)
			fprintf(ftrace, "Tracing history started %s\n",
			    ctime(&now.tv_sec));
	} else {
		tracepackets++;
		tracecontents++;
		tracehistory = 0;
		if (ftrace)
			fprintf(ftrace, "Tracing packet contents started %s\n",
			    ctime(&now.tv_sec));
	}
	if (ftrace)
		fflush(ftrace);
}

void trace(struct ifdebug *ifd, struct sockaddr *who, char *p, int len,int m)
{
	struct iftrace *t;

	if (ifd->ifd_records == 0)
		return;
	t = ifd->ifd_front++;
	if (ifd->ifd_front >= ifd->ifd_records + NRECORDS)
		ifd->ifd_front = ifd->ifd_records;
	if (ifd->ifd_count < NRECORDS)
		ifd->ifd_count++;
	if (t->ift_size > 0 && t->ift_size < len && t->ift_packet) {
		free(t->ift_packet);
		t->ift_packet = 0;
	}
	t->ift_stamp = now;
	t->ift_who = *who;
	if (len > 0 && t->ift_packet == 0) {
		t->ift_packet = malloc(len);
		if (t->ift_packet == 0)
			len = 0;
	}
	if (len > 0)
		memcpy(t->ift_packet, p, len);
	t->ift_size = len;
	t->ift_metric = m;
}

void traceaction(FILE *fd, char *action, struct rt_entry *rt)
{
	struct sockaddr_in *dst, *gate;
	static struct bits {
		int		t_bits;
		const char	*t_name;
	} flagbits[] = {
		{ RTF_UP,	"UP" },
		{ RTF_GATEWAY,	"GATEWAY" },
		{ RTF_HOST,	"HOST" },
		{ 0, 0 }
	}, statebits[] = {
		{ RTS_PASSIVE,	"PASSIVE" },
		{ RTS_REMOTE,	"REMOTE" },
		{ RTS_INTERFACE,"INTERFACE" },
		{ RTS_CHANGED,	"CHANGED" },
		{ RTS_INTERNAL,	"INTERNAL" },
		{ RTS_EXTERNAL,	"EXTERNAL" },
		{ RTS_SUBNET,	"SUBNET" },
		{ 0, 0 }
	};
	struct bits *p;
	int first;
	char *cp;

	if (fd == NULL)
		return;
	if (lastlog.tv_sec != now.tv_sec || lastlog.tv_usec != now.tv_usec) {
		fprintf(fd, "\n%.19s:\n", ctime(&now.tv_sec));
		lastlog = now;
	}
	fprintf(fd, "%s ", action);
	dst = (struct sockaddr_in *)&rt->rt_dst;
	gate = (struct sockaddr_in *)&rt->rt_router;
	fprintf(fd, "dst %s, ", inet_ntoa(dst->sin_addr));
	fprintf(fd, "router %s, metric %d, flags",
	     inet_ntoa(gate->sin_addr), rt->rt_metric);
	cp = " %s";
	for (first = 1, p = flagbits; p->t_bits > 0; p++) {
		if ((rt->rt_flags & p->t_bits) == 0)
			continue;
		fprintf(fd, cp, p->t_name);
		if (first) {
			cp = "|%s";
			first = 0;
		}
	}
	fprintf(fd, " state");
	cp = " %s";
	for (first = 1, p = statebits; p->t_bits > 0; p++) {
		if ((rt->rt_state & p->t_bits) == 0)
			continue;
		fprintf(fd, cp, p->t_name);
		if (first) {
			cp = "|%s";
			first = 0;
		}
	}
	fprintf(fd, " timer %d\n", rt->rt_timer);
	if (tracehistory && !tracepackets &&
	    (rt->rt_state & RTS_PASSIVE) == 0 && rt->rt_ifp)
		dumpif(fd, rt->rt_ifp);
	fflush(fd);
	if (ferror(fd))
		traceoff();
}

void tracenewmetric(FILE *fd, struct rt_entry *rt, int newmetric)
{
	struct sockaddr_in *dst, *gate;

	if (fd == NULL)
		return;
	if (lastlog.tv_sec != now.tv_sec || lastlog.tv_usec != now.tv_usec) {
		fprintf(fd, "\n%.19s:\n", ctime(&now.tv_sec));
		lastlog = now;
	}
	dst = (struct sockaddr_in *)&rt->rt_dst;
	gate = (struct sockaddr_in *)&rt->rt_router;
	fprintf(fd, "CHANGE metric dst %s, ", inet_ntoa(dst->sin_addr));
	fprintf(fd, "router %s, from %d to %d\n",
	     inet_ntoa(gate->sin_addr), rt->rt_metric, newmetric);
	fflush(fd);
	if (ferror(fd))
		traceoff();
}

void dumpif(FILE *fd, struct interface *ifp)
{
	if (ifp->int_input.ifd_count || ifp->int_output.ifd_count) {
		fprintf(fd, "*** Packet history for interface %s ***\n",
			ifp->int_name);
		dumptrace(fd, "from", &ifp->int_input);
		fprintf(fd, "*** end packet history ***\n");
	}
}

void dumptrace(FILE *fd, char *dir, struct ifdebug *ifd)
{
	struct iftrace *t;
	char *cp = !strcmp(dir, "to") ? "Output" : "Input";

	if (ifd->ifd_front == ifd->ifd_records &&
	    ifd->ifd_front->ift_size == 0) {
		fprintf(fd, "%s: no packets.\n", cp);
		fflush(fd);
		return;
	}
	fprintf(fd, "%s trace:\n", cp);
	t = ifd->ifd_front - ifd->ifd_count;
	if (t < ifd->ifd_records)
		t += NRECORDS;
	for ( ; ifd->ifd_count; ifd->ifd_count--, t++) {
		if (t >= ifd->ifd_records + NRECORDS)
			t = ifd->ifd_records;
		if (t->ift_size == 0)
			continue;
		dumppacket(fd, dir, &t->ift_who,
		    t->ift_packet, t->ift_size, &t->ift_stamp);
	}
}

void dumppacket(FILE *fd, char *dir, struct sockaddr *sa, 
	char *cp, int size, struct timeval *stamp)
{
	struct rip *msg = (struct rip *)cp;
	struct sockaddr_in *who=(struct sockaddr_in *)sa;
	struct netinfo *n;

	if (fd == NULL)
		return;
	if (msg->rip_cmd && msg->rip_cmd < RIPCMD_MAX)
		fprintf(fd, "%s %s %s.%d %.19s:\n", ripcmds[msg->rip_cmd],
		    dir, inet_ntoa(who->sin_addr), ntohs(who->sin_port),
		    ctime(&stamp->tv_sec));
	else {
		fprintf(fd, "Bad cmd 0x%x %s %s.%d %.19s\n", msg->rip_cmd,
		    dir, inet_ntoa(who->sin_addr), ntohs(who->sin_port),
		    ctime(&stamp->tv_sec));
		fprintf(fd, "size=%d cp=%p packet=%p\n", size, cp, packet);
		fflush(fd);
		return;
	}
	if (tracepackets && tracecontents == 0) {
		fflush(fd);
		return;
	}
	switch (msg->rip_cmd) {

	case RIPCMD_REQUEST:
	case RIPCMD_RESPONSE:
		size -= 4 * sizeof (char);
		n = msg->rip_nets;
		for (; size > 0; n++, size -= sizeof (struct netinfo)) {
			if (size < (int)sizeof (struct netinfo)) {
				fprintf(fd, "(truncated record, len %d)\n",
				    size);
				break;
			}
			if (sizeof(n->rip_dst.sa_family) > 1)
			    n->rip_dst.sa_family = ntohs(n->rip_dst.sa_family);

			switch ((int)n->rip_dst.sa_family) {

			case AF_INET:
				fprintf(fd, "\tdst %s metric %ld\n",
#define	satosin(sa)	((struct sockaddr_in *)&sa)
				     inet_ntoa(satosin(n->rip_dst)->sin_addr),
				     (long int)ntohl(n->rip_metric));
				break;

			default:
				fprintf(fd, "\taf %d? metric %ld\n",
				     n->rip_dst.sa_family,
				     (long int)ntohl(n->rip_metric));
				break;
			}
		}
		break;

	case RIPCMD_TRACEON:
		fprintf(fd, "\tfile=%*s\n", size, msg->rip_tracefile);
		break;

	case RIPCMD_TRACEOFF:
		break;
	}
	fflush(fd);
	if (ferror(fd))
		traceoff();
}
