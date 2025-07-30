/*	$FreeBSD: rev 288217 $	*/
/*	$NetBSD: common.c,v 1.29 2014/01/08 20:25:34 joerg Exp $	*/
/*-
 * Copyright (c) 1998-2014 Dag-Erling Smorgrav
 * Copyright (c) 2008, 2010 Joerg Sonnenberger <joerg@NetBSD.org>
 * Copyright (c) 2013 Michael Gmelin <freebsd@grem.de>
 * Copyright (c) 2019 Duncan Overbruck <mail@duncano.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "compat.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#if defined(HAVE_INTTYPES_H) || defined(NETBSD)
#include <inttypes.h>
#endif
#include <netdb.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <strings.h>
#include <poll.h>
#include <fcntl.h>

#ifndef MSG_NOSIGNAL
#include <signal.h>
#endif

#ifdef WITH_SSL
#include <openssl/x509v3.h>
#endif

#include <pthread.h>

#include "fetch.h"
#include "common.h"

/*** Local data **************************************************************/

/*
 * Error messages for resolver errors
 */
static struct fetcherr netdb_errlist[] = {
#ifdef EAI_NODATA
	{ EAI_NODATA,	FETCH_RESOLV,	"Host not found" },
#endif
	{ EAI_AGAIN,	FETCH_TEMP,	"Transient resolver failure" },
	{ EAI_FAIL,	FETCH_RESOLV,	"Non-recoverable resolver failure" },
	{ EAI_NONAME,	FETCH_RESOLV,	"No address record" },
	{ -1,		FETCH_UNKNOWN,	"Unknown resolver error" }
};

/*** Error-reporting functions ***********************************************/

/*
 * Map error code to string
 */
static struct fetcherr *
fetch_finderr(struct fetcherr *p, int e)
{
	while (p->num != -1 && p->num != e)
		p++;
	return (p);
}

/*
 * Set error code
 */
void
fetch_seterr(struct fetcherr *p, int e)
{
	p = fetch_finderr(p, e);
	fetchLastErrCode = p->cat;
	snprintf(fetchLastErrString, MAXERRSTRING, "%s", p->string);
}

/*
 * Set error code according to errno
 */
void
fetch_syserr(void)
{
	switch (errno) {
	case 0:
		fetchLastErrCode = FETCH_OK;
		break;
	case EPERM:
	case EACCES:
	case EROFS:
#ifdef EAUTH
	case EAUTH:
#endif
#ifdef ENEEDAUTH
	case ENEEDAUTH:
#endif
		fetchLastErrCode = FETCH_AUTH;
		break;
	case ENOENT:
	case EISDIR: /* XXX */
		fetchLastErrCode = FETCH_UNAVAIL;
		break;
	case ENOMEM:
		fetchLastErrCode = FETCH_MEMORY;
		break;
	case EBUSY:
	case EAGAIN:
		fetchLastErrCode = FETCH_TEMP;
		break;
	case EEXIST:
		fetchLastErrCode = FETCH_EXISTS;
		break;
	case ENOSPC:
		fetchLastErrCode = FETCH_FULL;
		break;
	case EADDRINUSE:
	case EADDRNOTAVAIL:
	case ENETDOWN:
	case ENETUNREACH:
	case ENETRESET:
	case EHOSTUNREACH:
		fetchLastErrCode = FETCH_NETWORK;
		break;
	case ECONNABORTED:
	case ECONNRESET:
		fetchLastErrCode = FETCH_ABORT;
		break;
	case ETIMEDOUT:
		fetchLastErrCode = FETCH_TIMEOUT;
		break;
	case ECONNREFUSED:
	case EHOSTDOWN:
		fetchLastErrCode = FETCH_DOWN;
		break;
default:
		fetchLastErrCode = FETCH_UNKNOWN;
	}
	snprintf(fetchLastErrString, MAXERRSTRING, "%s", strerror(errno));
}


/*
 * Emit status message
 */
void LIBFETCH_PRINTFLIKE(1, 2)
fetch_info(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}


/*** Network-related utility functions ***************************************/

/*
 * Return the default port for a scheme
 */
int
fetch_default_port(const char *scheme)
{
	struct servent *se;

	if (strcasecmp(scheme, SCHEME_FTP) == 0)
		return (FTP_DEFAULT_PORT);
	if (strcasecmp(scheme, SCHEME_HTTP) == 0)
		return (HTTP_DEFAULT_PORT);
	if (strcasecmp(scheme, SCHEME_HTTPS) == 0)
		return (HTTPS_DEFAULT_PORT);
	if (strcasecmp(scheme, SCHEME_SOCKS5) == 0)
		return (SOCKS5_DEFAULT_PORT);
	if ((se = getservbyname(scheme, "tcp")) != NULL)
		return (ntohs(se->s_port));
	return (0);
}

/*
 * Return the default proxy port for a scheme
 */
int
fetch_default_proxy_port(const char *scheme)
{
	if (strcasecmp(scheme, SCHEME_FTP) == 0)
		return (FTP_DEFAULT_PROXY_PORT);
	if (strcasecmp(scheme, SCHEME_HTTP) == 0)
		return (HTTP_DEFAULT_PROXY_PORT);
	return (0);
}


/*
 * Create a connection for an existing descriptor.
 */
conn_t *
fetch_reopen(int sd)
{
	conn_t *conn;

	/* allocate and fill connection structure */
	if ((conn = calloc(1, sizeof(*conn))) == NULL)
		return (NULL);
	conn->ftp_home = NULL;
	conn->cache_url = NULL;
	conn->next_buf = NULL;
	conn->next_len = 0;
	conn->sd = sd;
	return (conn);
}


/*
 * Bind a socket to a specific local address
 */
int
fetch_bind(int sd, int af, const char *addr)
{
	struct addrinfo hints, *res, *res0;
	int rv = -1;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = af;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	if (getaddrinfo(addr, NULL, &hints, &res0))
		return (-1);
	for (res = res0; res; res = res->ai_next) {
		if (bind(sd, res->ai_addr, res->ai_addrlen) == 0) {
			rv = 0;
			break;
		}
	}
	freeaddrinfo(res0);
	return rv;
}

