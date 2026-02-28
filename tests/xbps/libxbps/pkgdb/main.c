/*-
 * Copyright (c) 2013 Juan Romero Pardines.
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

ATF_TC(pkgdb_get_pkg_test);
ATF_TC_HEAD(pkgdb_get_pkg_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_pkgdb_get_pkg()");
}

ATF_TC_BODY(pkgdb_get_pkg_test, tc)
{
	xbps_dictionary_t pkgd;
	struct xbps_handle xh;
	const char *tcsdir, *pkgver;

	/* get test source dir */
	tcsdir = atf_tc_get_config_var(tc, "srcdir");

	memset(&xh, 0, sizeof(xh));
	xbps_strlcpy(xh.rootdir, tcsdir, sizeof(xh.rootdir));
	xbps_strlcpy(xh.metadir, tcsdir, sizeof(xh.metadir));
	xh.flags = XBPS_FLAG_DEBUG;
	ATF_REQUIRE_EQ(xbps_init(&xh), 0);

	pkgd = xbps_pkgdb_get_pkg(&xh, "mixed");
	ATF_REQUIRE_EQ(xbps_object_type(pkgd), XBPS_TYPE_DICTIONARY);
	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	ATF_REQUIRE_STREQ(pkgver, "mixed-0.1_1");

	pkgd = xbps_pkgdb_get_pkg(&xh, "mixed>0");
	ATF_REQUIRE_EQ(xbps_object_type(pkgd), XBPS_TYPE_DICTIONARY);
	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	ATF_REQUIRE_STREQ(pkgver, "mixed-0.1_1");

	pkgd = xbps_pkgdb_get_pkg(&xh, "mixed<2");
	ATF_REQUIRE_EQ(xbps_object_type(pkgd), XBPS_TYPE_DICTIONARY);
	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	ATF_REQUIRE_STREQ(pkgver, "mixed-0.1_1");

	pkgd = xbps_pkgdb_get_pkg(&xh, "mixed-0.1_1");
	ATF_REQUIRE_EQ(xbps_object_type(pkgd), XBPS_TYPE_DICTIONARY);
	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	ATF_REQUIRE_STREQ(pkgver, "mixed-0.1_1");
	xbps_end(&xh);
}

ATF_TC(pkgdb_get_virtualpkg_test);
ATF_TC_HEAD(pkgdb_get_virtualpkg_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_pkgdb_get_virtualpkg()");
}

ATF_TC_BODY(pkgdb_get_virtualpkg_test, tc)
{
	xbps_dictionary_t pkgd;
	struct xbps_handle xh;
	const char *tcsdir, *pkgver;

	/* get test source dir */
	tcsdir = atf_tc_get_config_var(tc, "srcdir");

	memset(&xh, 0, sizeof(xh));
	xbps_strlcpy(xh.rootdir, tcsdir, sizeof(xh.rootdir));
	xbps_strlcpy(xh.metadir, tcsdir, sizeof(xh.metadir));
	xh.flags = XBPS_FLAG_DEBUG;
	ATF_REQUIRE_EQ(xbps_init(&xh), 0);

	pkgd = xbps_pkgdb_get_virtualpkg(&xh, "mixed");
	ATF_REQUIRE_EQ(xbps_object_type(pkgd), XBPS_TYPE_DICTIONARY);
	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	ATF_REQUIRE_STREQ(pkgver, "virtual-mixed-0.1_1");

	pkgd = xbps_pkgdb_get_virtualpkg(&xh, "mixed>0");
	ATF_REQUIRE_EQ(xbps_object_type(pkgd), XBPS_TYPE_DICTIONARY);
	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	ATF_REQUIRE_STREQ(pkgver, "virtual-mixed-0.1_1");

	pkgd = xbps_pkgdb_get_virtualpkg(&xh, "mixed<2");
	ATF_REQUIRE_EQ(xbps_object_type(pkgd), XBPS_TYPE_DICTIONARY);
	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	ATF_REQUIRE_STREQ(pkgver, "virtual-mixed-0.1_1");

	pkgd = xbps_pkgdb_get_virtualpkg(&xh, "mixed-0.1_1");
	ATF_REQUIRE_EQ(xbps_object_type(pkgd), XBPS_TYPE_DICTIONARY);
	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	ATF_REQUIRE_STREQ(pkgver, "virtual-mixed-0.1_1");
	xbps_end(&xh);
}

