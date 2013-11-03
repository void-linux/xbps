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

ATF_TC(cmpver_test);

ATF_TC_HEAD(cmpver_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_cmpver conditions");
}

ATF_TC_BODY(cmpver_test, tc)
{
	ATF_REQUIRE_EQ(xbps_cmpver("foo-1.0", "foo-1.0"), 0);
	ATF_REQUIRE_EQ(xbps_cmpver("foo-1.0", "foo-1.0_1"), -1);
	ATF_REQUIRE_EQ(xbps_cmpver("foo-1.0_1", "foo-1.0"), 1);
	ATF_REQUIRE_EQ(xbps_cmpver("foo-2.0rc2", "foo-2.0rc3"), -1);
	ATF_REQUIRE_EQ(xbps_cmpver("foo-2.0rc3", "foo-2.0rc2"), 1);
	ATF_REQUIRE_EQ(xbps_cmpver("foo-129", "foo-129_1"), -1);
	ATF_REQUIRE_EQ(xbps_cmpver("foo-blah-100dpi-21", "foo-blah-100dpi-21_0"), 0);
	ATF_REQUIRE_EQ(xbps_cmpver("foo-blah-100dpi-21", "foo-blah-100dpi-2.1"), 1);
	ATF_REQUIRE_EQ(xbps_cmpver("foo-1.0.1", "foo-1.0_1"), 1);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, cmpver_test);
	return atf_no_error();
}
