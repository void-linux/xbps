/*-
 * Copyright (c) 2014 Enno Boland.
 * Copyright (c) 2026 Duncan Overbruck <mail@duncano.de>.
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
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>

#include <atf-c.h>
#include <atf-c/macros.h>

#include <xbps.h>

static ssize_t PRINTF_LIKE(3, 0)
xsnprintf(char *dst, size_t dstsz, const char *fmt, ...)
{
	int l;
	va_list ap;
	va_start(ap, fmt);
	l = vsnprintf(dst, dstsz, fmt, ap);
	va_end(ap);
	if (l < 0 || (size_t)l >= dstsz)
		return -ENOBUFS;
	return l;
}

ATF_TC(config_include_test);
ATF_TC_HEAD(config_include_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test including files by file globbing");
}

ATF_TC_BODY(config_include_test, tc)
{
	char pwd[PATH_MAX];
	char buf1[PATH_MAX];
	char buf2[PATH_MAX];
	struct xbps_handle xh = {0};
	const char *tcsdir;

	tcsdir = atf_tc_get_config_var(tc, "srcdir");

	ATF_REQUIRE(getcwd(pwd, sizeof(pwd)));

	xbps_strlcpy(xh.rootdir, pwd, sizeof(xh.rootdir));
	xbps_strlcpy(xh.metadir, pwd, sizeof(xh.metadir));
	ATF_REQUIRE(xsnprintf(xh.confdir, sizeof(xh.confdir), "%s/xbps.d", pwd) >= 0);

	ATF_REQUIRE_EQ(xbps_mkpath(xh.confdir, 0755), 0);

	ATF_REQUIRE(xsnprintf(buf1, sizeof(buf1), "%s/xbps.cf", tcsdir) >= 0);
	ATF_REQUIRE(xsnprintf(buf2, sizeof(buf2), "%s/xbps.d/xbps.conf", pwd) >= 0);
	ATF_REQUIRE_EQ(symlink(buf1, buf2), 0);

	ATF_REQUIRE(xsnprintf(buf1, sizeof(buf1), "%s/1.include.cf", tcsdir) >= 0);
	ATF_REQUIRE(xsnprintf(buf2, sizeof(buf2), "%s/xbps.d/1.include.conf", pwd) >= 0);
	ATF_REQUIRE_EQ(symlink(buf1, buf2), 0);

	ATF_REQUIRE(xsnprintf(buf1, sizeof(buf1), "%s/2.include.cf", tcsdir) >= 0);
	ATF_REQUIRE(xsnprintf(buf2, sizeof(buf2), "%s/xbps.d/2.include.conf", pwd) >= 0);
	ATF_REQUIRE_EQ(symlink(buf1, buf2), 0);

	xh.flags = XBPS_FLAG_DEBUG;
	ATF_REQUIRE_EQ(xbps_init(&xh), 0);
	/* should contain both repositories defined in [12].include.conf */
	ATF_REQUIRE_EQ(xbps_array_count(xh.repositories), 2);
	xbps_end(&xh);
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
	ATF_REQUIRE(xsnprintf(xh.confdir, sizeof(xh.confdir), "%s/xbps.d", pwd) >= 0);

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
	xbps_end(&xh);
}

ATF_TC(config_include_absolute);
ATF_TC_HEAD(config_include_absolute, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test including files by absolute path");
}

