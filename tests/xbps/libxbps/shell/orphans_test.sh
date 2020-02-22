#!/usr/bin/env atf-sh

atf_test_case tc1

tc1_head() {
	atf_set "descr" "Tests for pkg orphans: https://github.com/void-linux/xbps/issues/234"
}

tc1_body() {
	mkdir -p repo pkg_A
	cd repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "A>=0" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n C-1.0_1 -s "C pkg" --dependencies "B>=0" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n D-1.0_1 -s "D pkg" --dependencies "C>=0" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -r root --repo=repo -yd D
	atf_check_equal $? 0
	out="$(xbps-query -r root -m)"
	atf_check_equal $? 0
	atf_check_equal "$out" "D-1.0_1"
	xbps-remove -r root -Ryd D
	atf_check_equal $? 0
	out="$(xbps-query -r root -l|wc -l)"
	atf_check_equal $? 0
	atf_check_equal "$out" "0"

	xbps-install -r root --repo=repo -yd A
	atf_check_equal $? 0
	out="$(xbps-query -r root -m)"
	atf_check_equal $? 0
	atf_check_equal "$out" "A-1.0_1"
	xbps-install -r root --repo=repo -yd D
	atf_check_equal $? 0
	xbps-remove -r root -Ryd D
	atf_check_equal $? 0
	out="$(xbps-query -r root -m)"
	atf_check_equal $? 0
	atf_check_equal "$out" "A-1.0_1"

	xbps-install -r root --repo=repo -yd D
	atf_check_equal $? 0
	xbps-pkgdb -r root -m auto A
	atf_check_equal $? 0
	xbps-remove -r root -Ryd D
	atf_check_equal $? 0
	out="$(xbps-query -r root -l|wc -l)"
	atf_check_equal $? 0
	atf_check_equal "$out" "0"

}

atf_init_test_cases() {
	atf_add_test_case tc1
}
