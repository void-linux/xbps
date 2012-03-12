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

static const char axml[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
"<plist version=\"1.0\">\n"
"<array>\n"
"	<dict>\n"
"		<key>pkgname</key>\n"
"		<string>afoo</string>\n"
"		<key>version</key>\n"
"		<string>1.1</string>\n"
"		<key>pkgver</key>\n"
"		<string>afoo-1.1</string>\n"
"	</dict>\n"
"	<dict>\n"
"		<key>pkgname</key>\n"
"		<string>foo</string>\n"
"		<key>version</key>\n"
"		<string>2.0</string>\n"
"		<key>pkgver</key>\n"
"		<string>foo-2.0</string>\n"
"	</dict>\n"
"</array>\n"
"</plist>\n";

static const char axml2[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
"<plist version=\"1.0\">\n"
"<array>\n"
"	<dict>\n"
"		<key>pkgname</key>\n"
"		<string>bfoo</string>\n"
"		<key>version</key>\n"
"		<string>1.2</string>\n"
"		<key>pkgver</key>\n"
"		<string>bfoo-1.2</string>\n"
"	</dict>\n"
"	<dict>\n"
"		<key>pkgname</key>\n"
"		<string>foo</string>\n"
"		<key>version</key>\n"
"		<string>2.0</string>\n"
"		<key>pkgver</key>\n"
"		<string>foo-2.0</string>\n"
"	</dict>\n"
"</array>\n"
"</plist>\n";

ATF_TC(array_replace_dict_by_name_test);

ATF_TC_HEAD(array_replace_dict_by_name_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_array_replace_dict_by_name");
}

ATF_TC_BODY(array_replace_dict_by_name_test, tc)
{
	prop_array_t orig, new;
	prop_dictionary_t d;
	
	orig = prop_array_internalize(axml);
	ATF_REQUIRE_EQ(prop_object_type(orig), PROP_TYPE_ARRAY);

	new = prop_array_internalize(axml2);
	ATF_REQUIRE_EQ(prop_object_type(new), PROP_TYPE_ARRAY);

	d = prop_dictionary_create();
	ATF_REQUIRE_EQ(prop_object_type(d), PROP_TYPE_DICTIONARY);
	ATF_REQUIRE_EQ(prop_dictionary_set_cstring_nocopy(d, "pkgname", "bfoo"), true);
	ATF_REQUIRE_EQ(prop_dictionary_set_cstring_nocopy(d, "pkgver", "bfoo-1.2"), true);
	ATF_REQUIRE_EQ(prop_dictionary_set_cstring_nocopy(d, "version", "1.2"), true);

	ATF_REQUIRE_EQ(xbps_array_replace_dict_by_name(orig, d, "afoo"), 0);
	ATF_REQUIRE_EQ(prop_array_equals(orig, new), true);
}

ATF_TC(array_replace_dict_by_pattern_test);

ATF_TC_HEAD(array_replace_dict_by_pattern_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_array_replace_dict_by_pattern");
}

ATF_TC_BODY(array_replace_dict_by_pattern_test, tc)
{
	prop_array_t orig, new;
	prop_dictionary_t d;
	
	orig = prop_array_internalize(axml);
	ATF_REQUIRE_EQ(prop_object_type(orig), PROP_TYPE_ARRAY);

	new = prop_array_internalize(axml2);
	ATF_REQUIRE_EQ(prop_object_type(new), PROP_TYPE_ARRAY);

	d = prop_dictionary_create();
	ATF_REQUIRE_EQ(prop_object_type(d), PROP_TYPE_DICTIONARY);
	ATF_REQUIRE_EQ(prop_dictionary_set_cstring_nocopy(d, "pkgname", "bfoo"), true);
	ATF_REQUIRE_EQ(prop_dictionary_set_cstring_nocopy(d, "pkgver", "bfoo-1.2"), true);
	ATF_REQUIRE_EQ(prop_dictionary_set_cstring_nocopy(d, "version", "1.2"), true);

	ATF_REQUIRE_EQ(xbps_array_replace_dict_by_pattern(orig, d, "afoo>=1.0"), 0);
	ATF_REQUIRE_EQ(prop_array_equals(orig, new), true);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, array_replace_dict_by_name_test);
	ATF_TP_ADD_TC(tp, array_replace_dict_by_pattern_test);

	return atf_no_error();
}
