/*-
 * Copyright (c) 2026 Juan Romero Pardines.
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

/*
 * A SubjectPublicKeyInfo whose RSA public exponent is zero still decodes
 * with OpenSSL, but BN_num_bytes() of the exponent is 0.  xbps_pubkey2fp()
 * used to malloc(0) and then read byte 0 of that allocation, so feed it such
 * a key and make sure it is rejected instead of dereferencing the empty
 * buffer.
 */
static const char pubkey_zero_exponent[] =
    "-----BEGIN PUBLIC KEY-----\n"
    "MBowDQYJKoZIhvcNAQEBBQADCQAwBgIBCwIBAA==\n"
    "-----END PUBLIC KEY-----\n";

/* A valid 2048-bit RSA public key: the fingerprint must still compute. */
static const char pubkey_valid[] =
    "-----BEGIN PUBLIC KEY-----\n"
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAxEEPApsMmdpPhwHNOOnq\n"
    "gPMDaXMLf+j8/h6m5hlMIVGVl4It8GqynbCfF9LznsJZTypTYZGWp3cJoEUVGGCe\n"
    "5QE5kCs1s/iyeEGoGJSHdvJE2Yv4TlgS6pqKUke/mif/Vjlgpik6kuutIHSs89yH\n"
    "tp/6ufjGuej/hjlNjT3LAqgX1gP8W0gaZI8+4zfWdPnqax3NFV0Qi8d6N9oAiOwe\n"
    "oFDY5CjU9S5yEJ4gMkvfrvv6I9Jed4cRf4WtBQmEuvwFi1fo8pk6nK/1wWKbO3zD\n"
    "+NjZjb/qvds8px3m8TIay6zDbLG0iI9SpqvMVVMfb2zrIUGR/XKStAI1eblM8Xt3\n"
    "oQIDAQAB\n"
    "-----END PUBLIC KEY-----\n";

ATF_TC(pubkey2fp_test);
ATF_TC_HEAD(pubkey2fp_test, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test xbps_pubkey2fp handling of RSA public keys");
}
ATF_TC_BODY(pubkey2fp_test, tc)
{
	xbps_data_t d;
	char *fp;

	d = xbps_data_create_data(pubkey_zero_exponent,
	    strlen(pubkey_zero_exponent));
	ATF_REQUIRE(d != NULL);
	fp = xbps_pubkey2fp(d);
	ATF_CHECK_EQ(fp, NULL);
	free(fp);
	xbps_object_release(d);

	d = xbps_data_create_data(pubkey_valid, strlen(pubkey_valid));
	ATF_REQUIRE(d != NULL);
	fp = xbps_pubkey2fp(d);
	ATF_CHECK(fp != NULL);
	free(fp);
	xbps_object_release(d);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, pubkey2fp_test);
	return atf_no_error();
}