ATF_TC(pkgdb_get_pkg_revdeps_test);
ATF_TC_HEAD(pkgdb_get_pkg_revdeps_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_pkgdb_get_pkg_revdeps()");
}

ATF_TC_BODY(pkgdb_get_pkg_revdeps_test, tc)
{
	struct xbps_handle xh;
	xbps_array_t res;
	xbps_string_t pstr;
	const char *tcsdir, *str;
	const char *eout = "four-0.1_1\ntwo-0.1_1\n";
	unsigned int i;

	/* get test source dir */
	tcsdir = atf_tc_get_config_var(tc, "srcdir");

	memset(&xh, 0, sizeof(xh));
	xbps_strlcpy(xh.rootdir, tcsdir, sizeof(xh.rootdir));
	xbps_strlcpy(xh.metadir, tcsdir, sizeof(xh.metadir));
	xh.flags = XBPS_FLAG_DEBUG;
	ATF_REQUIRE_EQ(xbps_init(&xh), 0);

	res = xbps_pkgdb_get_pkg_revdeps(&xh, "virtual-mixed");
	ATF_REQUIRE_EQ(xbps_object_type(res), XBPS_TYPE_ARRAY);

	pstr = xbps_string_create();
	for (i = 0; i < xbps_array_count(res); i++) {
		xbps_array_get_cstring_nocopy(res, i, &str);
		xbps_string_append_cstring(pstr, str);
		xbps_string_append_cstring(pstr, "\n");
	}
	ATF_REQUIRE_STREQ(xbps_string_cstring_nocopy(pstr), eout);
	xbps_object_release(pstr);
	xbps_end(&xh);
}

ATF_TC(pkgdb_pkg_reverts_test);
ATF_TC_HEAD(pkgdb_pkg_reverts_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_pkg_reverts()");
}

ATF_TC_BODY(pkgdb_pkg_reverts_test, tc)
{
	struct xbps_handle xh;
	const char *tcsdir;
	xbps_dictionary_t pkgd;

	/* get test source dir */
	tcsdir = atf_tc_get_config_var(tc, "srcdir");

	memset(&xh, 0, sizeof(xh));
	xbps_strlcpy(xh.rootdir, tcsdir, sizeof(xh.rootdir));
	xbps_strlcpy(xh.metadir, tcsdir, sizeof(xh.metadir));
	xh.flags = XBPS_FLAG_DEBUG;
	ATF_REQUIRE_EQ(xbps_init(&xh), 0);

	pkgd = xbps_pkgdb_get_pkg(&xh, "reverts");
	ATF_REQUIRE_EQ(xbps_object_type(pkgd), XBPS_TYPE_DICTIONARY);

	ATF_REQUIRE_EQ(xbps_pkg_reverts(pkgd, "reverts-0.2_1"), 0);
	ATF_REQUIRE_EQ(xbps_pkg_reverts(pkgd, "reverts-0.3_1"), 1);
	ATF_REQUIRE_EQ(xbps_pkg_reverts(pkgd, "reverts-0.4_1"), 1);
	ATF_REQUIRE_EQ(xbps_pkg_reverts(pkgd, "reverts-0.5_1"), 0);
	xbps_end(&xh);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, pkgdb_get_pkg_test);
	ATF_TP_ADD_TC(tp, pkgdb_get_virtualpkg_test);
	ATF_TP_ADD_TC(tp, pkgdb_get_pkg_revdeps_test);
	ATF_TP_ADD_TC(tp, pkgdb_pkg_reverts_test);

	return atf_no_error();
}
