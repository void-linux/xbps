#! /usr/bin/env atf-sh
# Test that xbps-uhelper arch works as expected.

atf_test_case native

native_head() {
	atf_set "descr" "xbps-uhelper arch: native test"
}

native_body() {
	unset XBPS_ARCH XBPS_TARGET_ARCH
	atf_check -o "inline:$(uname -m)\n" -- xbps-uhelper -r "$PWD" arch
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
	mkdir -p xbps.d root
	unset XBPS_ARCH XBPS_TARGET_ARCH
	echo "architecture=foo" > xbps.d/arch.conf
	atf_check -o inline:"foo\n" -- xbps-uhelper -C $PWD/xbps.d -r root arch
}

atf_init_test_cases() {
	atf_add_test_case native
	atf_add_test_case env
	atf_add_test_case conf
}