int
fetch_socks5(conn_t *conn, struct url *url, struct url *socks, int verbose)
{
	char buf[262];
	uint8_t auth;
	size_t alen;
	ssize_t dlen;

	alen = strlen(url->host);
	if (alen > 255) {
		if (verbose)
			fetch_info("socks5 only supports addresses <= 255 bytes");
		errno = EINVAL;
		return -1;
	}

	auth = (*socks->user != '\0' && *socks->pwd != '\0')
	    ? SOCKS5_USER_PASS : SOCKS5_NO_AUTH;

	buf[0] = SOCKS5_VERSION;
	buf[1] = 0x01; /* number of auth methods */
	buf[2] = auth;
	if (fetch_write(conn, buf, 3) != 3)
		return -1;

	if (fetch_read(conn, buf, 2) != 2)
		return -1;

	if (buf[0] != SOCKS5_VERSION) {
		if (verbose)
			fetch_info("socks5 version not recognized");
		errno = EINVAL;
		return -1;
	}

	if ((uint8_t)buf[1] == SOCKS5_NO_METHOD) {
		if (verbose)
			fetch_info("no acceptable socks5 authentication method");
		errno = EPERM;
		return -1;
	}

	switch (buf[1]) {
	case SOCKS5_USER_PASS:
		if (verbose)
			fetch_info("authenticate socks5 user '%s'", socks->user);
		buf[0] = SOCKS5_PASS_VERSION;
		buf[1] = strlen(socks->user);
		if (fetch_write(conn, buf, 2) != 2)
			return -1;
		if (fetch_write(conn, socks->user, buf[1]) == -1)
			return -1;

		buf[0] = strlen(socks->pwd);
		if (fetch_write(conn, buf, 1) != 1)
			return -1;
		if (fetch_write(conn, socks->pwd, buf[0]) == -1)
			return -1;

		if (fetch_read(conn, buf, 2) != 2)
			return -1;

		if (buf[0] != SOCKS5_PASS_VERSION) {
			if (verbose)
				fetch_info("socks5 password version not recognized");
			errno = EINVAL;
			return -1;
		}

		if (verbose)
			fetch_info("socks5 authentication response %d", buf[1]);

		if (buf[1] != SOCKS5_AUTH_SUCCESS) {
			if (verbose)
				fetch_info("socks5 authentication failed");
			errno = EPERM;
			return -1;
		}

		break;
	}

	if (verbose)
		fetch_info("connecting socks5 to %s:%d", url->host, url->port);

	/* write request */
	dlen = 0;
	buf[dlen++] = SOCKS5_VERSION;
	buf[dlen++] = SOCKS5_TCP_STREAM;
	buf[dlen++] = 0x00;
	buf[dlen++] = SOCKS5_ATYPE_DOMAIN;
	buf[dlen++] = alen;

	memcpy(&buf[dlen], url->host, alen);
	dlen += alen;

	buf[dlen++] = (url->port >> 0x08);
	buf[dlen++] = (url->port & 0xFF);

	if (fetch_write(conn, buf, dlen) != dlen)
		return -1;

	/* read answer */
	if (fetch_read(conn, buf, 4) != 4)
		return -1;

	if (buf[0] != SOCKS5_VERSION) {
		if (verbose)
			fetch_info("socks5 version not recognized");
		errno = EINVAL;
		return -1;
	}

	/* answer status */
	if (buf[1] != SOCKS5_REPLY_SUCCESS) {
		if (verbose)
			fetch_info("socks5 response status %d", buf[1]);
		switch (buf[1]) {
		case SOCKS5_REPLY_DENY: errno = EACCES; break;
		case SOCKS5_REPLY_NO_NET: errno = ENETUNREACH; break;
		case SOCKS5_REPLY_NO_HOST: errno = EHOSTUNREACH; break;
		case SOCKS5_REPLY_REFUSED: errno = ECONNREFUSED; break;
		case SOCKS5_REPLY_TIMEOUT: errno = ETIMEDOUT; break;
		case SOCKS5_REPLY_CMD_NOTSUP: errno = ENOTSUP; break;
		case SOCKS5_REPLY_ADR_NOTSUP: errno = ENOTSUP; break;
		}
		return -1;
	}

	switch (buf[3]) {
	case SOCKS5_ATYPE_IPV4:
		if (fetch_read(conn, buf, 4) != 4)
			return -1;
		break;
	case SOCKS5_ATYPE_DOMAIN:
		if (fetch_read(conn, buf, 1) != 1 &&
		    fetch_read(conn, buf, buf[0]) != buf[0])
			return -1;
		break;
	case SOCKS5_ATYPE_IPV6:
		if (fetch_read(conn, buf, 16) != 16)
			return -1;
		break;
	default:
		return -1;
	}

	// port
	if (fetch_read(conn, buf, 2) != 2)
		return -1;

	return 0;
}

static int
get_conn_timeout(void)
{
	static int result = -2;
	char *conn_timeout;

	if (result != -2) {
		return result;
	}

	conn_timeout = getenv("CONNECTION_TIMEOUT");
	if (conn_timeout) {
		char *char_read = conn_timeout;
		long from_env = strtol(conn_timeout, &char_read, 10);
		if (from_env < -1 || char_read == conn_timeout) {
			from_env = fetchConnTimeout;
		}
		result = from_env > INT_MAX ? INT_MAX: from_env;
	} else {
		result = fetchConnTimeout;
	}

	return result;
}

/*
 * Happy Eyeballs (RFC8305):
 *
 * Connect to the addresses in res0, alternating between
 * address family, starting with ipv6 and waits `fetchConnDelay`
 * between each connection attempt.
 *
 * If a connection is established within the attempts,
 * use this connection and close all others.
 *
 * If `connect(3)` returns `ENETUNREACH`, don't attempt more
 * connections with the failing address family.
 *
 * If there are no more addresses to attempt, wait for
 * CONNECTION_TIMEOUT milliseconds if given, where value
 * -1 means waiting for response indefinitely, else
 * `fetchConnTimeout` and return the first established
 * connection.
 *
 * If no connection was established within the timeouts,
 * close all sockets and return -1 and set errno to
 * `ETIMEDOUT`.
 */