ATF_TC_BODY(config_include_absolute, tc)
{
	char buf1[PATH_MAX];
	char buf2[PATH_MAX];
	char pwd[PATH_MAX];
	struct xbps_handle xh = {0};
	const char *tcsdir;

	tcsdir = atf_tc_get_config_var(tc, "srcdir");

	ATF_REQUIRE(getcwd(pwd, sizeof(pwd)));

	xbps_strlcpy(xh.rootdir, pwd, sizeof(xh.rootdir));
	xbps_strlcpy(xh.metadir, pwd, sizeof(xh.metadir));
	ATF_REQUIRE(xsnprintf(xh.confdir, sizeof(xh.confdir), "%s/xbps.d", pwd) >= 0);

	ATF_REQUIRE_EQ(xbps_mkpath(xh.confdir, 0755), 0);

	ATF_REQUIRE(xsnprintf(buf1, sizeof(buf1), "%s/xbps2.d", pwd) >= 0);
	ATF_REQUIRE_EQ(xbps_mkpath(buf1, 0755), 0);

	ATF_REQUIRE(xsnprintf(buf1, sizeof(buf1), "%s/xbps_absolute.cf", tcsdir) >= 0);
	ATF_REQUIRE(xsnprintf(buf2, sizeof(buf2), "%s/xbps.d/xbps.conf", pwd) >= 0);
	ATF_REQUIRE_EQ(symlink(buf1, buf2), 0);

	ATF_REQUIRE(xsnprintf(buf1, sizeof(buf1), "%s/1.include.cf", tcsdir) >= 0);
	ATF_REQUIRE(xsnprintf(buf2, sizeof(buf2), "%s/xbps2.d/1.include.conf", pwd) >= 0);
	ATF_REQUIRE_EQ(symlink(buf1, buf2), 0);

	xh.flags = XBPS_FLAG_DEBUG;
	ATF_REQUIRE_EQ(xbps_init(&xh), 0);
	/* should contain one repository defined in 1.include.conf */
	ATF_REQUIRE_EQ(xbps_array_count(xh.repositories), 1);
	xbps_end(&xh);
}

ATF_TC(config_include_absolute_glob);
ATF_TC_HEAD(config_include_absolute_glob, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test including files by absolute path with globbing");
}

ATF_TC_BODY(config_include_absolute_glob, tc)
{
	char pwd[PATH_MAX];
	char buf1[PATH_MAX];
	char buf2[PATH_MAX];
	struct xbps_handle xh = {0};
	const char *tcsdir;

	tcsdir = atf_tc_get_config_var(tc, "srcdir");

	ATF_REQUIRE(getcwd(pwd, sizeof(pwd)));

	xbps_strlcpy(xh.rootdir, pwd, sizeof(xh.rootdir));
	xbps_strlcpy(xh.metadir, pwd, sizeof(xh.metadir));
	ATF_REQUIRE(xsnprintf(xh.confdir, sizeof(xh.confdir), "%s/xbps.d", pwd) >= 0);

	ATF_REQUIRE_EQ(xbps_mkpath(xh.confdir, 0755), 0);

	ATF_REQUIRE(xsnprintf(buf1, sizeof(buf1), "%s/xbps2.d", pwd) >= 0);
	ATF_REQUIRE_EQ(xbps_mkpath(buf1, 0755), 0);

	ATF_REQUIRE(xsnprintf(buf1, sizeof(buf1), "%s/xbps_absolute_glob.cf", tcsdir) >= 0);
	ATF_REQUIRE(xsnprintf(buf2, sizeof(buf2), "%s/xbps.d/xbps.conf", pwd) >= 0);
	ATF_REQUIRE_EQ(symlink(buf1, buf2), 0);

	ATF_REQUIRE(xsnprintf(buf1, sizeof(buf1), "%s/1.include.cf", tcsdir) >= 0);
	ATF_REQUIRE(xsnprintf(buf2, sizeof(buf2), "%s/xbps2.d/1.include.conf", pwd) >= 0);
	ATF_REQUIRE_EQ(symlink(buf1, buf2), 0);

	ATF_REQUIRE(xsnprintf(buf1, sizeof(buf1), "%s/2.include.cf", tcsdir) >= 0);
	ATF_REQUIRE(xsnprintf(buf2, sizeof(buf2), "%s/xbps2.d/2.include.conf", pwd) >= 0);
	ATF_REQUIRE_EQ(symlink(buf1, buf2), 0);

	xh.flags = XBPS_FLAG_DEBUG;
	ATF_REQUIRE_EQ(xbps_init(&xh), 0);
	/* should contain both repositories defined in [12].include.conf */
	ATF_REQUIRE_EQ(xbps_array_count(xh.repositories), 2);
	xbps_end(&xh);
}

