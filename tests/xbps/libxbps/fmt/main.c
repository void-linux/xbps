/*-
 * Copyright (c) 2023 Duncan Overbruck <mail@duncano.de>.
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
#include "xbps.h"
#include "xbps/xbps_dictionary.h"
#include "xbps/xbps_object.h"
#include <atf-c/macros.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <atf-c.h>
#include <xbps.h>

ATF_TC(xbps_fmt_print_number);

ATF_TC_HEAD(xbps_fmt_print_number, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_fmt_print_number");
}

ATF_TC_BODY(xbps_fmt_print_number, tc)
{
	char *buf = NULL;
	size_t bufsz = 0;
	FILE *fp;
	struct test {
		const char *expect;
		int64_t d;
		struct xbps_fmt_spec spec;
	} tests[] = {
		{ "1",  1, {0}},
		{"-1", -1, {0}},
		{"-1", -1, {.sign = '+'}},
		{"+1",  1, {.sign = '+'}},

		{"a", 0xA, {.type = 'x'}},
		{"A", 0xA, {.type = 'X'}},

		{"644", 0644, {.type = 'o'}},

		{"0010",  10, {.fill = '0', .align = '>', .width = 4}},
		{"1000",  10, {.fill = '0', .align = '<', .width = 4}},
		{"0010",  10, {.fill = '0', .align = '=', .width = 4}},
		{"-010", -10, {.fill = '0', .align = '=', .width = 4}},
		{"+010",  10, {.fill = '0', .align = '=', .sign = '+', .width = 4}},
	};
	ATF_REQUIRE(fp = open_memstream(&buf, &bufsz));

	for (unsigned i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
		struct xbps_fmt fmt = { .spec = &tests[i].spec };
		memset(buf, '\0', bufsz);
		rewind(fp);
		xbps_fmt_print_number(&fmt, tests[i].d, fp);
		ATF_REQUIRE(fflush(fp) == 0);
		ATF_CHECK_STREQ(buf, tests[i].expect);
	}
	ATF_REQUIRE(fclose(fp) == 0);
	free(buf);
}

ATF_TC(xbps_fmt_print_string);

ATF_TC_HEAD(xbps_fmt_print_string, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_fmt_print_string");
}

ATF_TC_BODY(xbps_fmt_print_string, tc)
{
	char *buf = NULL;
	size_t bufsz = 0;
	FILE *fp;
	struct test {
		const char *expect;
		const char *input;
		size_t len;
		struct xbps_fmt_spec spec;
	} tests[] = {
		{ "1",   "1",   0, {0}},
		{ "2 ",  "2",   0, {.fill = ' ', .align = '<', .width = 2}},
		{ " 3",  "3",   0, {.fill = ' ', .align = '>', .width = 2}},
		{ "444", "444", 0, {.fill = ' ', .align = '>', .width = 2}},
		{ "44",  "444", 2, {.fill = ' ', .align = '>', .width = 2}},
	};
	ATF_REQUIRE(fp = open_memstream(&buf, &bufsz));

	for (unsigned i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
		struct xbps_fmt fmt = { .spec = &tests[i].spec };
		memset(buf, '\0', bufsz);
		rewind(fp);
		xbps_fmt_print_string(&fmt, tests[i].input, tests[i].len, fp);
		ATF_REQUIRE(fflush(fp) == 0);
		ATF_CHECK_STREQ(buf, tests[i].expect);
	}
	ATF_REQUIRE(fclose(fp) == 0);
	free(buf);
}

ATF_TC(xbps_fmt_dictionary);

ATF_TC_HEAD(xbps_fmt_dictionary, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_fmt_dictionary");
}

ATF_TC_BODY(xbps_fmt_dictionary, tc)
{
	char *buf = NULL;
	size_t bufsz = 0;
	FILE *fp;
	struct xbps_fmt *fmt;
	xbps_dictionary_t dict;
	ATF_REQUIRE(fp = open_memstream(&buf, &bufsz));
	ATF_REQUIRE(dict = xbps_dictionary_create());
	ATF_REQUIRE(xbps_dictionary_set_cstring_nocopy(dict, "string", "s"));
	ATF_REQUIRE(xbps_dictionary_set_int64(dict, "number", 1));
	ATF_REQUIRE(fmt = xbps_fmt_parse(">{string} {number} {number!humanize}<"));
	ATF_REQUIRE(xbps_fmt_dictionary(fmt, dict, fp) == 0);
	ATF_REQUIRE(fflush(fp) == 0);
	ATF_CHECK_STREQ(buf, ">s 1 0KB<");
	ATF_REQUIRE(fclose(fp) == 0);
	free(buf);
	xbps_object_release(dict);
}

ATF_TC(xbps_fmts_dictionary);

ATF_TC_HEAD(xbps_fmts_dictionary, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_fmt_dictionary");
}

ATF_TC_BODY(xbps_fmts_dictionary, tc)
{
	char *buf = NULL;
	size_t bufsz = 0;
	FILE *fp;
	xbps_dictionary_t dict;
	ATF_REQUIRE(fp = open_memstream(&buf, &bufsz));
	ATF_REQUIRE(dict = xbps_dictionary_create());
	ATF_REQUIRE(xbps_dictionary_set_cstring_nocopy(dict, "string", "s"));
	ATF_REQUIRE(xbps_dictionary_set_int64(dict, "number", 1));
	ATF_REQUIRE(xbps_fmts_dictionary(">{string} {number} {number!humanize}<", dict, fp) == 0);
	ATF_REQUIRE(fflush(fp) == 0);
	ATF_CHECK_STREQ(buf, ">s 1 0KB<");
	ATF_REQUIRE(fclose(fp) == 0);
	free(buf);
	xbps_object_release(dict);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, xbps_fmt_print_number);
	ATF_TP_ADD_TC(tp, xbps_fmt_print_string);
	ATF_TP_ADD_TC(tp, xbps_fmt_dictionary);
	ATF_TP_ADD_TC(tp, xbps_fmts_dictionary);
	return atf_no_error();
}
