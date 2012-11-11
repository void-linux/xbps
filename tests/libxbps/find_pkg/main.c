/*-
 * Copyright (c) 2012 Juan Romero Pardines.
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
#include <atf-c.h>
#include <xbps_api.h>

ATF_TC(find_pkg_dict_installed_test);
ATF_TC_HEAD(find_pkg_dict_installed_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_find_pkg_dict_installed");
}
ATF_TC_BODY(find_pkg_dict_installed_test, tc)
{
	struct xbps_handle xh;
	prop_dictionary_t dr;
	const char *pkgver, *tcsdir;

	/* get test source dir */
	tcsdir = atf_tc_get_config_var(tc, "srcdir");

	/* initialize xbps */
	memset(&xh, 0, sizeof(xh));
	xh.rootdir = "/tmp";
	xh.metadir = tcsdir;
	ATF_REQUIRE_EQ(xbps_init(&xh), 0);

	dr = xbps_find_pkg_dict_installed(&xh, "xbps", false);
	ATF_REQUIRE_EQ(prop_object_type(dr), PROP_TYPE_DICTIONARY);
	prop_dictionary_get_cstring_nocopy(dr, "pkgver", &pkgver);
	ATF_REQUIRE_STREQ(pkgver, "xbps-0.14");

	xbps_end(&xh);
}

ATF_TC(find_virtualpkg_dict_installed_test);
ATF_TC_HEAD(find_virtualpkg_dict_installed_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_find_virtualpkg_dict_installed");
}
ATF_TC_BODY(find_virtualpkg_dict_installed_test, tc)
{
	struct xbps_handle xh;
	prop_dictionary_t dr;
	const char *pkgver, *tcsdir;
	char *cffile;

	/* get test source dir */
	tcsdir = atf_tc_get_config_var(tc, "srcdir");
	cffile = xbps_xasprintf("%s/xbps.conf", tcsdir);

	/* initialize xbps */
	memset(&xh, 0, sizeof(xh));
	xh.rootdir = "/tmp";
	xh.conffile = cffile;
	xh.metadir = tcsdir;
	ATF_REQUIRE_EQ(xbps_init(&xh), 0);

	dr = xbps_find_virtualpkg_dict_installed(&xh, "xbps-src>=24", true);
	ATF_REQUIRE_EQ(prop_object_type(dr), PROP_TYPE_DICTIONARY);
	prop_dictionary_get_cstring_nocopy(dr, "pkgver", &pkgver);
	ATF_REQUIRE_STREQ(pkgver, "xbps-src-git-20120312");

	xbps_end(&xh);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, find_pkg_dict_installed_test);
	ATF_TP_ADD_TC(tp, find_virtualpkg_dict_installed_test);

	return atf_no_error();
}
