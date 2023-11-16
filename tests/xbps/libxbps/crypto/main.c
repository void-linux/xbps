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

#include <atf-c/macros.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>

#include <atf-c.h>
#include <xbps/crypto.h>
#include <xbps.h>

#include "../lib/external/codecs.h"

static const char *a_pub_content =
	"untrusted comment: minisign public key 6161616161616161\n"
	"YWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFh\n";

static const char *test_key_content UNUSED =
	"untrusted comment: minisign encrypted secret key\n"
	"RWQAAEIyAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
	"AAAr5smWD4A8c3+JH5wEe+7C5dQbgSIS8lnvgSUiMGIYGbaZMh+wzTUux5FGmxu4PrfGd"
	"NzVobtnluFTeELWHaqyU0dQhO5hzA7AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
	"AAAA=\n";

static const char *test_pub_content =
	"untrusted comment: minisign public key CDF1003E58269BAF\n"
	"RWSvmyZYPgDxzR5FGmxu4PrfGdNzVobtnluFTeELWHaqyU0dQhO5hzA7\n";

static const char *test_pub_rn_content =
	"untrusted comment: minisign public key CDF1003E58269BAF\r\n"
	"RWSvmyZYPgDxzR5FGmxu4PrfGdNzVobtnluFTeELWHaqyU0dQhO5hzA7\r\n";

static const char *enobufs_pub_content =
	"untrusted comment: minisign public key CDF1003E58269BAF\n"
	"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n";


ATF_TC(xbps_pubkey_decode);

ATF_TC_HEAD(xbps_pubkey_decode, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_pubkey_decode");
}

ATF_TC_BODY(xbps_pubkey_decode, tc)
{
	struct xbps_pubkey pubkey;
	ATF_CHECK_EQ(xbps_pubkey_decode(&pubkey, "RWRfQ4v9r2BE5vDHBlJfZ1UL7byoLYM+jq22Sc34O+w0hW7NOtQZZ0nT"), 0);
	ATF_CHECK_EQ(xbps_pubkey_decode(&pubkey, "RWRfQ4v9r2BE5vDHBlJfZ1UL7byoLYM+jq22Sc34O+w0hW7NOtQZZ0"), -EINVAL);
	ATF_CHECK_EQ(xbps_pubkey_decode(&pubkey, "RWRfQ4v9r2BE5vDHBlJfZ1UL7byoLYM+jq22Sc34O+w0hW7NOtQZZ0nTAA"), -EINVAL);
	/* algorith set to XX */
	ATF_CHECK_EQ(xbps_pubkey_decode(&pubkey, "WFhfQ4v9r2BE5vDHBlJfZ1UL7byoLYM+jq22Sc34O+w0hW7NOtQZZ0nT"), -ENOTSUP);
}

ATF_TC(xbps_pubkey_encode);

ATF_TC_HEAD(xbps_pubkey_encode, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_pubkey_encode");
}

ATF_TC_BODY(xbps_pubkey_encode, tc)
{
	char pubkey_s[BASE64_ENCODED_LEN(sizeof(struct xbps_pubkey), BASE64_VARIANT_ORIGINAL)];
	struct xbps_pubkey pubkey;
	int r;
	memset(&pubkey, 'a', sizeof(pubkey));

	r = xbps_pubkey_encode(&pubkey, pubkey_s, sizeof(pubkey_s));
	fprintf(stderr, "%d\n", r);
	ATF_REQUIRE_EQ(r, 0);
	ATF_CHECK_STREQ(pubkey_s, "YWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFh");
}

ATF_TC(xbps_pubkey_read);

ATF_TC_HEAD(xbps_pubkey_read, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_pubkey_read");
}

ATF_TC_BODY(xbps_pubkey_read, tc)
{
	struct xbps_pubkey pubkey;
	int fd;

	memset(&pubkey, 'a', sizeof(pubkey));
	atf_utils_create_file("a.pub", "%s", a_pub_content);

	ATF_REQUIRE((fd = open("a.pub", O_RDONLY)) != -1);
	ATF_REQUIRE_EQ(xbps_pubkey_read(&pubkey, fd), -ENOTSUP);
	close(fd);

	atf_utils_create_file("test.pub", "%s", test_pub_content);
	ATF_REQUIRE((fd = open("test.pub", O_RDONLY)) != -1);
	ATF_REQUIRE_EQ(xbps_pubkey_read(&pubkey, fd), 0);
	close(fd);

	atf_utils_create_file("test.pub", "%s", test_pub_rn_content);
	ATF_REQUIRE((fd = open("test.pub", O_RDONLY)) != -1);
	ATF_REQUIRE_EQ(xbps_pubkey_read(&pubkey, fd), 0);
	close(fd);

	atf_utils_create_file("enobufs.pub", "%s", enobufs_pub_content);
	ATF_REQUIRE((fd = open("enobufs.pub", O_RDONLY)) != -1);
	ATF_REQUIRE_EQ(xbps_pubkey_read(&pubkey, fd), -ENOBUFS);
	close(fd);
}

ATF_TC(xbps_pubkey_write);

ATF_TC_HEAD(xbps_pubkey_write, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_pubkey_write");
}

ATF_TC_BODY(xbps_pubkey_write, tc)
{
	struct xbps_pubkey pubkey;
	memset(&pubkey, 'a', sizeof(pubkey));
	ATF_REQUIRE(xbps_pubkey_write(&pubkey, "test.pub") == 0);
	ATF_CHECK(atf_utils_compare_file("test.pub", a_pub_content));

}

static const char *a_sec_content =
	"untrusted comment: minisign encrypted secret key\n"
	"YWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFh"
	"YWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFh"
	"YWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFh"
	"YWFhYWE=\n";


ATF_TC(xbps_seckey_write);

ATF_TC_HEAD(xbps_seckey_write, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test xbps_seckey_write");
}

ATF_TC_BODY(xbps_seckey_write, tc)
{
	struct xbps_seckey seckey;
	memset(&seckey, 'a', sizeof(seckey));
	ATF_REQUIRE_EQ(xbps_seckey_write(&seckey, NULL, "test.key"), 0);
	ATF_CHECK(atf_utils_compare_file("test.key", a_sec_content));
	ATF_REQUIRE_EQ(xbps_seckey_write(&seckey, NULL, "test.key"), -EEXIST);

}

ATF_TP_ADD_TCS(tp)
{
	xbps_debug_level = 1;
	ATF_TP_ADD_TC(tp, xbps_pubkey_decode);
	ATF_TP_ADD_TC(tp, xbps_pubkey_encode);
	ATF_TP_ADD_TC(tp, xbps_pubkey_read);
	ATF_TP_ADD_TC(tp, xbps_pubkey_write);
	ATF_TP_ADD_TC(tp, xbps_seckey_write);
	return atf_no_error();
}
