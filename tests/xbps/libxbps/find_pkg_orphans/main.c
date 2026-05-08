/*-
 * Copyright (c) 2013-2015 Juan Romero Pardines.
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

static const char expected_output[] =
	"xbps-git-20130310_2\n"
	"libxbps-git-20130310_2\n"
	"confuse-2.7_2\n"
	"libarchive-3.1.2_1\n"
	"bzip2-1.0.5_1\n"
	"liblzma-5.0.4_3\n"
	"expat-2.1.0_3\n"
	"attr-2.4.46_5\n"
	"proplib-0.6.3_1\n"
	"libfetch-2.34_1\n"
	"libssl-1.0.1e_3\n"
	"zlib-1.2.7_1\n"
	"glibc-2.20_1\n"
	"xbps-triggers-1.0_1\n";

static const char expected_output_all[] =
	"orphan2-0_1\n"
	"unexistent-pkg-0_1\n"
	"orphan1-0_1\n"
	"orphan0-0_1\n";

ATF_TC(find_pkg_orphans_test);
ATF_TC_HEAD(find_pkg_orphans_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_find_pkg_orphans() for target pkg");
}

ATF_TC_BODY(find_pkg_orphans_test, tc)
{
	struct xbps_handle xh;
	xbps_array_t a, res;
	xbps_dictionary_t pkgd;
	xbps_string_t pstr;
	const char *pkgver, *tcsdir;
	unsigned int i;

	/* get test source dir */
	tcsdir = atf_tc_get_config_var(tc, "srcdir");

	memset(&xh, 0, sizeof(xh));
	xbps_strlcpy(xh.rootdir, tcsdir, sizeof(xh.rootdir));
	xbps_strlcpy(xh.metadir, tcsdir, sizeof(xh.metadir));
	ATF_REQUIRE_EQ(xbps_init(&xh), 0);

	ATF_REQUIRE((a = xbps_array_create()));
	ATF_REQUIRE(xbps_array_add_cstring_nocopy(a, "xbps-git"));

	pstr = xbps_string_create();
	res = xbps_find_pkg_orphans(&xh, a);
	for (i = 0; i < xbps_array_count(res); i++) {
		pkgd = xbps_array_get(res, i);
		xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
		xbps_string_append_cstring(pstr, pkgver);
		xbps_string_append_cstring(pstr, "\n");
	}
	xbps_object_release(a);
	xbps_object_release(res);
	ATF_REQUIRE_STREQ(xbps_string_cstring_nocopy(pstr), expected_output);
	xbps_object_release(pstr);
	xbps_end(&xh);
}

ATF_TC(find_all_orphans_test);
ATF_TC_HEAD(find_all_orphans_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_find_pkg_orphans() for all pkgs");
}

ATF_TC_BODY(find_all_orphans_test, tc)
{
	struct xbps_handle xh;
	xbps_array_t res;
	xbps_dictionary_t pkgd;
	xbps_string_t pstr;
	const char *pkgver, *tcsdir;
	unsigned int i;

	/* get test source dir */
	tcsdir = atf_tc_get_config_var(tc, "srcdir");

	memset(&xh, 0, sizeof(xh));
	xbps_strlcpy(xh.rootdir, tcsdir, sizeof(xh.rootdir));
	xbps_strlcpy(xh.metadir, tcsdir, sizeof(xh.metadir));
	ATF_REQUIRE_EQ(xbps_init(&xh), 0);

	pstr = xbps_string_create();
	res = xbps_find_pkg_orphans(&xh, NULL);
	for (i = 0; i < xbps_array_count(res); i++) {
		pkgd = xbps_array_get(res, i);
		ATF_REQUIRE(xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver));
		ATF_REQUIRE(xbps_string_append_cstring(pstr, pkgver));
		ATF_REQUIRE(xbps_string_append_cstring(pstr, "\n"));
	}
	printf("%s", xbps_string_cstring_nocopy(pstr));
	xbps_object_release(res);

	ATF_REQUIRE_STREQ(xbps_string_cstring_nocopy(pstr), expected_output_all);
	xbps_object_release(pstr);
	xbps_end(&xh);
}

ATF_TP_ADD_TCS(tp)
{
	/* First test: find orphans for xbps-git pkg */
	ATF_TP_ADD_TC(tp, find_pkg_orphans_test);
	/* Second test: find all orphans */
	ATF_TP_ADD_TC(tp, find_all_orphans_test);

	return atf_no_error();
}
