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

static const char dictxml2[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
"<plist version=\"1.0\">\n"
"<dict>\n"
"	<key>packages</key>\n"
"	<array>\n"
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

static const char axml[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
"<plist version=\"1.0\">\n"
"<array>\n"
"	<string>foo-1.0</string>\n"
"	<string>blah-2.0</string>\n"
"</array>\n"
"</plist>\n";

static const char axml2[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
"<plist version=\"1.0\">\n"
"<array>\n"
"	<string>blah-2.0</string>\n"
"</array>\n"
"</plist>\n";

ATF_TC(remove_pkg_from_array_by_name_test);

ATF_TC_HEAD(remove_pkg_from_array_by_name_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_remove_pkg_from_array_by_name");
}

ATF_TC_BODY(remove_pkg_from_array_by_name_test, tc)
{
	prop_array_t a;
	prop_dictionary_t d, d2;

	d = prop_dictionary_internalize(dictxml);
	ATF_REQUIRE_EQ(prop_object_type(d), PROP_TYPE_DICTIONARY);

	d2 = prop_dictionary_internalize(dictxml2);
	ATF_REQUIRE_EQ(prop_object_type(d2), PROP_TYPE_DICTIONARY);

	a = prop_dictionary_get(d, "packages");
	ATF_REQUIRE_EQ(xbps_remove_pkg_from_array_by_name(a, "afoo"), true);
	ATF_REQUIRE_EQ(prop_dictionary_equals(d, d2), true);
}

ATF_TC(remove_pkg_from_array_by_pattern_test);

ATF_TC_HEAD(remove_pkg_from_array_by_pattern_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_remove_pkg_from_array_by_pattern");
}

ATF_TC_BODY(remove_pkg_from_array_by_pattern_test, tc)
{
	prop_array_t a;
	prop_dictionary_t d, d2;

	d = prop_dictionary_internalize(dictxml);
	ATF_REQUIRE_EQ(prop_object_type(d), PROP_TYPE_DICTIONARY);

	d2 = prop_dictionary_internalize(dictxml2);
	ATF_REQUIRE_EQ(prop_object_type(d2), PROP_TYPE_DICTIONARY);

	a = prop_dictionary_get(d, "packages");
	ATF_REQUIRE_EQ(xbps_remove_pkg_from_array_by_pattern(a, "afoo>=1.0"), true);
	ATF_REQUIRE_EQ(prop_dictionary_equals(d, d2), true);
}

ATF_TC(remove_pkg_from_array_by_pkgver_test);

ATF_TC_HEAD(remove_pkg_from_array_by_pkgver_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_remove_pkg_from_array_by_pkgver");
}

ATF_TC_BODY(remove_pkg_from_array_by_pkgver_test, tc)
{
	prop_array_t a;
	prop_dictionary_t d, d2;

	d = prop_dictionary_internalize(dictxml);
	ATF_REQUIRE_EQ(prop_object_type(d), PROP_TYPE_DICTIONARY);

	d2 = prop_dictionary_internalize(dictxml2);
	ATF_REQUIRE_EQ(prop_object_type(d2), PROP_TYPE_DICTIONARY);

	a = prop_dictionary_get(d, "packages");
	ATF_REQUIRE_EQ(xbps_remove_pkg_from_array_by_pkgver(a, "afoo-1.1"), true);
	ATF_REQUIRE_EQ(prop_dictionary_equals(d, d2), true);
}

ATF_TC(remove_string_from_array_test);

ATF_TC_HEAD(remove_string_from_array_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_remove_string_from_array");
}

ATF_TC_BODY(remove_string_from_array_test, tc)
{
	prop_array_t a, a2;

	a = prop_array_internalize(axml);
	ATF_REQUIRE_EQ(prop_object_type(a), PROP_TYPE_ARRAY);

	a2 = prop_array_internalize(axml2);
	ATF_REQUIRE_EQ(prop_object_type(a2), PROP_TYPE_ARRAY);

	ATF_REQUIRE_EQ(xbps_remove_string_from_array(a, "foo-1.0"), true);
	ATF_REQUIRE_EQ(prop_array_equals(a, a2), true);
}

ATF_TC(remove_pkgname_from_array_test);

ATF_TC_HEAD(remove_pkgname_from_array_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_remove_pkgname_from_array");
}

ATF_TC_BODY(remove_pkgname_from_array_test, tc)
{
	prop_array_t a, a2;

	a = prop_array_internalize(axml);
	ATF_REQUIRE_EQ(prop_object_type(a), PROP_TYPE_ARRAY);

	a2 = prop_array_internalize(axml2);
	ATF_REQUIRE_EQ(prop_object_type(a2), PROP_TYPE_ARRAY);

	ATF_REQUIRE_EQ(xbps_remove_pkgname_from_array(a, "foo"), true);
	ATF_REQUIRE_EQ(prop_array_equals(a, a2), true);
}

ATF_TC(remove_pkg_from_dict_by_name_test);

ATF_TC_HEAD(remove_pkg_from_dict_by_name_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_remove_pkg_from_dict_by_name");
}

ATF_TC_BODY(remove_pkg_from_dict_by_name_test, tc)
{
	prop_dictionary_t d, d2;

	d = prop_dictionary_internalize(dictxml);
	ATF_REQUIRE_EQ(prop_object_type(d), PROP_TYPE_DICTIONARY);

	d2 = prop_dictionary_internalize(dictxml2);
	ATF_REQUIRE_EQ(prop_object_type(d2), PROP_TYPE_DICTIONARY);

	ATF_REQUIRE_EQ(xbps_remove_pkg_from_dict_by_name(d, "packages", "afoo"), true);
	ATF_REQUIRE_EQ(prop_dictionary_equals(d, d2), true);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, remove_pkg_from_array_by_name_test);
	ATF_TP_ADD_TC(tp, remove_pkg_from_array_by_pattern_test);
	ATF_TP_ADD_TC(tp, remove_pkg_from_array_by_pkgver_test);
	ATF_TP_ADD_TC(tp, remove_string_from_array_test);
	ATF_TP_ADD_TC(tp, remove_pkgname_from_array_test);
	ATF_TP_ADD_TC(tp, remove_pkg_from_dict_by_name_test);

	return atf_no_error();
}
