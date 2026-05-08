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
#include <xbps.h>

static xbps_array_t
array_init(void)
{
	xbps_array_t a;

	a = xbps_array_create();
	ATF_REQUIRE(a != NULL);
	xbps_array_add_cstring_nocopy(a, "foo-2.0_1");
	xbps_array_add_cstring_nocopy(a, "blah-2.1_1");

	return a;
}

ATF_TC(match_string_test);
ATF_TC_HEAD(match_string_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_match_string_in_array");
}

ATF_TC_BODY(match_string_test, tc)
{
	xbps_array_t a = array_init();
	ATF_REQUIRE_EQ(xbps_match_string_in_array(a, "foo-2.0_1"), true);
	ATF_REQUIRE_EQ(xbps_match_string_in_array(a, "foo-2.1_1"), false);
	xbps_object_release(a);
}

ATF_TC(match_pkgname_test);
ATF_TC_HEAD(match_pkgname_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_match_pkgname_in_array");
}

ATF_TC_BODY(match_pkgname_test, tc)
{
	xbps_array_t a = array_init();
	ATF_REQUIRE_EQ(xbps_match_pkgname_in_array(a, "foo"), true);
	ATF_REQUIRE_EQ(xbps_match_pkgname_in_array(a, "baz"), false);
	xbps_object_release(a);
}

ATF_TC(match_pkgpattern_test);
ATF_TC_HEAD(match_pkgpattern_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_match_pkgpattern_in_array");
}

ATF_TC_BODY(match_pkgpattern_test, tc)
{
	xbps_array_t a = array_init();
	ATF_REQUIRE_EQ(xbps_match_pkgpattern_in_array(a, "foo>=1.0"), true);
	ATF_REQUIRE_EQ(xbps_match_pkgpattern_in_array(a, "blah<1.0"), false);
	xbps_object_release(a);
}

ATF_TC(match_pkgdep_test);
ATF_TC_HEAD(match_pkgdep_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_match_pkgdep_in_array");
}

ATF_TC_BODY(match_pkgdep_test, tc)
{
	xbps_array_t a = array_init();
	ATF_REQUIRE_EQ(xbps_match_pkgdep_in_array(a, "foo-2.0_1"), true);
	xbps_object_release(a);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, match_string_test);
	ATF_TP_ADD_TC(tp, match_pkgname_test);
	ATF_TP_ADD_TC(tp, match_pkgpattern_test);
	ATF_TP_ADD_TC(tp, match_pkgdep_test);

	return atf_no_error();
}