#define UNREACH_IPV6 0x01
#define UNREACH_IPV4 0x10
static int
happy_eyeballs_connect(struct addrinfo *res0, int verbose)
{
	static int unreach = 0;
	int connTimeout = get_conn_timeout();
	struct pollfd *pfd;
	struct addrinfo *res;
	const char *bindaddr;
	int optval;
	socklen_t optlen = sizeof(optval);
	int rv = -1;
	int err = 0;
	int timeout = fetchConnDelay;
	unsigned int attempts = 0, waiting = 0;
	unsigned int i, n4, n6, i4, i6, done = 0;

	bindaddr = getenv("FETCH_BIND_ADDRESS");

	for (n4 = n6 = 0, res = res0; res; res = res->ai_next)
		switch (res->ai_family) {
		case AF_INET6: n6++; break;
		case AF_INET: n4++; break;
		}

#ifdef FULL_DEBUG
	fetch_info("got %d A and %d AAAA records", n4, n6);
#endif

	i4 = i6 = 0;
	if (unreach & UNREACH_IPV6 || getenv("FORCE_IPV4"))
		i6 = n6;
	if (unreach & UNREACH_IPV4 || getenv("FORCE_IPV6"))
		i4 = n4;

	if (n6+n4 == 0 || i6+i4 == n6+n4) {
		netdb_seterr(EAI_FAIL);
		return -1;
	}

	if (!(pfd = calloc(n4+n6, sizeof (struct pollfd)))) {
		fetch_syserr();
		return -1;
	}

	res = NULL;
	for (;;) {
		int sd = -1;
		int ret;
		unsigned short family = 0;

#ifdef FULL_DEBUG
		if (verbose)
		    fetch_info("happy eyeballs state: i4=%u n4=%u i6=%u n6=%u"
		        " attempts=%u waiting=%u", i4, n4, i6, n6, attempts, waiting);
#endif

		if (i6+i4 < n6+n4) {
			/* first round when res == NULL, prefer ipv6 */
			if (res == NULL || res->ai_family == AF_INET) {
				/* prefer ipv6 */
				if (i6 < n6)
					family = AF_INET6;
				else if (i4 < n4)
					family = AF_INET;
			} else {
				/* prefer ipv4 */
				if (i4 < n4)
					family = AF_INET;
				else if (i6 < n6)
					family = AF_INET6;
			}
		} else {
			timeout = connTimeout;
			/* no more connections to try */
			if (verbose)
				fetch_info("attempted to connect to all addresses, waiting...");
			done = 1;
			goto wait;
		}


		for (i = 0, res = res0; res; res = res->ai_next) {
			if (res->ai_family == family) {
				if (family == AF_INET && i == i4) {
					i4++;
					break;
				}
				if (family == AF_INET6 && i == i6) {
					i6++;
					break;
				}
				i++;
			}
		}
		if (res == NULL) {
			netdb_seterr(EAI_FAIL);
			goto out;
		}

		if ((sd = socket(res->ai_family, res->ai_socktype | SOCK_NONBLOCK,
		    res->ai_protocol)) == -1)
			continue;

		if (bindaddr != NULL && *bindaddr != '\0' &&
		    fetch_bind(sd, res->ai_family, bindaddr) != 0) {
			fetch_info("failed to bind to '%s'", bindaddr);
			close(sd);
			continue;
		}

		if (verbose) {
			char hbuf[1025];
			if (getnameinfo(res->ai_addr, res->ai_addrlen, hbuf, sizeof(hbuf), NULL,
			    0, NI_NUMERICHOST) == 0)
				fetch_info("connecting to %s", hbuf);
		}

		if (connect(sd, res->ai_addr, res->ai_addrlen) == -1) {
			if (errno == EINPROGRESS) {
				pfd[attempts].fd = sd;
			} else if (errno == ENETUNREACH) {
				close(sd);
				if (family == AF_INET) {
					i4 = n4;
					unreach |= UNREACH_IPV4;
				} else {
					i6 = n6;
					unreach |= UNREACH_IPV6;
				}
				continue;
			} else if (errno == EADDRNOTAVAIL || errno == EINVAL) {
				err = errno;
				close(sd);
				continue;
			} else {
				err = errno;
				rv = -1;
				close(sd);
				break;
			}
		} else {
			/* XXX: does this actually happen? */
			rv = sd;
			break;
		}

		attempts++;
		waiting++;
wait:
		if (!attempts) {
			netdb_seterr(EAI_FAIL);
			rv = -1;
			goto out;
		}
		for (i = 0; i < attempts; i++) {
			pfd[i].revents = pfd[i].events = 0;
			if (pfd[i].fd != -1)
				pfd[i].events = POLLOUT;
		}
		if (!waiting)
			break;
		if ((ret = poll(pfd, attempts, timeout ? timeout : -1)) == -1) {
			err = errno;
			rv = -1;
			break;
		} else if (ret > 0) {
			sd = -1;
			for (i = 0; i < attempts; i++) {
				if (pfd[i].revents & POLLHUP) {
					/* connection failed, save errno */
					if ((getsockopt(pfd[i].fd, SOL_SOCKET, SO_ERROR, &optval, &optlen)) == 0)
						err = optval;
					close(pfd[i].fd);
					pfd[i].fd = -1;
					waiting--;
				} else if (pfd[i].revents & POLLOUT) {
					/* connection established */
					err = 0;
					sd = pfd[i].fd;
					break;
				}
			}
			if (sd != -1) {
				rv = sd;
				break;
			}
		} else if (done) {
			err = ETIMEDOUT;
			rv = -1;
			break;
		}
	}

out:
	for (i = 0; i < attempts; i++)
		if ((rv == -1 || rv != pfd[i].fd) && pfd[i].fd != -1)
			close(pfd[i].fd);
	free(pfd);

	if (rv != -1) {
		if (fcntl(rv, F_SETFL, fcntl(rv, F_GETFL, 0) & ~O_NONBLOCK) == -1) {
			err = errno;
			close(rv);
			rv = -1;
		}
	}
	if ((errno = err))
		fetch_syserr();
	return rv;
}

/*
 * Establish a TCP connection to the specified port on the specified host.
 */
conn_t *
fetch_connect(struct url *url, int af, int verbose)
{
	conn_t *conn;
	char pbuf[10];
	struct url *socks_url = NULL, *connurl;
	const char *socks_proxy;
	struct addrinfo hints, *res0;
	int sd, error;

	socks_url = NULL;
	socks_proxy = getenv("SOCKS_PROXY");
	if (socks_proxy != NULL && *socks_proxy != '\0') {
		if (!(socks_url = fetchParseURL(socks_proxy)))
			return NULL;
		if (strcasecmp(socks_url->scheme, SCHEME_SOCKS5) != 0) {
			if (verbose)
				fetch_info("SOCKS_PROXY scheme '%s' not supported", socks_url->scheme);

			fetchFreeURL(socks_url);
			return NULL;
		}
		if (!socks_url->port)
			socks_url->port = fetch_default_port(socks_url->scheme);
		connurl = socks_url;
	} else {
		connurl = url;
	}

	if (verbose)
		fetch_info("looking up %s", connurl->host);

	/* look up host name and set up socket address structure */
	snprintf(pbuf, sizeof(pbuf), "%d", connurl->port);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = af;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	if ((error = getaddrinfo(connurl->host, pbuf, &hints, &res0)) != 0) {
		netdb_seterr(error);
		fetchFreeURL(socks_url);
		return (NULL);
	}

	if (verbose)
		fetch_info("connecting to %s:%d", connurl->host, connurl->port);

	sd = happy_eyeballs_connect(res0, verbose);
	freeaddrinfo(res0);
	if (sd == -1) {
		fetchFreeURL(socks_url);
		return (NULL);
	}
	if ((conn = fetch_reopen(sd)) == NULL) {
		fetchFreeURL(socks_url);
		fetch_syserr();
		close(sd);
		return NULL;
	}
	if (socks_url) {
		if (strcasecmp(socks_url->scheme, SCHEME_SOCKS5) == 0) {
			if (fetch_socks5(conn, url, socks_url, verbose) != 0) {
				fetchFreeURL(socks_url);
				fetch_syserr();
				close(sd);
				free(conn);
				return NULL;
			}
		}
		fetchFreeURL(socks_url);
	}
	conn->cache_url = fetchCopyURL(url);
	conn->cache_af = af;
	return (conn);
}

static pthread_mutex_t cache_mtx = PTHREAD_MUTEX_INITIALIZER;
static conn_t *connection_cache;
static int cache_global_limit = 0;
static int cache_per_host_limit = 0;

/*
 * Initialise cache with the given limits.
 */
void
fetchConnectionCacheInit(int global_limit, int per_host_limit)
{

	if (global_limit < 0)
		cache_global_limit = INT_MAX;
	else if (per_host_limit > global_limit)
		cache_global_limit = per_host_limit;
	else
		cache_global_limit = global_limit;
	if (per_host_limit < 0)
		cache_per_host_limit = INT_MAX;
	else
		cache_per_host_limit = per_host_limit;
}

/*
 * Flush cache and free all associated resources.
 */
void
fetchConnectionCacheClose(void)
{
	conn_t *conn;

	while ((conn = connection_cache) != NULL) {
		connection_cache = conn->next_cached;
		(*conn->cache_close)(conn);
	}
}

/*
 * Check connection cache for an existing entry matching
 * protocol/host/port/user/password/family.
 */
conn_t *
fetch_cache_get(const struct url *url, int af)
{
	conn_t *conn, *last_conn = NULL;

	pthread_mutex_lock(&cache_mtx);
	for (conn = connection_cache; conn; conn = conn->next_cached) {
		if (conn->cache_url->port == url->port &&
		    strcmp(conn->cache_url->scheme, url->scheme) == 0 &&
		    strcmp(conn->cache_url->host, url->host) == 0 &&
		    strcmp(conn->cache_url->user, url->user) == 0 &&
		    strcmp(conn->cache_url->pwd, url->pwd) == 0 &&
		    (conn->cache_af == AF_UNSPEC || af == AF_UNSPEC ||
		     conn->cache_af == af)) {
			if (last_conn != NULL)
				last_conn->next_cached = conn->next_cached;
			else
				connection_cache = conn->next_cached;

			pthread_mutex_unlock(&cache_mtx);
			return conn;
		}
	}
	pthread_mutex_unlock(&cache_mtx);

	return NULL;
}