ATF_TC(config_masking);
ATF_TC_HEAD(config_masking, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test file masking");
}

ATF_TC_BODY(config_masking, tc)
{
	char pwd[PATH_MAX];
	char buf1[PATH_MAX];
	char buf2[PATH_MAX];
	struct xbps_handle xh = {0};
	const char *tcsdir, *repo;

	tcsdir = atf_tc_get_config_var(tc, "srcdir");

	ATF_REQUIRE(getcwd(pwd, sizeof(pwd)));

	xbps_strlcpy(xh.rootdir, tcsdir, sizeof(xh.rootdir));
	xbps_strlcpy(xh.metadir, tcsdir, sizeof(xh.metadir));
	ATF_REQUIRE(xsnprintf(xh.confdir, sizeof(xh.confdir), "%s/xbps.d", pwd) >= 0);
	ATF_REQUIRE(xsnprintf(xh.sysconfdir, sizeof(xh.sysconfdir), "%s/sys-xbps.d", pwd) >= 0);

	ATF_REQUIRE_EQ(xbps_mkpath(xh.confdir, 0755), 0);
	ATF_REQUIRE_EQ(xbps_mkpath(xh.sysconfdir, 0755), 0);

	ATF_REQUIRE(xsnprintf(buf1, sizeof(buf1), "%s/1.include.cf", tcsdir) >= 0);
	ATF_REQUIRE(xsnprintf(buf2, sizeof(buf2), "%s/xbps.d/repo.conf", pwd) >= 0);
	ATF_REQUIRE_EQ(symlink(buf1, buf2), 0);

	ATF_REQUIRE(xsnprintf(buf1, sizeof(buf1), "%s/2.include.cf", tcsdir) >= 0);
	ATF_REQUIRE(xsnprintf(buf2, sizeof(buf2), "%s/sys-xbps.d/repo.conf", pwd) >= 0);
	ATF_REQUIRE_EQ(symlink(buf1, buf2), 0);

	xh.flags = XBPS_FLAG_DEBUG;
	ATF_REQUIRE_EQ(xbps_init(&xh), 0);

	/* should contain one repository */
	ATF_REQUIRE_EQ(xbps_array_count(xh.repositories), 1);

	/* should contain repository=1 */
	ATF_REQUIRE_EQ(xbps_array_get_cstring_nocopy(xh.repositories, 0, &repo), true);
	ATF_REQUIRE_STREQ(repo, "1");
	xbps_end(&xh);
}

ATF_TC(config_trim_values);
ATF_TC_HEAD(config_trim_values, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test trimming of values");
}

