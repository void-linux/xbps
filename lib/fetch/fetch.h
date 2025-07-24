/*	$NetBSD: fetch.h,v 1.16 2010/01/22 13:21:09 joerg Exp $	*/
/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1998-2014 Dag-Erling Smørgrav
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
 *
 * $FreeBSD: head/lib/libfetch/fetch.h 326219 2017-11-26 02:00:33Z pfg $
 */

#ifndef _FETCH_H_INCLUDED
#define _FETCH_H_INCLUDED

#include <sys/types.h>
#include <limits.h>
#include <stdio.h>

#define _LIBFETCH_VER "xbps/2.0"

#define URL_HOSTLEN 255
#define URL_SCHEMELEN 16
#define URL_USERLEN 256
#define URL_PWDLEN 256

typedef struct fetchIO fetchIO;

struct url {
	char		 scheme[URL_SCHEMELEN+1];
	char		 user[URL_USERLEN+1];
	char		 pwd[URL_PWDLEN+1];
	char		 host[URL_HOSTLEN+1];
	int		 port;
	char		*doc;
	off_t		 offset;
	size_t		 length;
	time_t		 ims_time;
	int		 netrcfd;
};

struct url_stat {
	off_t		 size;
	time_t		 atime;
	time_t		 mtime;
};

/* Recognized schemes */
#define SCHEME_FTP	"ftp"
#define SCHEME_HTTP	"http"
#define SCHEME_HTTPS	"https"
#define SCHEME_FILE	"file"
#define SCHEME_SOCKS5	"socks5"

/* Error codes */
#define	FETCH_ABORT	 1
#define	FETCH_AUTH	 2
#define	FETCH_DOWN	 3
#define	FETCH_EXISTS	 4
#define	FETCH_FULL	 5
#define	FETCH_INFO	 6
#define	FETCH_MEMORY	 7
#define	FETCH_MOVED	 8
#define	FETCH_NETWORK	 9
#define	FETCH_OK	10
#define	FETCH_PROTO	11
#define	FETCH_RESOLV	12
#define	FETCH_SERVER	13
#define	FETCH_TEMP	14
#define	FETCH_TIMEOUT	15
#define	FETCH_UNAVAIL	16
#define	FETCH_UNKNOWN	17
#define	FETCH_URL	18
#define	FETCH_VERBOSE	19
#define	FETCH_UNCHANGED	20

#if defined(__cplusplus)
extern "C" {
#endif

int		fetchIO_close(fetchIO *);
ssize_t		fetchIO_read(fetchIO *, void *, size_t);
ssize_t		fetchIO_write(fetchIO *, const void *, size_t);

/* fetchIO-specific functions */
fetchIO		*fetchXGetFile(struct url *, struct url_stat *, const char *);
fetchIO		*fetchGetFile(struct url *, const char *);
fetchIO		*fetchPutFile(struct url *, const char *);
int		 fetchStatFile(struct url *, struct url_stat *, const char *);
struct url_ent	*fetchListFile(struct url *, const char *);

/* HTTP-specific functions */
fetchIO		*fetchXGetHTTP(struct url *, struct url_stat *, const char *);
fetchIO		*fetchGetHTTP(struct url *, const char *);
fetchIO		*fetchPutHTTP(struct url *, const char *);
int		 fetchStatHTTP(struct url *, struct url_stat *, const char *);
struct url_ent	*fetchListHTTP(struct url *, const char *);
fetchIO		*fetchReqHTTP(struct url *, const char *, const char *,
		    const char *, const char *);

/* FTP-specific functions */
fetchIO		*fetchXGetFTP(struct url *, struct url_stat *, const char *);
fetchIO		*fetchGetFTP(struct url *, const char *);
fetchIO		*fetchPutFTP(struct url *, const char *);
int		 fetchStatFTP(struct url *, struct url_stat *, const char *);
struct url_ent	*fetchListFTP(struct url *, const char *);

/* Generic functions */
fetchIO		*fetchXGetURL(const char *, struct url_stat *, const char *);
fetchIO		*fetchGetURL(const char *, const char *);
fetchIO		*fetchPutURL(const char *, const char *);
int		 fetchStatURL(const char *, struct url_stat *, const char *);
fetchIO		*fetchXGet(struct url *, struct url_stat *, const char *);
fetchIO		*fetchGet(struct url *, const char *);
fetchIO		*fetchPut(struct url *, const char *);
int		 fetchStat(struct url *, struct url_stat *, const char *);
struct url_ent	*fetchList(struct url *, const char *);

/* URL parsing */
struct url	*fetchMakeURL(const char *, const char *, int,
		     const char *, const char *, const char *);
struct url	*fetchParseURL(const char *);
struct url	*fetchDupURL(struct url *);
char        *fetchStringifyURL(const struct url *);
void		 fetchFreeURL(struct url *);
char         *fetchUnquotePath(struct url *);
char         *fetchUnquoteFilename(struct url *);

/* Connection caching */
void fetchConnectionCacheInit(int, int);
void fetchConnectionCacheClose(void);

/* Authentication */
typedef int (*auth_t)(struct url *);
extern auth_t		 fetchAuthMethod;

/* Last error code */
extern int		 fetchLastErrCode;
#define MAXERRSTRING 256
extern char		 fetchLastErrString[MAXERRSTRING];

/* I/O timeout */
extern int		 fetchTimeout;

/* Connect timeout */
extern int		 fetchConnTimeout;

/* Connect attempt delay  */
extern int		 fetchConnDelay;

/* Restart interrupted syscalls */
extern volatile int	 fetchRestartCalls;

/* Extra verbosity */
extern int		 fetchDebug;

#if defined(__cplusplus)
}
#endif

#endif