/*
 * Put the connection back into the cache for reuse.
 * If the connection is freed due to LRU or if the cache
 * is explicitly closed, the given callback is called.
 */
void
fetch_cache_put(conn_t *conn, int (*closecb)(conn_t *))
{
	conn_t *iter, *last;
	int global_count, host_count;

	if (conn->cache_url == NULL || cache_global_limit == 0) {
		(*closecb)(conn);
		return;
	}

	pthread_mutex_lock(&cache_mtx);
	global_count = host_count = 0;
	last = NULL;
	for (iter = connection_cache; iter;
	    last = iter, iter = iter->next_cached) {
		++global_count;
		if (strcmp(conn->cache_url->host, iter->cache_url->host) == 0)
			++host_count;
		if (global_count < cache_global_limit &&
		    host_count < cache_per_host_limit)
			continue;
		--global_count;
		if (last != NULL)
			last->next_cached = iter->next_cached;
		else
			connection_cache = iter->next_cached;
		(*iter->cache_close)(iter);
	}

	conn->cache_close = closecb;
	conn->next_cached = connection_cache;
	connection_cache = conn;
	pthread_mutex_unlock(&cache_mtx);
}


#ifdef WITH_SSL

#ifndef HAVE_STRNSTR
/*
 * Find the first occurrence of find in s, where the search is limited to the
 * first slen characters of s.
 */
static char *
strnstr(const char *s, const char *find, size_t slen)
{
	char c, sc;
	size_t len;

	if ((c = *find++) != '\0') {
		len = strlen(find);
		do {
			do {
				if (slen-- < 1 || (sc = *s++) == '\0')
					return (NULL);
			} while (sc != c);
			if (len > slen)
				return (NULL);
		} while (strncmp(s, find, len) != 0);
		s--;
	}
	return ((char *)__UNCONST(s));
}
#endif

/*
 * Convert characters A-Z to lowercase (intentionally avoid any locale
 * specific conversions).
 */
static char
fetch_ssl_tolower(char in)
{
	if (in >= 'A' && in <= 'Z')
		return (in + 32);
	else
		return (in);
}

/*
 * isalpha implementation that intentionally avoids any locale specific
 * conversions.
 */
static int
fetch_ssl_isalpha(unsigned char in)
{
	return ((in >= 'A' && in <= 'Z') || (in >= 'a' && in <= 'z'));
}

/*
 * Check if passed hostnames a and b are equal.
 */
static int
fetch_ssl_hname_equal(const char *a, size_t alen, const char *b,
    size_t blen)
{
	size_t i;

	if (alen != blen)
		return (0);
	for (i = 0; i < alen; ++i) {
		if (fetch_ssl_tolower(a[i]) != fetch_ssl_tolower(b[i]))
			return (0);
	}
	return (1);
}

/*
 * Check if domain label is traditional, meaning that only A-Z, a-z, 0-9
 * and '-' (hyphen) are allowed. Hyphens have to be surrounded by alpha-
 * numeric characters. Double hyphens (like they're found in IDN a-labels
 * 'xn--') are not allowed. Empty labels are invalid.
 */
static int
fetch_ssl_is_trad_domain_label(const char *l, size_t len, int wcok)
{
	size_t i;

	if (!len || l[0] == '-' || l[len-1] == '-')
		return (0);
	for (i = 0; i < len; ++i) {
		if (!isdigit((unsigned char)l[i]) &&
		    !fetch_ssl_isalpha((unsigned char)l[i]) &&
		    !(l[i] == '*' && wcok) &&
		    !(l[i] == '-' && l[i - 1] != '-'))
			return (0);
	}
	return (1);
}

/*
 * Check if host name consists only of numbers. This might indicate an IP
 * address, which is not a good idea for CN wildcard comparison.
 */
static int
fetch_ssl_hname_is_only_numbers(const char *hostname, size_t len)
{
	size_t i;

	for (i = 0; i < len; ++i) {
		if (!((hostname[i] >= '0' && hostname[i] <= '9') ||
		    hostname[i] == '.'))
			return (0);
	}
	return (1);
}

/*
 * Check if the host name h passed matches the pattern passed in m which
 * is usually part of subjectAltName or CN of a certificate presented to
 * the client. This includes wildcard matching. The algorithm is based on
 * RFC6125, sections 6.4.3 and 7.2, which clarifies RFC2818 and RFC3280.
  */
static int
fetch_ssl_hname_match(const char *h, size_t hlen, const char *m,
    size_t mlen)
{
	int delta, hdotidx, mdot1idx, wcidx;
	const char *hdot, *mdot1, *mdot2;
	const char *wc; /* wildcard */

	if (!(h && *h && m && *m))
		return (0);
	if ((wc = strnstr(m, "*", mlen)) == NULL)
		return (fetch_ssl_hname_equal(h, hlen, m, mlen));
	wcidx = wc - m;
	/* hostname should not be just dots and numbers */
	if (fetch_ssl_hname_is_only_numbers(h, hlen))
		return (0);
	/* only one wildcard allowed in pattern */
	if (strnstr(wc + 1, "*", mlen - wcidx - 1) != NULL)
		return (0);
	/*
	 * there must be at least two more domain labels and
	 * wildcard has to be in the leftmost label (RFC6125)
	 */
	mdot1 = strnstr(m, ".", mlen);
	if (mdot1 == NULL || mdot1 < wc || (mlen - (mdot1 - m)) < 4)
		return (0);
	mdot1idx = mdot1 - m;
		mdot2 = strnstr(mdot1 + 1, ".", mlen - mdot1idx - 1);
	if (mdot2 == NULL || (mlen - (mdot2 - m)) < 2)
		return (0);
	/* hostname must contain a dot and not be the 1st char */
	hdot = strnstr(h, ".", hlen);
	if (hdot == NULL || hdot == h)
		return (0);
	hdotidx = hdot - h;
	/*
	 * host part of hostname must be at least as long as
	 * pattern it's supposed to match
	 */
	if (hdotidx < mdot1idx)
		return (0);
	/*
	 * don't allow wildcards in non-traditional domain names
	 * (IDN, A-label, U-label...)
	 */
	if (!fetch_ssl_is_trad_domain_label(h, hdotidx, 0) ||
	    !fetch_ssl_is_trad_domain_label(m, mdot1idx, 1))
		return (0);
	/* match domain part (part after first dot) */
	if (!fetch_ssl_hname_equal(hdot, hlen - hdotidx, mdot1,
	    mlen - mdot1idx))
		return (0);
	/* match part left of wildcard */
	if (!fetch_ssl_hname_equal(h, wcidx, m, wcidx))
		return (0);
	/* match part right of wildcard */
	delta = mdot1idx - wcidx - 1;
	if (!fetch_ssl_hname_equal(hdot - delta, delta,
	    mdot1 - delta, delta))
		return (0);
	/* all tests succeded, it's a match */
	return (1);
}

/*
 * Get numeric host address info - returns NULL if host was not an IP
 * address. The caller is responsible for deallocation using
 * freeaddrinfo(3).
 */
