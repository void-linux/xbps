/*-
 * Copyright (c) 2012-2014 Juan Romero Pardines.
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
#include <stdlib.h>
#include <string.h>
#include <atf-c.h>
#include <xbps.h>

ATF_TC(util_test);

ATF_TC_HEAD(util_test, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test some utility functions");
}

ATF_TC_BODY(util_test, tc)
{
	char name[XBPS_NAME_SIZE];
	char *s;

	ATF_CHECK_EQ(xbps_pkg_name(name, sizeof(name), "font-adobe-a"), false);
	ATF_CHECK_EQ(xbps_pkg_name(name, sizeof(name), "font-adobe-1"), false);
	ATF_CHECK_EQ(xbps_pkg_name(name, sizeof(name), "font-adobe-100dpi"), false);
	ATF_CHECK_EQ(xbps_pkg_name(name, sizeof(name), "font-adobe-100dpi-7.8"), false);
	ATF_CHECK_EQ(xbps_pkg_name(name, sizeof(name), "python-e_dbus"), false);
	ATF_CHECK_EQ(xbps_pkg_name(name, sizeof(name), "fs-utils-v1"), false);
	ATF_CHECK_EQ(xbps_pkg_name(name, sizeof(name), "fs-utils-v_1"), false);
	ATF_CHECK_EQ(xbps_pkg_name(name, sizeof(name), "font-adobe-100dpi-1.8_blah"), false);
	ATF_CHECK_EQ(xbps_pkg_name(name, sizeof(name), "perl-PerlIO-utf8_strict"), false);

	ATF_CHECK_EQ(xbps_pkg_version("perl-PerlIO-utf8_strict"), NULL);
	ATF_CHECK_EQ(xbps_pkg_version("font-adobe-100dpi"), NULL);
	ATF_CHECK_EQ(xbps_pkg_version("font-adobe-100dpi-7.8"), NULL);
	ATF_CHECK_EQ(xbps_pkg_version("python-e_dbus"), NULL);
	ATF_CHECK_EQ(xbps_pkg_version("python-e_dbus-1"), NULL);
	ATF_CHECK_EQ(xbps_pkg_version("font-adobe-100dpi-1.8_blah"), NULL);

	ATF_REQUIRE_STREQ(xbps_pkg_version("font-adobe-100dpi-7.8_2"), "7.8_2");
	ATF_REQUIRE_STREQ(xbps_pkg_version("python-e_dbus-1_1"), "1_1");
	ATF_REQUIRE_STREQ(xbps_pkg_version("fs-utils-v1_1"), "v1_1");
	ATF_REQUIRE_STREQ(xbps_pkg_version("perl-Digest-1.17_01_1"), "1.17_01_1");
	ATF_REQUIRE_STREQ(xbps_pkg_version("perl-PerlIO-utf8_strict-0.007_1"), "0.007_1");

	ATF_REQUIRE_STREQ(xbps_pkg_revision("systemd_21-43_0"), "0");
	ATF_REQUIRE_STREQ(xbps_pkg_revision("systemd-43_1_0"), "0");
	ATF_REQUIRE_STREQ(xbps_pkg_revision("perl-Module-CoreList-5.20170715_24_1"), "1");

	ATF_CHECK_EQ(xbps_pkgpattern_name(name, sizeof(name), "systemd>=43"), true);
	ATF_REQUIRE_STREQ(name, "systemd");
	ATF_CHECK_EQ(xbps_pkgpattern_name(name, sizeof(name), "systemd>43"), true);
	ATF_REQUIRE_STREQ(name, "systemd");
	ATF_CHECK_EQ(xbps_pkgpattern_name(name, sizeof(name), "systemd<43"), true);
	ATF_REQUIRE_STREQ(name, "systemd");
	ATF_CHECK_EQ(xbps_pkgpattern_name(name, sizeof(name), "systemd<=43"), true);
	ATF_REQUIRE_STREQ(name, "systemd");
	ATF_CHECK_EQ(xbps_pkgpattern_name(name, sizeof(name), "systemd>4[3-9]?"), true);
	ATF_REQUIRE_STREQ(name, "systemd");
	ATF_CHECK_EQ(xbps_pkgpattern_name(name, sizeof(name), "systemd<4_1?"), true);
	ATF_REQUIRE_STREQ(name, "systemd");
	ATF_CHECK_EQ(xbps_pkgpattern_name(name, sizeof(name), "systemd-[0-9]*"), true);
	ATF_REQUIRE_STREQ(name, "systemd");
	ATF_CHECK_EQ(xbps_pkgpattern_name(name, sizeof(name), "*nslookup"), false);

	ATF_REQUIRE_STREQ((s = xbps_binpkg_arch("/path/to/foo-1.0_1.x86_64.xbps")), "x86_64");
	free(s);
	ATF_REQUIRE_STREQ((s = xbps_binpkg_arch("/path/to/foo-1.0_1.x86_64-musl.xbps")), "x86_64-musl");
	free(s);
	ATF_REQUIRE_STREQ((s = xbps_binpkg_arch("foo-1.0_1.x86_64-musl.xbps")), "x86_64-musl");
	free(s);
	ATF_REQUIRE_STREQ((s = xbps_binpkg_arch("foo-1.0_1.x86_64.xbps")), "x86_64");
	free(s);

	ATF_REQUIRE_STREQ((s = xbps_binpkg_pkgver("foo-1.0_1.x86_64.xbps")), "foo-1.0_1");
	free(s);
	ATF_REQUIRE_STREQ((s = xbps_binpkg_pkgver("foo-1.0_1.x86_64-musl.xbps")), "foo-1.0_1");
	free(s);
	ATF_REQUIRE_STREQ((s = xbps_binpkg_pkgver("/path/to/foo-1.0_1.x86_64.xbps")), "foo-1.0_1");
	free(s);
	ATF_REQUIRE_STREQ((s = xbps_binpkg_pkgver("/path/to/foo-1.0_1.x86_64-musl.xbps")), "foo-1.0_1");
	free(s);

	ATF_CHECK_EQ((s = xbps_binpkg_pkgver("foo-1.0.x86_64.xbps")), NULL);
	free(s);
	ATF_CHECK_EQ((s = xbps_binpkg_pkgver("foo-1.0.x86_64")), NULL);
	free(s);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, util_test);
	return atf_no_error();
}
