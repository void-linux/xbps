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

ATF_TC(pkgpattern_match_test);

ATF_TC_HEAD(pkgpattern_match_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_pkgpattern_match");
}

ATF_TC_BODY(pkgpattern_match_test, tc)
{
	ATF_REQUIRE_EQ(xbps_pkgpattern_match("foo-1.0", "foo>=0"), 1);
	ATF_REQUIRE_EQ(xbps_pkgpattern_match("foo-1.0", "foo>=1.0"), 1);
	ATF_REQUIRE_EQ(xbps_pkgpattern_match("foo-1.0", "foo>=1.0<1.0_1"), 1);
	ATF_REQUIRE_EQ(xbps_pkgpattern_match("foo-1.0", "foo>1.0_1"), 0);
	ATF_REQUIRE_EQ(xbps_pkgpattern_match("foo-1.0", "foo<1.0"), 0);
	ATF_REQUIRE_EQ(xbps_pkgpattern_match("foo-1.0", "foo-1.0"), 1);
	ATF_REQUIRE_EQ(xbps_pkgpattern_match("foo-1.0", "foo-[0-1].[0-9]*"), 1);
	ATF_REQUIRE_EQ(xbps_pkgpattern_match("foo-1.0", "foo-[1-2].[1-9]*"), 0);
	ATF_REQUIRE_EQ(xbps_pkgpattern_match("foo-1.01", "foo-1.[0-9]?"), 1);
	ATF_REQUIRE_EQ(xbps_pkgpattern_match("foo-1.01", "foo-1.[1-9]?"), 0);
	ATF_REQUIRE_EQ(xbps_pkgpattern_match("foo-1.01", "foo-1.[0-2][2-4]?"), 0);
	ATF_REQUIRE_EQ(xbps_pkgpattern_match("foo-1.02", "foo>=1.[0-2][2-4]?"), 1);
	ATF_REQUIRE_EQ(xbps_pkgpattern_match("foo-1.12", "foo>=1.[0-2][2-4]?"), 1);
	ATF_REQUIRE_EQ(xbps_pkgpattern_match("foo-1.22", "foo>=1.[0-2][2-4]?"), 1);
	ATF_REQUIRE_EQ(xbps_pkgpattern_match("foo-1.23", "foo>=1.[0-2][2-4]?"), 1);
	ATF_REQUIRE_EQ(xbps_pkgpattern_match("foo-1.24", "foo>=1.[0-2][2-4]?"), 1);
	ATF_REQUIRE_EQ(xbps_pkgpattern_match("foo-1.11", "foo-1.[0-2][2-4]?"), 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, pkgpattern_match_test);
	return atf_no_error();
}
