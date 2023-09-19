/* SPDX-FileCopyrightText: Copyright 2023 Duncan Overbruck <mail@duncano.de> */
/* SPDX-License-Identifier: BSD-2-Clause */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <atf-c.h>

#include <xbps.h>
#include <xbps/json.h>
#include <xbps/xbps_dictionary.h>

ATF_TC(xbps_json_print_escape);

ATF_TC_HEAD(xbps_json_print_escape, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_json_print_escape");
}

ATF_TC_BODY(xbps_json_print_escape, tc)
{
	char *buf = NULL;
	size_t bufsz = 0;
	struct xbps_json_printer p = {0};

	ATF_REQUIRE(p.file = open_memstream(&buf, &bufsz));

	ATF_CHECK_EQ(0, xbps_json_print_escaped(&p, "\"\\\b\f\n\r\t"));
	ATF_REQUIRE_EQ(0, fflush(p.file));
	ATF_CHECK_STREQ("\\\"\\\\\\b\\f\\n\\r\\t", buf);

	memset(buf, '\0', bufsz);
	ATF_CHECK_EQ(0, fseek(p.file, 0, SEEK_SET));
	ATF_CHECK_EQ(0, xbps_json_print_escaped(&p, "09azAZ !$#%^()%"));
	ATF_REQUIRE_EQ(0, fflush(p.file));
	ATF_CHECK_STREQ("09azAZ !$#%^()%", buf);

	memset(buf, '\0', bufsz);
	ATF_CHECK_EQ(0, fseek(p.file, 0, SEEK_SET));
	ATF_CHECK_EQ(0, xbps_json_print_escaped(&p, "\x01\x1F"));
	ATF_REQUIRE_EQ(0, fflush(p.file));
	ATF_CHECK_STREQ("\\u0001\\u001f", buf);
}

ATF_TC(xbps_json_print_xbps_dictionary);

ATF_TC_HEAD(xbps_json_print_xbps_dictionary, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_json_print_escape");
}

ATF_TC_BODY(xbps_json_print_xbps_dictionary, tc)
{
	char *buf = NULL;
	size_t bufsz = 0;
	struct xbps_json_printer p = {0};
	xbps_dictionary_t dict;
	static const char *s = ""
		"<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
		"<plist version=\"1.0\">\n"
		"<dict>\n"
		"<key>array-empty</key>\n"
		"<array>\n"
		"</array>\n"
		"<key>array-numbers</key>\n"
		"<array>\n"
		"    <integer>1</integer>\n"
		"    <integer>2</integer>\n"
		"    <integer>3</integer>\n"
		"</array>\n"
		"<key>dict-empty</key>\n"
		"<dict></dict>\n"
		"<key>num-signed</key>\n"
		"<integer>1</integer>\n"
		"<key>num-unsigned</key>\n"
		"<integer>0x1</integer>\n"
		"<key>string</key>\n"
		"<string>hello world</string>\n"
		"</dict>\n"
		"</plist>\n";

	ATF_REQUIRE(dict = xbps_dictionary_internalize(s));
	ATF_REQUIRE(p.file = open_memstream(&buf, &bufsz));

	ATF_REQUIRE_EQ(0, xbps_json_print_xbps_dictionary(&p, dict));
	ATF_REQUIRE_EQ(0, fflush(p.file));
	ATF_CHECK_STREQ("{\"array-empty\": [], \"array-numbers\": [1, 2, 3], \"dict-empty\": {}, \"num-signed\": 1, \"num-unsigned\": 1, \"string\": \"hello world\"}", buf);
}

ATF_TC(xbps_json_print_xbps_dictionary_indented);

ATF_TC_HEAD(xbps_json_print_xbps_dictionary_indented, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_json_print_xbps_dictionary: with indents");
}

ATF_TC_BODY(xbps_json_print_xbps_dictionary_indented, tc)
{
	char *buf = NULL;
	size_t bufsz = 0;
	struct xbps_json_printer p = {.indent = 2};
	xbps_dictionary_t dict;
	static const char *s = ""
		"<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
		"<plist version=\"1.0\">\n"
		"<dict>\n"
		"<key>array-empty</key>\n"
		"<array>\n"
		"</array>\n"
		"<key>array-numbers</key>\n"
		"<array>\n"
		"    <integer>1</integer>\n"
		"    <integer>2</integer>\n"
		"    <integer>3</integer>\n"
		"</array>\n"
		"<key>dict-empty</key>\n"
		"<dict></dict>\n"
		"<key>num-signed</key>\n"
		"<integer>1</integer>\n"
		"<key>num-unsigned</key>\n"
		"<integer>0x1</integer>\n"
		"<key>string</key>\n"
		"<string>hello world</string>\n"
		"</dict>\n"
		"</plist>\n";

	ATF_REQUIRE(dict = xbps_dictionary_internalize(s));
	ATF_REQUIRE(p.file = open_memstream(&buf, &bufsz));

	ATF_REQUIRE_EQ(0, xbps_json_print_xbps_dictionary(&p, dict));
	ATF_REQUIRE_EQ(0, fflush(p.file));
	ATF_CHECK_STREQ(
		"{\n"
		"  \"array-empty\": [],\n"
		"  \"array-numbers\": [\n"
		"    1,\n"
		"    2,\n"
		"    3\n"
		"  ],\n"
		"  \"dict-empty\": {},\n"
		"  \"num-signed\": 1,\n"
		"  \"num-unsigned\": 1,\n"
		"  \"string\": \"hello world\"\n"
		"}", buf);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, xbps_json_print_escape);
	ATF_TP_ADD_TC(tp, xbps_json_print_xbps_dictionary);
	ATF_TP_ADD_TC(tp, xbps_json_print_xbps_dictionary_indented);
	return atf_no_error();
}
