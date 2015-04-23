/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * Copyright (c) 2015, Joyent, Inc.
 */

/*
 * tcpkatest: test TCP keep-alive configuration.  This program makes a
 * connection to a remote server and sets the given keepalive options on the
 * socket.  It then loops polling until the socket is readable and reading data.
 * See the README for usage information.
 *
 * This code is a little janky.
 */

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

char *kat_arg0;
const char *kat_usagefmt =
    "usage: %s [-c CNT] [-d IDLE] [-i INTVL] HOST:PORT\n";

typedef struct {
	struct sockaddr_in	cp_ip;		/* remote IP address and port */
	int			cp_keepalive;	/* SO_KEEPALIVE value */
	int			cp_keepidle;	/* TCP_KEEPIDLE value */
	int			cp_keepcnt;	/* TCP_KEEPCNT value */
	int			cp_keepintvl;	/* TCP_KEEPINTVL value */
} connparams_t;

static void usage(void);
static int parse_positive_int_option(char, const char *);
static int parse_ip4port(const char *, struct sockaddr_in *);
static int connectandwait(connparams_t *);
static void log_time(void);

int
main(int argc, char *argv[])
{
	int rv;
	char c;
	const char *ipport;
	connparams_t cp;

	/*
	 * The command-line options fill in the connparams_t and pass that to
	 * connectandwait().
	 */
	kat_arg0 = basename(argv[0]);
	bzero(&cp, sizeof (cp));
	cp.cp_keepalive = 1;
	cp.cp_keepidle = -1;
	cp.cp_keepcnt = -1;
	cp.cp_keepintvl = -1;

	while ((c = getopt(argc, argv, ":c:d:i:")) != -1) {
		switch (c) {
		case 'c':
			cp.cp_keepcnt = parse_positive_int_option(
			    optopt, optarg);
			break;

		case 'd':
			cp.cp_keepidle = parse_positive_int_option(
			    optopt, optarg);
			break;

		case 'i':
			cp.cp_keepintvl = parse_positive_int_option(
			    optopt, optarg);
			break;
		
		case ':':
			warnx("option requires an argument: -%c", optopt);
			usage();
			break;

		case '?':
			warnx("unrecognized option: -%c", optopt);
			usage();
			break;
		}
	}

	if (optind > argc - 1) {
		warnx("missing required arguments");
		usage();
	}

	ipport = argv[optind++];
	if (parse_ip4port(ipport, &cp.cp_ip) == -1) {
		warnx("invalid IP/port: \"%s\"", ipport);
		usage();
	}

	(void) fprintf(stderr, "will connect to: %s port %d\n",
	    inet_ntoa(cp.cp_ip.sin_addr), ntohs(cp.cp_ip.sin_port));
	(void) fprintf(stderr, "SO_KEEPALIVE  = %d\n", cp.cp_keepalive);
	(void) fprintf(stderr, "TCP_KEEPIDLE  = %d\n", cp.cp_keepidle);
	(void) fprintf(stderr, "TCP_KEEPCNT   = %d\n", cp.cp_keepcnt);
	(void) fprintf(stderr, "TCP_KEEPINTVL = %d\n", cp.cp_keepintvl);
	rv = connectandwait(&cp);
	return (rv == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

/*
 * Print a usage message and exit.
 */
static void
usage(void)
{
	(void) fprintf(stderr, kat_usagefmt, kat_arg0);
	exit(2);
}

/*
 * Given a command line option-argument "optarg" for short option "optname",
 * parse it as a number and return it as an "int".  If it's not a valid positive
 * "int", prints a usage message.
 */
static int
parse_positive_int_option(char optname, const char *optarg)
{
	char *endptr;
	long val;

	val = strtol(optarg, &endptr, 0);
	if (*endptr != '\0' || val <= 0 || val >= INT_MAX) {
		warnx("invalid value for -%c", optname);
		usage();
	}

	return ((int)val);
}

/*
 * Parse the given IP address and port pair in "ipport" (as "IP:PORT") and store
 * the result into "addrp".  Returns -1 on bad input.
 */
static int
parse_ip4port(const char *ipport, struct sockaddr_in *addrp)
{
	char *c, *endptr;
	long port;
	char buf[INET_ADDRSTRLEN + sizeof ("65536")];

	(void) strlcpy(buf, ipport, sizeof (buf));
	c = strrchr(buf, ':');
	if (c == NULL) {
		return (-1);
	}

	*c = '\0';
	if (inet_pton(AF_INET, buf, &addrp->sin_addr) != 1) {
		return (-1);
	}

	port = strtol(c + 1, &endptr, 10);
	if (*endptr != '\0' || port <= 0 || port >= 65536) {
		return (-1);
	}

	addrp->sin_family = AF_INET;
	addrp->sin_port = htons((uint16_t)port);
	return (0);
}

/*
 * Connect to the IP specified in "cpp" using the contained keepalive
 * parameters.
 */
static int
connectandwait(connparams_t *cpp)
{
	struct protoent *protop;
	int sock, err;

	err = -1;
	protop = getprotobyname("tcp");
	if (protop == NULL) {
		warnx("protocol not found: \"tcp\"");
		return (-1);
	}

	sock = socket(PF_INET, SOCK_STREAM, protop->p_proto);
	if (sock < 0) {
		warn("connect \"%s\": socket", inet_ntoa(cpp->cp_ip.sin_addr));
		goto out;
	}

	if (cpp->cp_keepalive != 0 &&
	    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE,
	    &cpp->cp_keepalive, sizeof (cpp->cp_keepalive)) != 0) {
		warn("setsockopt SO_KEEPALIVE");
		goto cleanup;
	}

	if (cpp->cp_keepidle != -1 &&
	    setsockopt(sock, protop->p_proto, TCP_KEEPIDLE,
	    &cpp->cp_keepidle, sizeof (cpp->cp_keepidle)) != 0) {
		warn("setsockopt TCP_KEEPIDLE");
		goto cleanup;
	}

	if (cpp->cp_keepintvl != -1 &&
	    setsockopt(sock, protop->p_proto, TCP_KEEPINTVL,
	    &cpp->cp_keepintvl, sizeof (cpp->cp_keepintvl)) != 0) {
		warn("setsockopt TCP_KEEPINTVL");
		goto cleanup;
	}

	if (cpp->cp_keepcnt != -1 &&
	    setsockopt(sock, protop->p_proto, TCP_KEEPCNT,
	    &cpp->cp_keepcnt, sizeof (cpp->cp_keepcnt)) != 0) {
		warn("setsockopt TCP_KEEPCNT");
		goto cleanup;
	}

	if (connect(sock, (struct sockaddr *)&cpp->cp_ip,
	    sizeof (cpp->cp_ip)) != 0) {
		warn("connect \"%s\": connect", inet_ntoa(cpp->cp_ip.sin_addr));
		goto cleanup;
	}

	log_time();
	(void) fprintf(stderr, "connected\n");
	for (;;) {
		struct pollfd pfd;
		char buf[512];
		ssize_t rv;

		pfd.fd = sock;
		pfd.events = POLLIN;
		pfd.revents = 0;
		if (poll(&pfd, 1, -1) < 0) {
			warn("poll");
			goto cleanup;
		}

		log_time();
		(void) fprintf(stderr, "poll events: 0x%x\n", pfd.revents);
		if ((pfd.revents & ~POLLIN) != 0) {
			break;
		}

		assert((pfd.revents & POLLIN) != 0);
		rv = read(sock, buf, sizeof (buf));
		if (rv == -1) {
			log_time();
			warn("read");
			goto cleanup;
		}

		log_time();
		(void) fprintf(stderr, "read %d bytes\n", (int)rv);
	}

	err = 0;
cleanup:
	(void) close(sock);
out:
	return (err);
}

/*
 * Prints a timestamp to stderr (with no newline).  This is a prelude to
 * printing some other message.
 */
static void
log_time(void)
{
	int errsave;
	time_t nowt;
	struct tm nowtm;
	char buf[sizeof ("2014-01-01T01:00:00Z")];

	errsave = errno;
	time(&nowt);
	gmtime_r(&nowt, &nowtm);
	if (strftime(buf, sizeof (buf), "%FT%TZ", &nowtm) == 0) {
		err(1, "strftime failed unexpectedly");
	}

	(void) fprintf(stderr, "%s: ", buf);
	errno = errsave;
}