static struct addrinfo *
fetch_ssl_get_numeric_addrinfo(const char *hostname, size_t len)
{
	struct addrinfo hints, *res;
	char *host;

	host = malloc(len + 1);
	if (!host)
		return NULL;

	memcpy(host, hostname, len);
	host[len] = '\0';
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	hints.ai_flags = AI_NUMERICHOST;
	/* port is not relevant for this purpose */
	if (getaddrinfo(host, "443", &hints, &res) != 0) {
		free(host);
		return NULL;
	}
	free(host);
	return res;
}

/*
 * Compare ip address in addrinfo with address passes.
 */
static int
fetch_ssl_ipaddr_match_bin(const struct addrinfo *lhost, const char *rhost,
    size_t rhostlen)
{
	const void *left;

	if (lhost->ai_family == AF_INET && rhostlen == 4) {
		left = (void *)&((struct sockaddr_in*)(void *)
		lhost->ai_addr)->sin_addr.s_addr;
#ifdef INET6
	} else if (lhost->ai_family == AF_INET6 && rhostlen == 16) {
		left = (void *)&((struct sockaddr_in6 *)(void *)
		lhost->ai_addr)->sin6_addr;
#endif
	} else
		return (0);
	return (!memcmp(left, (const void *)rhost, rhostlen) ? 1 : 0);
}

/*
 * Compare ip address in addrinfo with host passed. If host is not an IP
 * address, comparison will fail.
 */
static int
fetch_ssl_ipaddr_match(const struct addrinfo *laddr, const char *r,
    size_t rlen)
{
	struct addrinfo *raddr;
	int ret;
	char *rip;

	ret = 0;
	if ((raddr = fetch_ssl_get_numeric_addrinfo(r, rlen)) == NULL)
		return 0; /* not a numeric host */

	if (laddr->ai_family == raddr->ai_family) {
		if (laddr->ai_family == AF_INET) {
			rip = (char *)&((struct sockaddr_in *)(void *)
			raddr->ai_addr)->sin_addr.s_addr;
			ret = fetch_ssl_ipaddr_match_bin(laddr, rip, 4);
#ifdef INET6
		} else if (laddr->ai_family == AF_INET6) {
			rip = (char *)&((struct sockaddr_in6 *)(void *)
			raddr->ai_addr)->sin6_addr;
			ret = fetch_ssl_ipaddr_match_bin(laddr, rip, 16);
#endif
		}
	}
	freeaddrinfo(raddr);
	return (ret);
}

/*
 * Verify server certificate by subjectAltName.
 */
static int
fetch_ssl_verify_altname(STACK_OF(GENERAL_NAME) *altnames,
    const char *host, struct addrinfo *ip)
{
	const GENERAL_NAME *name;
	size_t nslen;
	int i;
	const char *ns;

	for (i = 0; i < sk_GENERAL_NAME_num(altnames); ++i) {
		name = sk_GENERAL_NAME_value(altnames, i);
		ns = (const char *)ASN1_STRING_get0_data(name->d.ia5);
		nslen = (size_t)ASN1_STRING_length(name->d.ia5);

		if (name->type == GEN_DNS && ip == NULL &&
		    fetch_ssl_hname_match(host, strlen(host), ns, nslen))
			return (1);
		else if (name->type == GEN_IPADD && ip != NULL &&
		    fetch_ssl_ipaddr_match_bin(ip, ns, nslen))
			return (1);
	}
	return (0);
}

/*
 * Verify server certificate by CN.
 */
static int
fetch_ssl_verify_cn(X509_NAME *subject, const char *host,
    struct addrinfo *ip)
{
	ASN1_STRING *namedata;
	X509_NAME_ENTRY *nameentry;
	int cnlen, lastpos, loc, ret;
	unsigned char *cn;

	ret = 0;
	lastpos = -1;
	loc = -1;
	cn = NULL;
	/* get most specific CN (last entry in list) and compare */
	while ((lastpos = X509_NAME_get_index_by_NID(subject,
		    NID_commonName, lastpos)) != -1)
		loc = lastpos;

	if (loc > -1) {
		nameentry = X509_NAME_get_entry(subject, loc);
		namedata = X509_NAME_ENTRY_get_data(nameentry);
		cnlen = ASN1_STRING_to_UTF8(&cn, namedata);
		if (ip == NULL &&
		    fetch_ssl_hname_match(host, strlen(host), (const char *)cn, cnlen))
			ret = 1;
		else if (ip != NULL && fetch_ssl_ipaddr_match(ip, (const char *)cn, cnlen))
			ret = 1;
		OPENSSL_free(cn);
	}
	return (ret);
}

/*
 * Verify that server certificate subjectAltName/CN matches
 * hostname. First check, if there are alternative subject names. If yes,
 * those have to match. Only if those don't exist it falls back to
 * checking the subject's CN.
 */
static int
fetch_ssl_verify_hname(X509 *cert, const char *host)
{
	struct addrinfo *ip;
	STACK_OF(GENERAL_NAME) *altnames;
	X509_NAME *subject;
	int ret;

	ret = 0;
	ip = fetch_ssl_get_numeric_addrinfo(host, strlen(host));
	altnames = X509_get_ext_d2i(cert, NID_subject_alt_name,
				    NULL, NULL);

	if (altnames != NULL) {
		ret = fetch_ssl_verify_altname(altnames, host, ip);
	} else {
		subject = X509_get_subject_name(cert);
		if (subject != NULL)
			ret = fetch_ssl_verify_cn(subject, host, ip);
	}

	if (ip != NULL)
		freeaddrinfo(ip);
	if (altnames != NULL)
		GENERAL_NAMES_free(altnames);
	return (ret);
}

/*
 * Configure transport security layer based on environment.
 */
static void
fetch_ssl_setup_transport_layer(SSL_CTX *ctx, int verbose)
{
	long ssl_ctx_options;

	ssl_ctx_options = SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_TICKET;
	if (getenv("SSL_ALLOW_SSL3") == NULL)
		ssl_ctx_options |= SSL_OP_NO_SSLv3;
	if (getenv("SSL_NO_TLS1") != NULL)
		ssl_ctx_options |= SSL_OP_NO_TLSv1;
	if (getenv("SSL_NO_TLS1_1") != NULL)
		ssl_ctx_options |= SSL_OP_NO_TLSv1_1;
	if (getenv("SSL_NO_TLS1_2") != NULL)
		ssl_ctx_options |= SSL_OP_NO_TLSv1_2;
	if (verbose)
		fetch_info("SSL options: %lx", ssl_ctx_options);
	SSL_CTX_set_options(ctx, ssl_ctx_options);
}


/*
 * Configure peer verification based on environment.
 */
