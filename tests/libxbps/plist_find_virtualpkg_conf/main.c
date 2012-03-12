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

static const char dictxml[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
"<plist version=\"1.0\">\n"
"<dict>\n"
"	<key>packages</key>\n"
"	<array>\n"
"		<dict>\n"
"			<key>pkgname</key>\n"
"			<string>xbps-src-git</string>\n"
"			<key>version</key>\n"
"			<string>20120311</string>\n"
"			<key>pkgver</key>\n"
"			<string>xbps-src-git-20120311</string>\n"
"			<key>provides</key>\n"
"			<array>\n"
"				<string>xbps-src-24</string>\n"
"			</array>\n"
"		</dict>\n"
"		<dict>\n"
"			<key>pkgname</key>\n"
"			<string>xbps-src</string>\n"
"			<key>version</key>\n"
"			<string>24</string>\n"
"			<key>pkgver</key>\n"
"			<string>xbps-src-24</string>\n"
"		</dict>\n"
"	</array>\n"
"</dict>\n"
"</plist>\n";

ATF_TC(find_virtualpkg_conf_in_array_by_name_test);
ATF_TC_HEAD(find_virtualpkg_conf_in_array_by_name_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_find_virtualpkg_conf_in_array_by_name");
}
ATF_TC_BODY(find_virtualpkg_conf_in_array_by_name_test, tc)
{
	struct xbps_handle xh;
	prop_array_t a;
	prop_dictionary_t d, dr;
	const char *pkgver, *tcsdir;
	char *cffile;

	/* get test source dir */
	tcsdir = atf_tc_get_config_var(tc, "srcdir");
	cffile = xbps_xasprintf("%s/xbps.conf", tcsdir);
	ATF_REQUIRE(cffile != NULL);

	/* initialize xbps */
	memset(&xh, 0, sizeof(xh));
	xh.rootdir = "/tmp";
	xh.conffile = cffile;
	ATF_REQUIRE_EQ(xbps_init(&xh), 0);

	d = prop_dictionary_internalize(dictxml);
	ATF_REQUIRE_EQ(prop_object_type(d), PROP_TYPE_DICTIONARY);

	a = prop_dictionary_get(d, "packages");
	ATF_REQUIRE_EQ(prop_object_type(a), PROP_TYPE_ARRAY);

	dr = xbps_find_virtualpkg_conf_in_array_by_name(a, "xbps-src");
	ATF_REQUIRE_EQ(prop_object_type(dr), PROP_TYPE_DICTIONARY);
	prop_dictionary_get_cstring_nocopy(dr, "pkgver", &pkgver);
	ATF_REQUIRE_STREQ(pkgver, "xbps-src-git-20120311");

	xbps_end();
}

ATF_TC(find_virtualpkg_conf_in_array_by_pattern_test);
ATF_TC_HEAD(find_virtualpkg_conf_in_array_by_pattern_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_find_virtualpkg_conf_in_array_by_pattern");
}
ATF_TC_BODY(find_virtualpkg_conf_in_array_by_pattern_test, tc)
{
	struct xbps_handle xh;
	prop_array_t a;
	prop_dictionary_t d, dr;
	const char *pkgver, *tcsdir;
	char *cffile;

	/* get test source dir */
	tcsdir = atf_tc_get_config_var(tc, "srcdir");
	cffile = xbps_xasprintf("%s/xbps.conf", tcsdir);
	ATF_REQUIRE(cffile != NULL);

	/* initialize xbps */
	memset(&xh, 0, sizeof(xh));
	xh.rootdir = "/tmp";
	xh.conffile = cffile;
	ATF_REQUIRE_EQ(xbps_init(&xh), 0);

	d = prop_dictionary_internalize(dictxml);
	ATF_REQUIRE_EQ(prop_object_type(d), PROP_TYPE_DICTIONARY);

	a = prop_dictionary_get(d, "packages");
	ATF_REQUIRE_EQ(prop_object_type(a), PROP_TYPE_ARRAY);

	dr = xbps_find_virtualpkg_conf_in_array_by_pattern(a, "xbps-src>=24");
	ATF_REQUIRE_EQ(prop_object_type(dr), PROP_TYPE_DICTIONARY);
	prop_dictionary_get_cstring_nocopy(dr, "pkgver", &pkgver);
	ATF_REQUIRE_STREQ(pkgver, "xbps-src-git-20120311");

	dr = xbps_find_virtualpkg_conf_in_array_by_pattern(a, "xbps-src>=25");
	ATF_REQUIRE(dr == NULL);

	xbps_end();
}

ATF_TC(find_virtualpkg_conf_in_dict_by_name_test);
ATF_TC_HEAD(find_virtualpkg_conf_in_dict_by_name_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_find_virtualpkg_conf_in_dict_by_name");
}
ATF_TC_BODY(find_virtualpkg_conf_in_dict_by_name_test, tc)
{
	struct xbps_handle xh;
	prop_dictionary_t d, dr;
	const char *pkgver, *tcsdir;
	char *cffile;

	/* get test source dir */
	tcsdir = atf_tc_get_config_var(tc, "srcdir");
	cffile = xbps_xasprintf("%s/xbps.conf", tcsdir);
	ATF_REQUIRE(cffile != NULL);

	/* initialize xbps */
	memset(&xh, 0, sizeof(xh));
	xh.rootdir = "/tmp";
	xh.conffile = cffile;
	ATF_REQUIRE_EQ(xbps_init(&xh), 0);

	d = prop_dictionary_internalize(dictxml);
	ATF_REQUIRE_EQ(prop_object_type(d), PROP_TYPE_DICTIONARY);

	dr = xbps_find_virtualpkg_conf_in_dict_by_name(d, "packages", "xbps-src");
	ATF_REQUIRE_EQ(prop_object_type(dr), PROP_TYPE_DICTIONARY);
	prop_dictionary_get_cstring_nocopy(dr, "pkgver", &pkgver);
	ATF_REQUIRE_STREQ(pkgver, "xbps-src-git-20120311");

	xbps_end();
}

ATF_TC(find_virtualpkg_conf_in_dict_by_pattern_test);
ATF_TC_HEAD(find_virtualpkg_conf_in_dict_by_pattern_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_find_virtualpkg_conf_in_pattern_by_name");
}
ATF_TC_BODY(find_virtualpkg_conf_in_dict_by_pattern_test, tc)
{
	struct xbps_handle xh;
	prop_dictionary_t d, dr;
	const char *pkgver, *tcsdir;
	char *cffile;

	/* get test source dir */
	tcsdir = atf_tc_get_config_var(tc, "srcdir");
	cffile = xbps_xasprintf("%s/xbps.conf", tcsdir);
	ATF_REQUIRE(cffile != NULL);

	/* initialize xbps */
	memset(&xh, 0, sizeof(xh));
	xh.rootdir = "/tmp";
	xh.conffile = cffile;
	ATF_REQUIRE_EQ(xbps_init(&xh), 0);

	d = prop_dictionary_internalize(dictxml);
	ATF_REQUIRE_EQ(prop_object_type(d), PROP_TYPE_DICTIONARY);

	dr = xbps_find_virtualpkg_conf_in_dict_by_pattern(d, "packages", "xbps-src>=24");
	ATF_REQUIRE_EQ(prop_object_type(dr), PROP_TYPE_DICTIONARY);
	prop_dictionary_get_cstring_nocopy(dr, "pkgver", &pkgver);
	ATF_REQUIRE_STREQ(pkgver, "xbps-src-git-20120311");

	dr = xbps_find_virtualpkg_conf_in_dict_by_pattern(d, "packages", "xbps-src>=25");
	ATF_REQUIRE(dr == NULL);

	xbps_end();
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, find_virtualpkg_conf_in_array_by_name_test);
	ATF_TP_ADD_TC(tp, find_virtualpkg_conf_in_array_by_pattern_test);
	ATF_TP_ADD_TC(tp, find_virtualpkg_conf_in_dict_by_name_test);
	ATF_TP_ADD_TC(tp, find_virtualpkg_conf_in_dict_by_pattern_test);

	return atf_no_error();
}