ATF_TC_BODY(config_trim_values, tc)
{
	char pwd[PATH_MAX];
	char buf1[PATH_MAX];
	char buf2[PATH_MAX];
	struct xbps_handle xh = {0};
	const char *tcsdir, *repo;

	tcsdir = atf_tc_get_config_var(tc, "srcdir");

	ATF_REQUIRE(getcwd(pwd, sizeof(pwd)));

	xbps_strlcpy(xh.rootdir, tcsdir, sizeof(xh.rootdir));
	xbps_strlcpy(xh.metadir, tcsdir, sizeof(xh.metadir));
	ATF_REQUIRE(xsnprintf(xh.confdir, sizeof(xh.confdir), "%s/xbps.d", pwd) >= 0);
	ATF_REQUIRE(xsnprintf(xh.sysconfdir, sizeof(xh.sysconfdir), "%s/sys-xbps.d", pwd) >= 0);

	ATF_REQUIRE_EQ(xbps_mkpath(xh.confdir, 0755), 0);
	ATF_REQUIRE_EQ(xbps_mkpath(xh.sysconfdir, 0755), 0);

	ATF_REQUIRE(xsnprintf(buf1, sizeof(buf1), "%s/trim.cf", tcsdir) >= 0);
	ATF_REQUIRE(xsnprintf(buf2, sizeof(buf2), "%s/xbps.d/1.conf", pwd) >= 0);
	ATF_REQUIRE_EQ(symlink(buf1, buf2), 0);

	xh.flags = XBPS_FLAG_DEBUG;
	ATF_REQUIRE_EQ(xbps_init(&xh), 0);

	/* should contain one repository */
	ATF_REQUIRE_EQ(xbps_array_count(xh.repositories), 3);

	/* should contain repository=1 */
	ATF_REQUIRE_EQ(xbps_array_get_cstring_nocopy(xh.repositories, 0, &repo), true);
	ATF_REQUIRE_STREQ(repo, "1");
	/* should contain repository=2 */
	ATF_REQUIRE_EQ(xbps_array_get_cstring_nocopy(xh.repositories, 1, &repo), true);
	ATF_REQUIRE_STREQ(repo, "2");
	ATF_REQUIRE_EQ(xbps_array_get_cstring_nocopy(xh.repositories, 2, &repo), true);
	ATF_REQUIRE_STREQ(repo, "3");
	xbps_end(&xh);
}

ATF_TC(config_no_trailing_newline);
ATF_TC_HEAD(config_no_trailing_newline, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test configuration files without trailing newline");
}

ATF_TC_BODY(config_no_trailing_newline, tc)
{
	char pwd[PATH_MAX];
	char buf1[PATH_MAX];
	char buf2[PATH_MAX];
	struct xbps_handle xh = {0};
	const char *tcsdir, *repo;

	tcsdir = atf_tc_get_config_var(tc, "srcdir");

	ATF_REQUIRE(getcwd(pwd, sizeof(pwd)));

	xbps_strlcpy(xh.rootdir, tcsdir, sizeof(xh.rootdir));
	xbps_strlcpy(xh.metadir, tcsdir, sizeof(xh.metadir));
	ATF_REQUIRE(xsnprintf(xh.confdir, sizeof(xh.confdir), "%s/xbps.d", pwd) >= 0);
	ATF_REQUIRE(xsnprintf(xh.sysconfdir, sizeof(xh.sysconfdir), "%s/sys-xbps.d", pwd) >= 0);

	ATF_REQUIRE_EQ(xbps_mkpath(xh.confdir, 0755), 0);
	ATF_REQUIRE_EQ(xbps_mkpath(xh.sysconfdir, 0755), 0);

	ATF_REQUIRE(xsnprintf(buf1, sizeof(buf1), "%s/no-trailing-nl.cf", tcsdir) >= 0);
	ATF_REQUIRE(xsnprintf(buf2, sizeof(buf2), "%s/xbps.d/1.conf", pwd) >= 0);
	ATF_REQUIRE_EQ(symlink(buf1, buf2), 0);

	xh.flags = XBPS_FLAG_DEBUG;
	ATF_REQUIRE_EQ(xbps_init(&xh), 0);

	/* should contain one repository */
	ATF_REQUIRE_EQ(xbps_array_count(xh.repositories), 1);

	/* should contain repository=test */
	ATF_REQUIRE_EQ(xbps_array_get_cstring_nocopy(xh.repositories, 0, &repo), true);
	ATF_REQUIRE_STREQ(repo, "test");
	xbps_end(&xh);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, config_include_test);
	ATF_TP_ADD_TC(tp, config_include_nomatch_test);
	ATF_TP_ADD_TC(tp, config_include_absolute);
	ATF_TP_ADD_TC(tp, config_include_absolute_glob);
	ATF_TP_ADD_TC(tp, config_masking);
	ATF_TP_ADD_TC(tp, config_trim_values);
	ATF_TP_ADD_TC(tp, config_no_trailing_newline);

	return atf_no_error();
}