static int
fetch_ssl_setup_peer_verification(SSL_CTX *ctx, int verbose)
{
	X509_LOOKUP *crl_lookup;
	X509_STORE *crl_store;
	const char *ca_cert_file, *ca_cert_path, *crl_file;

	if (getenv("SSL_NO_VERIFY_PEER") == NULL) {
		ca_cert_file = getenv("SSL_CA_CERT_FILE");
		ca_cert_path = getenv("SSL_CA_CERT_PATH") != NULL ?
		    getenv("SSL_CA_CERT_PATH") : X509_get_default_cert_dir();
		if (verbose) {
			fetch_info("Peer verification enabled");
			if (ca_cert_file != NULL)
				fetch_info("Using CA cert file: %s",
				    ca_cert_file);
			if (ca_cert_path != NULL)
				fetch_info("Using CA cert path: %s",
				    ca_cert_path);
		}
		SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER,
		    fetch_ssl_cb_verify_crt);
		SSL_CTX_load_verify_locations(ctx, ca_cert_file,
		    ca_cert_path);
		if ((crl_file = getenv("SSL_CRL_FILE")) != NULL) {
			if (verbose)
				fetch_info("Using CRL file: %s", crl_file);

			crl_store = SSL_CTX_get_cert_store(ctx);
			crl_lookup = X509_STORE_add_lookup(crl_store,
			    X509_LOOKUP_file());
			if (crl_lookup == NULL ||
			    !X509_load_crl_file(crl_lookup, crl_file,
			    X509_FILETYPE_PEM)) {
				fprintf(stderr,
				    "Could not load CRL file %s\n",
				    crl_file);
				return (0);
			}
			X509_STORE_set_flags(crl_store,
			    X509_V_FLAG_CRL_CHECK |
			    X509_V_FLAG_CRL_CHECK_ALL);
		}
	}
	return (1);
}

/*
 * Configure client certificate based on environment.
 */
static int
fetch_ssl_setup_client_certificate(SSL_CTX *ctx, int verbose)
{
	const char *client_cert_file, *client_key_file;

	if ((client_cert_file = getenv("SSL_CLIENT_CERT_FILE")) != NULL) {
		client_key_file = getenv("SSL_CLIENT_KEY_FILE") != NULL ?
		    getenv("SSL_CLIENT_KEY_FILE") : client_cert_file;
		if (verbose) {
			fetch_info("Using client cert file: %s",
			    client_cert_file);
			fetch_info("Using client key file: %s",
			    client_key_file);
		}
		if (SSL_CTX_use_certificate_chain_file(ctx,
			client_cert_file) != 1) {
			fprintf(stderr,
			    "Could not load client certificate %s\n",
			    client_cert_file);
			return (0);
		}
		if (SSL_CTX_use_PrivateKey_file(ctx, client_key_file,
			SSL_FILETYPE_PEM) != 1) {
			fprintf(stderr,
			    "Could not load client key %s\n",
			    client_key_file);
			return (0);
		}
	}
	return (1);
}

/*
 * Callback for SSL certificate verification, this is called on server
 * cert verification. It takes no decision, but informs the user in case
 * verification failed.
 */
int
fetch_ssl_cb_verify_crt(int verified, X509_STORE_CTX *ctx)
{
	X509 *crt;
	X509_NAME *name;
	char *str;

	str = NULL;
	if (!verified) {
		if ((crt = X509_STORE_CTX_get_current_cert(ctx)) != NULL &&
		    (name = X509_get_subject_name(crt)) != NULL)
			str = X509_NAME_oneline(name, 0, 0);
		fprintf(stderr, "Certificate verification failed for %s\n",
		    str != NULL ? str : "no relevant certificate");
		OPENSSL_free(str);
	}
	return (verified);
}

static pthread_once_t ssl_init_once = PTHREAD_ONCE_INIT;

static void
ssl_init(void)
{
	/* Init the SSL library and context */
	SSL_load_error_strings();
	SSL_library_init();
}
#endif


/*
 * Enable SSL on a connection.
 */
int
fetch_ssl(conn_t *conn, const struct url *URL, int verbose)
{

#ifdef WITH_SSL
	int ret;
	X509_NAME *name;
	char *str;

	(void)pthread_once(&ssl_init_once, ssl_init);

	conn->ssl_ctx = SSL_CTX_new(SSLv23_client_method());
	if (conn->ssl_ctx == NULL) {
		fprintf(stderr, "failed to create SSL context\n");
		ERR_print_errors_fp(stderr);
		return -1;
	}
	SSL_CTX_set_mode(conn->ssl_ctx, SSL_MODE_AUTO_RETRY);

	fetch_ssl_setup_transport_layer(conn->ssl_ctx, verbose);
	if (!fetch_ssl_setup_peer_verification(conn->ssl_ctx, verbose))
		return (-1);
	if (!fetch_ssl_setup_client_certificate(conn->ssl_ctx, verbose))
		return (-1);

	conn->ssl = SSL_new(conn->ssl_ctx);
	if (conn->ssl == NULL) {
		fprintf(stderr, "SSL context creation failed\n");
		return (-1);
	}
	SSL_set_connect_state(conn->ssl);
	if (!SSL_set_fd(conn->ssl, conn->sd)) {
		fprintf(stderr, "SSL_set_fd failed\n");
		return (-1);
	}
#if OPENSSL_VERSION_NUMBER >= 0x0090806fL && !defined(OPENSSL_NO_TLSEXT)
	if (!SSL_set_tlsext_host_name(conn->ssl, (char *)(uintptr_t)URL->host)) {
		fprintf(stderr,
		    "TLS server name indication extension failed for host %s\n",
		    URL->host);
		return (-1);
	}
#endif
	if ((ret = SSL_connect(conn->ssl)) <= 0){
		fprintf(stderr, "SSL_connect returned %d\n", SSL_get_error(conn->ssl, ret));
		return (-1);
	}

	conn->ssl_cert = SSL_get_peer_certificate(conn->ssl);

	if (conn->ssl_cert == NULL) {
		fprintf(stderr, "No server SSL certificate\n");
		return (-1);
	}

        if (getenv("SSL_NO_VERIFY_HOSTNAME") == NULL) {
		if (verbose)
			fetch_info("Verify hostname");
		if (!fetch_ssl_verify_hname(conn->ssl_cert, URL->host)) {
			fprintf(stderr,
				"SSL certificate subject doesn't match host %s\n",
				URL->host);
			return (-1);
		}
	}

	if (verbose) {
		fetch_info("%s connection established using %s",
		    SSL_get_version(conn->ssl), SSL_get_cipher(conn->ssl));
		conn->ssl_cert = SSL_get_peer_certificate(conn->ssl);
		name = X509_get_subject_name(conn->ssl_cert);
		str = X509_NAME_oneline(name, 0, 0);
		fetch_info("Certificate subject: %s", str);
		OPENSSL_free(str);
		name = X509_get_issuer_name(conn->ssl_cert);
		str = X509_NAME_oneline(name, 0, 0);
		fetch_info("Certificate issuer: %s", str);
		OPENSSL_free(str);
	}

	return (0);
#else
	(void)conn;
	(void)verbose;
	fprintf(stderr, "SSL support disabled\n");
	return (-1);
#endif
}


/*
 * Read a character from a connection w/ timeout
 */
