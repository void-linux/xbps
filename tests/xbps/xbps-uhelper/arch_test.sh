#! /usr/bin/env atf-sh
# Test that xbps-uhelper arch works as expected.

atf_test_case native

native_head() {
	atf_set "descr" "xbps-uhelper arch: native test"
}

native_body() {
	atf_check_equal $(xbps-uhelper -r $PWD arch) $(uname -m)
}

atf_test_case env

env_head() {
	atf_set "descr" "xbps-uhelper arch: envvar override test"
}
env_body() {
	export XBPS_ARCH=foo
	atf_check_equal $(xbps-uhelper -r $PWD arch) foo
}

atf_test_case conf

conf_head() {
	atf_set "descr" "xbps-uhelper arch: configuration override test"
}
conf_body() {
	mkdir -p xbps.d
	echo "architecture=x86_64-musl" > xbps.d/arch.conf
	atf_check_equal $(xbps-uhelper -C $PWD/xbps.d arch) x86_64-musl
}

atf_init_test_cases() {
	atf_add_test_case native
	atf_add_test_case env
	atf_add_test_case conf
}
