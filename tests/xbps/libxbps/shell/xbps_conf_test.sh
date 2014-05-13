#! /usr/bin/env atf-sh

# Tests to verify that xbps.conf features work

atf_test_case tc1

tc1_head() {
	atf_set "descr" "Tests for xbps.conf: include "
}

tc1_body() {
}

atf_init_test_cases() {
	atf_add_test_case tc1
}