ssize_t
fetch_read(conn_t *conn, char *buf, size_t len)
{
	struct timeval now, timeout, waittv;
	fd_set readfds;
	ssize_t rlen;
	int r;

	if (len == 0)
		return 0;
	if (!buf)
		return -1;

	if (conn->next_len != 0) {
		if (conn->next_len < len)
			len = conn->next_len;
		memmove(buf, conn->next_buf, len);
		conn->next_len -= len;
		conn->next_buf += len;
		return len;
	}

	if (fetchTimeout) {
		FD_ZERO(&readfds);
		gettimeofday(&timeout, NULL);
		timeout.tv_sec += fetchTimeout;
	}

	for (;;) {
		while (fetchTimeout && !FD_ISSET(conn->sd, &readfds)) {
			FD_SET(conn->sd, &readfds);
			gettimeofday(&now, NULL);
			waittv.tv_sec = timeout.tv_sec - now.tv_sec;
			waittv.tv_usec = timeout.tv_usec - now.tv_usec;
			if (waittv.tv_usec < 0) {
				waittv.tv_usec += 1000000;
				waittv.tv_sec--;
			}
			if (waittv.tv_sec < 0) {
				errno = ETIMEDOUT;
				fetch_syserr();
				return (-1);
			}
			errno = 0;
#ifdef WITH_SSL
			if (conn->ssl && SSL_pending(conn->ssl))
				break;
#endif
			r = select(conn->sd + 1, &readfds, NULL, NULL, &waittv);
			if (r == -1) {
				if (errno == EINTR && fetchRestartCalls)
					continue;
				fetch_syserr();
				return (-1);
			}
		}
#ifdef WITH_SSL
		if (conn->ssl != NULL)
			rlen = SSL_read(conn->ssl, buf, len);
		else
#endif
			rlen = read(conn->sd, buf, len);
		if (rlen >= 0)
			break;

		if (errno != EINTR || !fetchRestartCalls)
			return (-1);
	}
	return (rlen);
}


/*
 * Read a line of text from a connection w/ timeout
 */
#define MIN_BUF_SIZE 1024

int
fetch_getln(conn_t *conn)
{
	char *tmp, *next;
	size_t tmpsize;
	ssize_t len;

	if (conn->buf == NULL) {
		if ((conn->buf = malloc(MIN_BUF_SIZE)) == NULL) {
			errno = ENOMEM;
			return (-1);
		}
		conn->bufsize = MIN_BUF_SIZE;
	}

	conn->buflen = 0;
	next = NULL;

	do {
		/*
		 * conn->bufsize != conn->buflen at this point,
		 * so the buffer can be NUL-terminated below for
		 * the case of len == 0.
		 */
		len = fetch_read(conn, conn->buf + conn->buflen,
		    conn->bufsize - conn->buflen);
		if (len == -1)
			return (-1);
		if (len == 0)
			break;
		next = memchr(conn->buf + conn->buflen, '\n', len);
		conn->buflen += len;
		if (conn->buflen == conn->bufsize && next == NULL) {
			tmp = conn->buf;
			tmpsize = conn->bufsize * 2;
			if (tmpsize < conn->bufsize) {
				errno = ENOMEM;
				return (-1);
			}
			if ((tmp = realloc(tmp, tmpsize)) == NULL) {
				errno = ENOMEM;
				return (-1);
			}
			conn->buf = tmp;
			conn->bufsize = tmpsize;
		}
	} while (next == NULL);

	if (next != NULL) {
		*next = '\0';
		conn->next_buf = next + 1;
		conn->next_len = conn->buflen - (conn->next_buf - conn->buf);
		conn->buflen = next - conn->buf;
	} else {
		conn->buf[conn->buflen] = '\0';
		conn->next_len = 0;
	}
	return (0);
}

/*
 * Write a vector to a connection w/ timeout
 * Note: can modify the iovec.
 */
ssize_t
fetch_write(conn_t *conn, const void *buf, size_t len)
{
	struct timeval now, timeout, waittv;
	fd_set writefds;
	ssize_t wlen, total;
	int r;
#ifndef MSG_NOSIGNAL
	static int killed_sigpipe;
#endif

#ifndef MSG_NOSIGNAL
	if (!killed_sigpipe) {
		signal(SIGPIPE, SIG_IGN);
		killed_sigpipe = 1;
	}
#endif


	if (fetchTimeout) {
		FD_ZERO(&writefds);
		gettimeofday(&timeout, NULL);
		timeout.tv_sec += fetchTimeout;
	}

	total = 0;
	while (len) {
		while (fetchTimeout && !FD_ISSET(conn->sd, &writefds)) {
			FD_SET(conn->sd, &writefds);
			gettimeofday(&now, NULL);
			waittv.tv_sec = timeout.tv_sec - now.tv_sec;
			waittv.tv_usec = timeout.tv_usec - now.tv_usec;
			if (waittv.tv_usec < 0) {
				waittv.tv_usec += 1000000;
				waittv.tv_sec--;
			}
			if (waittv.tv_sec < 0) {
				errno = ETIMEDOUT;
				fetch_syserr();
				return (-1);
			}
			errno = 0;
			r = select(conn->sd + 1, NULL, &writefds, NULL, &waittv);
			if (r == -1) {
				if (errno == EINTR && fetchRestartCalls)
					continue;
				return (-1);
			}
		}
		errno = 0;
#ifdef WITH_SSL
		if (conn->ssl != NULL)
			wlen = SSL_write(conn->ssl, buf, len);
		else
#endif
#ifndef MSG_NOSIGNAL
			wlen = send(conn->sd, buf, len, 0);
#else
			wlen = send(conn->sd, buf, len, MSG_NOSIGNAL);
#endif
		if (wlen == 0) {
			/* we consider a short write a failure */
			errno = EPIPE;
			fetch_syserr();
			return (-1);
		}
		if (wlen < 0) {
			if (errno == EINTR && fetchRestartCalls)
				continue;
			return (-1);
		}
		total += wlen;
		buf = (const char *)buf + wlen;
		len -= wlen;
	}
	return (total);
}


/*
 * Close connection
 */
int
fetch_close(conn_t *conn)
{
	int ret;

#ifdef WITH_SSL
	if (conn->ssl) {
		SSL_shutdown(conn->ssl);
		SSL_set_connect_state(conn->ssl);
		SSL_free(conn->ssl);
		conn->ssl = NULL;
	}
	if (conn->ssl_ctx) {
		SSL_CTX_free(conn->ssl_ctx);
		conn->ssl_ctx = NULL;
	}
	if (conn->ssl_cert) {
		X509_free(conn->ssl_cert);
		conn->ssl_cert = NULL;
	}
#endif
	ret = close(conn->sd);
	if (conn->cache_url)
		fetchFreeURL(conn->cache_url);
	free(conn->ftp_home);
	free(conn->buf);
	free(conn);
	return (ret);
}


/*** Directory-related utility functions *************************************/

int
fetch_add_entry(struct url_list *ue, struct url *base, const char *name,
    int pre_quoted)
{
	struct url *tmp;
	char *tmp_name;
	size_t base_doc_len, name_len, i;
	unsigned char c;

	if (strchr(name, '/') != NULL ||
	    strcmp(name, "..") == 0 ||
	    strcmp(name, ".") == 0)
		return 0;

	if (strcmp(base->doc, "/") == 0)
		base_doc_len = 0;
	else
		base_doc_len = strlen(base->doc);

	name_len = 1;
	for (i = 0; name[i] != '\0'; ++i) {
		if ((!pre_quoted && name[i] == '%') ||
		    !fetch_urlpath_safe(name[i]))
			name_len += 3;
		else
			++name_len;
	}

	tmp_name = malloc( base_doc_len + name_len + 1);
	if (tmp_name == NULL) {
		errno = ENOMEM;
		fetch_syserr();
		return (-1);
	}

	if (ue->length + 1 >= ue->alloc_size) {
		tmp = realloc(ue->urls, (ue->alloc_size * 2 + 1) * sizeof(*tmp));
		if (tmp == NULL) {
			free(tmp_name);
			errno = ENOMEM;
			fetch_syserr();
			return (-1);
		}
		ue->alloc_size = ue->alloc_size * 2 + 1;
		ue->urls = tmp;
	}

	tmp = ue->urls + ue->length;
	strcpy(tmp->scheme, base->scheme);
	strcpy(tmp->user, base->user);
	strcpy(tmp->pwd, base->pwd);
	strcpy(tmp->host, base->host);
	tmp->port = base->port;
	tmp->doc = tmp_name;
	memcpy(tmp->doc, base->doc, base_doc_len);
	tmp->doc[base_doc_len] = '/';

	for (i = base_doc_len + 1; *name != '\0'; ++name) {
		if ((!pre_quoted && *name == '%') ||
		    !fetch_urlpath_safe(*name)) {
			tmp->doc[i++] = '%';
			c = (unsigned char)*name / 16;
			if (c < 10)
				tmp->doc[i++] = '0' + c;
			else
				tmp->doc[i++] = 'a' - 10 + c;
			c = (unsigned char)*name % 16;
			if (c < 10)
				tmp->doc[i++] = '0' + c;
			else
				tmp->doc[i++] = 'a' - 10 + c;
		} else {
			tmp->doc[i++] = *name;
		}
	}
	tmp->doc[i] = '\0';

	tmp->offset = 0;
	tmp->length = 0;
	tmp->last_modified = -1;

	++ue->length;

	return (0);
}

