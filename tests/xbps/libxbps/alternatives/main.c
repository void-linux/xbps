/*-
 * Copyright (c) 2021 Duncan Overbruck <mail@duncano.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *-
 */
#include <string.h>
#include <limits.h>
#include <errno.h>

#include <atf-c.h>
#include <xbps.h>

ATF_TC(xbps_alternative_link);

ATF_TC_HEAD(xbps_alternative_link, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_alternative_link");
}

ATF_TC_BODY(xbps_alternative_link, tc)
{
	char path[PATH_MAX];
	char target[PATH_MAX];
	char small[3];
	ssize_t rv;

#define TEST(a, b, c)                \
	xbps_alternative_link(a,     \
	    path, sizeof(path),      \
	    target, sizeof(target)); \
	ATF_CHECK_STREQ(path, b);    \
	ATF_CHECK_STREQ(target, c);

	TEST("tar:/usr/bin/bsdtar", "/usr/bin/tar", "bsdtar");
	TEST("whois.1:/usr/share/man/man1/gwhois.1",
	    "/usr/share/man/man1/whois.1", "gwhois.1");
	TEST("/usr/bin/unpack200:/usr/lib/jvm/openjdk11/bin/unpack200",
	    "/usr/bin/unpack200", "../lib/jvm/openjdk11/bin/unpack200");

	
	/* invalid alternative */
	ATF_CHECK_EQ(xbps_alternative_link("foo", NULL, 0, NULL, 0), -EINVAL);

	/* invalid alternative, no dir */
	rv = xbps_alternative_link("x:y",
	    path, sizeof(path),
	    NULL, 0);
	ATF_CHECK_EQ(rv, -EINVAL);

	/* path buffer too small */
	rv = xbps_alternative_link("foo:/usr/bin/bar",
	    small, sizeof(small),
	    NULL, 0);
	ATF_CHECK_EQ(rv, -ENOBUFS);

	/* target buffer too small */
	rv = xbps_alternative_link("foo:/usr/bin/bar",
	    path, sizeof(path),
	    small, sizeof(small));
	ATF_CHECK_EQ(rv, -ENOBUFS);

	/* path fits exactly */
	rv = xbps_alternative_link("x:/y",
	    small, sizeof(small),
	    NULL, 0);
	ATF_CHECK_EQ(rv, 0);

	/* target fits exactly */
	rv = xbps_alternative_link("xx:/usr/bin/yy",
	    path, sizeof(path),
	    small, sizeof(small));
	ATF_CHECK_EQ(rv, 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, xbps_alternative_link);
	return atf_no_error();
}
