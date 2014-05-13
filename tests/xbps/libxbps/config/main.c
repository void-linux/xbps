/*-
 * Copyright (c) 2013 Juan Romero Pardines.
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

ATF_TC(xbps_conf_include_test);
ATF_TC_HEAD(xbps_conf_include_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test including files by file globbing");
}

ATF_TC_BODY(xbps_conf_include_test, tc)
{
	struct xbps_handle xh;
	const char *tcsdir;
	char conffile[XBPS_MAXPATH-1];

	/* get test source dir */
	tcsdir = atf_tc_get_config_var(tc, "srcdir");

	/* change dir to make sure relative paths won't match */
	ATF_REQUIRE_EQ(chdir("/"), 0);
	memset(&xh, 0, sizeof(xh));
	strncpy(xh.rootdir, tcsdir, sizeof(xh.rootdir));
	strncpy(xh.metadir, tcsdir, sizeof(xh.metadir));
	strncpy(conffile, tcsdir, sizeof(conffile));
	strncat(conffile, "/xbps.conf", sizeof(conffile));
	xh.conffile = conffile;
	xh.flags = XBPS_FLAG_DEBUG;
	ATF_REQUIRE_EQ(xbps_init(&xh), 0);

	/* should contain both repositories defined in [12].include.conf */
	ATF_REQUIRE_EQ(xbps_array_count(xh.repositories), 2);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, xbps_conf_include_test);

	return atf_no_error();
}