void
fetchInitURLList(struct url_list *ue)
{
	ue->length = ue->alloc_size = 0;
	ue->urls = NULL;
}

int
fetchAppendURLList(struct url_list *dst, const struct url_list *src)
{
	size_t i, j, len;

	len = dst->length + src->length;
	if (len > dst->alloc_size) {
		struct url *tmp;

		tmp = realloc(dst->urls, len * sizeof(*tmp));
		if (tmp == NULL) {
			errno = ENOMEM;
			fetch_syserr();
			return (-1);
		}
		dst->alloc_size = len;
		dst->urls = tmp;
	}

	for (i = 0, j = dst->length; i < src->length; ++i, ++j) {
		dst->urls[j] = src->urls[i];
		dst->urls[j].doc = strdup(src->urls[i].doc);
		if (dst->urls[j].doc == NULL) {
			while (i-- > 0)
				free(dst->urls[j].doc);
			fetch_syserr();
			return -1;
		}
	}
	dst->length = len;

	return 0;
}

void
fetchFreeURLList(struct url_list *ue)
{
	size_t i;

	for (i = 0; i < ue->length; ++i)
		free(ue->urls[i].doc);
	free(ue->urls);
	ue->length = ue->alloc_size = 0;
}


/*** Authentication-related utility functions ********************************/

static const char *
fetch_read_word(FILE *f)
{
	static char word[1024];

	if (fscanf(f, " %1023s ", word) != 1)
		return (NULL);
	return (word);
}

/*
 * Get authentication data for a URL from .netrc
 */
int
fetch_netrc_auth(struct url *url)
{
	char fn[PATH_MAX];
	const char *word;
	char *p;
	FILE *f;

	if ((p = getenv("NETRC")) != NULL) {
		if (snprintf(fn, sizeof(fn), "%s", p) >= (int)sizeof(fn)) {
			fetch_info("$NETRC specifies a file name "
			    "longer than PATH_MAX");
			return (-1);
		}
	} else {
		if ((p = getenv("HOME")) != NULL) {
			struct passwd *pwd;

			if ((pwd = getpwuid(getuid())) == NULL ||
			    (p = pwd->pw_dir) == NULL)
				return (-1);
		}
		if (snprintf(fn, sizeof(fn), "%s/.netrc", p) >= (int)sizeof(fn))
			return (-1);
	}

	if ((f = fopen(fn, "r")) == NULL)
		return (-1);
	while ((word = fetch_read_word(f)) != NULL) {
		if (strcmp(word, "default") == 0)
			break;
		if (strcmp(word, "machine") == 0 &&
		    (word = fetch_read_word(f)) != NULL &&
		    strcasecmp(word, url->host) == 0) {
			break;
		}
	}
	if (word == NULL)
		goto ferr;
	while ((word = fetch_read_word(f)) != NULL) {
		if (strcmp(word, "login") == 0) {
			if ((word = fetch_read_word(f)) == NULL)
				goto ferr;
			if (snprintf(url->user, sizeof(url->user),
				"%s", word) > (int)sizeof(url->user)) {
				fetch_info("login name in .netrc is too long");
				url->user[0] = '\0';
			}
		} else if (strcmp(word, "password") == 0) {
			if ((word = fetch_read_word(f)) == NULL)
				goto ferr;
			if (snprintf(url->pwd, sizeof(url->pwd),
				"%s", word) > (int)sizeof(url->pwd)) {
				fetch_info("password in .netrc is too long");
				url->pwd[0] = '\0';
			}
		} else if (strcmp(word, "account") == 0) {
			if ((word = fetch_read_word(f)) == NULL)
				goto ferr;
			/* XXX not supported! */
		} else {
			break;
		}
	}
	fclose(f);
	return (0);
 ferr:
	fclose(f);
	return (-1);
}

/*
 * The no_proxy environment variable specifies a set of domains for
 * which the proxy should not be consulted; the contents is a comma-,
 * or space-separated list of domain names.  A single asterisk will
 * override all proxy variables and no transactions will be proxied
 * (for compatability with lynx and curl, see the discussion at
 * <http://curl.haxx.se/mail/archive_pre_oct_99/0009.html>).
 */
int
fetch_no_proxy_match(const char *host)
{
	const char *no_proxy, *p, *q;
	size_t h_len, d_len;

	if ((no_proxy = getenv("NO_PROXY")) == NULL &&
	    (no_proxy = getenv("no_proxy")) == NULL)
		return (0);

	/* asterisk matches any hostname */
	if (strcmp(no_proxy, "*") == 0)
		return (1);

	h_len = strlen(host);
	p = no_proxy;
	do {
		/* position p at the beginning of a domain suffix */
		while (*p == ',' || isspace((unsigned char)*p))
			p++;

		/* position q at the first separator character */
		for (q = p; *q; ++q)
			if (*q == ',' || isspace((unsigned char)*q))
				break;

		d_len = q - p;
		if (d_len > 0 && h_len > d_len &&
		    strncasecmp(host + h_len - d_len,
			p, d_len) == 0) {
			/* domain name matches */
			return (1);
		}

		p = q + 1;
	} while (*q);

	return (0);
}

struct fetchIO {
	void *io_cookie;
	ssize_t (*io_read)(void *, void *, size_t);
	ssize_t (*io_write)(void *, const void *, size_t);
	void (*io_close)(void *);
};

void
fetchIO_close(fetchIO *f)
{
	if (f->io_close != NULL)
		(*f->io_close)(f->io_cookie);

	free(f);
}

fetchIO *
fetchIO_unopen(void *io_cookie, ssize_t (*io_read)(void *, void *, size_t),
    ssize_t (*io_write)(void *, const void *, size_t),
    void (*io_close)(void *))
{
	fetchIO *f;

	f = malloc(sizeof(*f));
	if (f == NULL)
		return f;

	f->io_cookie = io_cookie;
	f->io_read = io_read;
	f->io_write = io_write;
	f->io_close = io_close;

	return f;
}

ssize_t
fetchIO_read(fetchIO *f, void *buf, size_t len)
{
	if (f->io_read == NULL)
		return EBADF;
	return (*f->io_read)(f->io_cookie, buf, len);
}

ssize_t
fetchIO_write(fetchIO *f, const void *buf, size_t len)
{
	if (f->io_read == NULL)
		return EBADF;
	return (*f->io_write)(f->io_cookie, buf, len);
}
