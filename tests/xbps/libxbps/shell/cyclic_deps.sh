#!/usr/bin/env atf-sh
#

atf_test_case cyclic_dep_vpkg

cyclic_dep_vpkg_head() {
	atf_set "descr" "Tests for cyclic deps: pkg depends on a cyclic vpkg"
}

cyclic_dep_vpkg_body() {
	mkdir some_repo
	mkdir -p pkg_{A,B,C,D}/usr/bin
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" --provides "libGL-7.11_1" --dependencies "libGL>=7.11" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "libGL>=7.11" ../pkg_B
	atf_check_equal $? 0
	xbps-create -A noarch -n C-1.0_1 -s "C pkg" --dependencies "B>=0" ../pkg_C
	atf_check_equal $? 0

	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/some_repo -dy C
	atf_check_equal $? 0

	xbps-query  -r root --fulldeptree -x C
	atf_check_equal $? 0
}

atf_test_case cyclic_dep_vpkg2

cyclic_dep_vpkg2_head() {
	atf_set "descr" "Tests for cyclic deps: unresolved circular dependencies"
}

cyclic_dep_vpkg2_body() {
	mkdir some_repo
	mkdir -p pkg_{A,B,C,D}/usr/bin
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" --provides "libGL-7.11_1" --dependencies "xserver-abi-video<20" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "libGL>=7.11" --provides "xserver-abi-video-19_1" ../pkg_B
	atf_check_equal $? 0
	xbps-create -A noarch -n C-1.0_1 -s "C pkg" --dependencies "libGL>=7.11" ../pkg_C
	atf_check_equal $? 0

	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/some_repo -dy C
	atf_check_equal $? 40
}

atf_test_case cyclic_dep_full

cyclic_dep_full_head() {
	atf_set "descr" "Tests for cyclic deps: verify fulldeptree"
}

cyclic_dep_full_body() {
	atf_set "timeout" 5
	atf_expect_timeout "Known bug: see https://github.com/voidlinux/xbps/issues/92"
	mkdir some_repo
	mkdir -p pkg_{A,B}/usr/bin
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" --dependencies "B>=0" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "A>=0" ../pkg_B
	atf_check_equal $? 0

	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/some_repo -dy B
	atf_check_equal $? 0
	xbps-query -r root --fulldeptree -d B
	atf_check_equal $? 0
	xbps-remove -r root -Ryvd B
	atf_check_equal $? 0
}

atf_init_test_cases() {
	atf_add_test_case cyclic_dep_vpkg
	atf_add_test_case cyclic_dep_vpkg2
	atf_add_test_case cyclic_dep_full
}
