#!/usr/bin/env atf-sh

atf_test_case update_hold

update_hold_head() {
	atf_set "descr" "Tests for pkg update: pkg is on hold mode"
}

update_hold_body() {
	mkdir -p repo pkg_A
	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -r root --repository=$PWD/repo -yd A
	atf_check_equal $? 0
	xbps-pkgdb -r root -m hold A
	atf_check_equal $? 0
	out=$(xbps-query -r root -H)
	atf_check_equal $out A-1.0_1
	cd repo
	xbps-create -A noarch -n A-1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	out=$(xbps-install -r root --repository=$PWD/repo -un)
	atf_check_equal "$out" "A-1.1_1 hold noarch $PWD/repo"
	xbps-install -r root --repository=$PWD/repo -yuvd
	atf_check_equal $? 0
	out=$(xbps-query -r root -p pkgver A)
	atf_check_equal $out A-1.0_1

}

atf_init_test_cases() {
	atf_add_test_case update_hold
}
