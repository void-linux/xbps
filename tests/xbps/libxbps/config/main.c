/*-
 * Copyright (c) 2014 Enno Boland.
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
#include <atf-c.h>
#include <xbps.h>
#include <stdlib.h>
#include <limits.h>

ATF_TC(config_include_test);
ATF_TC_HEAD(config_include_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test including files by file globbing");
}

ATF_TC_BODY(config_include_test, tc)
{
	struct xbps_handle xh;
	const char *tcsdir;
	char *buf, *buf2, pwd[PATH_MAX];

	/* get test source dir */
	tcsdir = atf_tc_get_config_var(tc, "srcdir");

	memset(&xh, 0, sizeof(xh));
	buf = getcwd(pwd, sizeof(pwd));

	xbps_strlcpy(xh.rootdir, pwd, sizeof(xh.rootdir));
	xbps_strlcpy(xh.metadir, pwd, sizeof(xh.metadir));
	snprintf(xh.confdir, sizeof(xh.confdir), "%s/xbps.d", pwd);

	ATF_REQUIRE_EQ(xbps_mkpath(xh.confdir, 0755), 0);

	buf = xbps_xasprintf("%s/xbps.cf", tcsdir);
	buf2 = xbps_xasprintf("%s/xbps.d/xbps.conf", pwd);
	ATF_REQUIRE_EQ(symlink(buf, buf2), 0);
	free(buf);
	free(buf2);

	buf = xbps_xasprintf("%s/1.include.cf", tcsdir);
	buf2 = xbps_xasprintf("%s/xbps.d/1.include.conf", pwd);
	ATF_REQUIRE_EQ(symlink(buf, buf2), 0);
	free(buf);
	free(buf2);

	buf = xbps_xasprintf("%s/2.include.cf", tcsdir);
	buf2 = xbps_xasprintf("%s/xbps.d/2.include.conf", pwd);
	ATF_REQUIRE_EQ(symlink(buf, buf2), 0);
	free(buf);
	free(buf2);

	xh.flags = XBPS_FLAG_DEBUG;
	ATF_REQUIRE_EQ(xbps_init(&xh), 0);
	/* should contain both repositories defined in [12].include.conf */
	ATF_REQUIRE_EQ(xbps_array_count(xh.repositories), 2);
}


ATF_TC(config_include_nomatch_test);
ATF_TC_HEAD(config_include_nomatch_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test finds no files to include");
}

ATF_TC_BODY(config_include_nomatch_test, tc)
{
	struct xbps_handle xh;
	const char *tcsdir;
	char *buf, *buf2, pwd[PATH_MAX];

	/* get test source dir */
	tcsdir = atf_tc_get_config_var(tc, "srcdir");

	memset(&xh, 0, sizeof(xh));
	buf = getcwd(pwd, sizeof(pwd));

	xbps_strlcpy(xh.rootdir, tcsdir, sizeof(xh.rootdir));
	xbps_strlcpy(xh.metadir, tcsdir, sizeof(xh.metadir));
	snprintf(xh.confdir, sizeof(xh.confdir), "%s/xbps.d", pwd);

	ATF_REQUIRE_EQ(xbps_mkpath(xh.confdir, 0755), 0);

	buf = xbps_xasprintf("%s/xbps_nomatch.cf", tcsdir);
	buf2 = xbps_xasprintf("%s/xbps.d/nomatch.conf", pwd);
	ATF_REQUIRE_EQ(symlink(buf, buf2), 0);
	free(buf);
	free(buf2);

	xh.flags = XBPS_FLAG_DEBUG;
	ATF_REQUIRE_EQ(xbps_init(&xh), 0);

	/* should contain no repositories */
	ATF_REQUIRE_EQ(xbps_array_count(xh.repositories), 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, config_include_test);
	ATF_TP_ADD_TC(tp, config_include_nomatch_test);

	return atf_no_error();
}
