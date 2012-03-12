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
"			<string>afoo</string>\n"
"			<key>version</key>\n"
"			<string>1.1</string>\n"
"			<key>pkgver</key>\n"
"			<string>afoo-1.1</string>\n"
"			<key>provides</key>\n"
"			<array>\n"
"				<string>virtualpkg-9999</string>\n"
"			</array>\n"
"		</dict>\n"
"		<dict>\n"
"			<key>pkgname</key>\n"
"			<string>foo</string>\n"
"			<key>version</key>\n"
"			<string>2.0</string>\n"
"			<key>pkgver</key>\n"
"			<string>foo-2.0</string>\n"
"		</dict>\n"
"	</array>\n"
"</dict>\n"
"</plist>\n";

ATF_TC(find_pkg_in_dict_by_name_test);
ATF_TC_HEAD(find_pkg_in_dict_by_name_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_find_pkg_in_dict_by_name");
}
ATF_TC_BODY(find_pkg_in_dict_by_name_test, tc)
{
	prop_dictionary_t d, dr;

	d = prop_dictionary_internalize(dictxml);
	ATF_REQUIRE_EQ(prop_object_type(d), PROP_TYPE_DICTIONARY);

	/* match by pkgname */
	dr = xbps_find_pkg_in_dict_by_name(d, "packages", "foo");
	ATF_REQUIRE_EQ(prop_object_type(dr), PROP_TYPE_DICTIONARY);
}

ATF_TC(find_pkg_in_dict_by_pattern_test);
ATF_TC_HEAD(find_pkg_in_dict_by_pattern_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_find_pkg_in_dict_by_pattern");
}
ATF_TC_BODY(find_pkg_in_dict_by_pattern_test, tc)
{
	prop_dictionary_t d, dr;

	d = prop_dictionary_internalize(dictxml);
	ATF_REQUIRE_EQ(prop_object_type(d), PROP_TYPE_DICTIONARY);

	/* match by pkgpattern */
	dr = xbps_find_pkg_in_dict_by_pattern(d, "packages", "foo>=2.0");
	ATF_REQUIRE_EQ(prop_object_type(dr), PROP_TYPE_DICTIONARY);
}

ATF_TC(find_pkg_in_dict_by_pkgver_test);
ATF_TC_HEAD(find_pkg_in_dict_by_pkgver_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_find_pkg_in_dict_by_pkgver");
}
ATF_TC_BODY(find_pkg_in_dict_by_pkgver_test, tc)
{
	prop_dictionary_t d, dr;
	
	d = prop_dictionary_internalize(dictxml);
	ATF_REQUIRE_EQ(prop_object_type(d), PROP_TYPE_DICTIONARY);

	/* exact match by pkgver */
	dr = xbps_find_pkg_in_dict_by_pkgver(d, "packages", "foo-2.0");
	ATF_REQUIRE_EQ(prop_object_type(dr), PROP_TYPE_DICTIONARY);
}

ATF_TC(find_virtualpkg_in_dict_by_pattern_test);
ATF_TC_HEAD(find_virtualpkg_in_dict_by_pattern_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_find_virtualpkg_in_dict_by_pattern");
}
ATF_TC_BODY(find_virtualpkg_in_dict_by_pattern_test, tc)
{
	prop_dictionary_t d, dr;
	const char *pkgver;

	d = prop_dictionary_internalize(dictxml);
	ATF_REQUIRE_EQ(prop_object_type(d), PROP_TYPE_DICTIONARY);

	/* match virtualpkg by pattern */
	dr = xbps_find_virtualpkg_in_dict_by_pattern(d, "packages", "virtualpkg<=9999");
	ATF_REQUIRE_EQ(prop_object_type(dr), PROP_TYPE_DICTIONARY);
	prop_dictionary_get_cstring_nocopy(dr, "pkgver", &pkgver);
	ATF_REQUIRE_STREQ(pkgver, "afoo-1.1");
}

ATF_TC(find_virtualpkg_in_dict_by_name_test);
ATF_TC_HEAD(find_virtualpkg_in_dict_by_name_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_find_virtualpkg_in_dict_by_name");
}
ATF_TC_BODY(find_virtualpkg_in_dict_by_name_test, tc)
{
	prop_dictionary_t d, dr;
	const char *pkgver;

	d = prop_dictionary_internalize(dictxml);
	ATF_REQUIRE_EQ(prop_object_type(d), PROP_TYPE_DICTIONARY);

	/* match virtualpkg by name */
	dr = xbps_find_virtualpkg_in_dict_by_name(d, "packages", "virtualpkg");
	ATF_REQUIRE_EQ(prop_object_type(dr), PROP_TYPE_DICTIONARY);
	prop_dictionary_get_cstring_nocopy(dr, "pkgver", &pkgver);
	ATF_REQUIRE_STREQ(pkgver, "afoo-1.1");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, find_pkg_in_dict_by_name_test);
	ATF_TP_ADD_TC(tp, find_pkg_in_dict_by_pattern_test);
	ATF_TP_ADD_TC(tp, find_pkg_in_dict_by_pkgver_test);
	ATF_TP_ADD_TC(tp, find_virtualpkg_in_dict_by_name_test);
	ATF_TP_ADD_TC(tp, find_virtualpkg_in_dict_by_pattern_test);

	return atf_no_error();
}
