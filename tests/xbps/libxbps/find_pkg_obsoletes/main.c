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

static void
append_file(xbps_dictionary_t d, const char *key, const char *fpath)
{
	xbps_array_t a;
	xbps_dictionary_t filed;

	filed = xbps_dictionary_create();
	a = xbps_dictionary_get(d, key);
	if (a == NULL)  {
		a = xbps_array_create();
		xbps_dictionary_set(d, key, a);
	}

	xbps_dictionary_set_cstring_nocopy(filed, "file", fpath);
	xbps_array_add(a, filed);
}

static xbps_dictionary_t
create_dict(const char *key, const char *fpath)
{
	xbps_array_t a;
	xbps_dictionary_t d, filed;

	d = xbps_dictionary_create();
	filed = xbps_dictionary_create();
	a = xbps_array_create();

	xbps_dictionary_set_cstring_nocopy(filed, "file", fpath);
	xbps_dictionary_set_cstring_nocopy(filed, "sha256", "kjaskajsk");
	xbps_array_add(a, filed);
	xbps_dictionary_set(d, key, a);

	return d;
}

ATF_TC(find_pkg_obsoletes_test);
ATF_TC_HEAD(find_pkg_obsoletes_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_find_pkg_obsoletes");
}

ATF_TC_BODY(find_pkg_obsoletes_test, tc)
{
	struct xbps_handle xh;
	xbps_array_t res;
	xbps_dictionary_t d1, d2;
	const char *tcsdir;

	/* get test source dir */
	tcsdir = atf_tc_get_config_var(tc, "srcdir");

	memset(&xh, 0, sizeof(xh));
	strncpy(xh.rootdir, tcsdir, sizeof(xh.rootdir));
	xh.conffile = "/tmp/unexistent.conf";
	ATF_REQUIRE_EQ(xbps_init(&xh), 0);

        d1 = create_dict("files", "/etc/foo.conf");
	d2 = create_dict("conf_files", "/etc/foo.conf");

	res = xbps_find_pkg_obsoletes(&xh, d1, d2);
	ATF_REQUIRE_EQ(xbps_array_count(res), 0);

	res = xbps_find_pkg_obsoletes(&xh, d2, d1);
	ATF_REQUIRE_EQ(xbps_array_count(res), 0);

	append_file(d1, "files", "file");
	res = xbps_find_pkg_obsoletes(&xh, d1, d2);
	ATF_REQUIRE_EQ(xbps_array_count(res), 1);

	append_file(d1, "conf_files", "conf_file");
	res = xbps_find_pkg_obsoletes(&xh, d1, d2);
	ATF_REQUIRE_EQ(xbps_array_count(res), 2);

	append_file(d1, "links", "link");
	res = xbps_find_pkg_obsoletes(&xh, d1, d2);
	ATF_REQUIRE_EQ(xbps_array_count(res), 3);

	append_file(d1, "dirs", "dir");
	res = xbps_find_pkg_obsoletes(&xh, d1, d2);
	ATF_REQUIRE_EQ(xbps_array_count(res), 4);

	append_file(d2, "files", "file");
	res = xbps_find_pkg_obsoletes(&xh, d1, d2);
	ATF_REQUIRE_EQ(xbps_array_count(res), 3);

	append_file(d2, "conf_files", "conf_file");
	res = xbps_find_pkg_obsoletes(&xh, d1, d2);
	ATF_REQUIRE_EQ(xbps_array_count(res), 2);

	append_file(d2, "links", "link");
	res = xbps_find_pkg_obsoletes(&xh, d1, d2);
	ATF_REQUIRE_EQ(xbps_array_count(res), 1);

	append_file(d2, "dirs", "dir");
	res = xbps_find_pkg_obsoletes(&xh, d1, d2);
	ATF_REQUIRE_EQ(xbps_array_count(res), 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, find_pkg_obsoletes_test);

	return atf_no_error();
}
