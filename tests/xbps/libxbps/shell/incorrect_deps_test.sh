#!/usr/bin/env atf-sh
#
atf_test_case incorrect_dep

incorrect_dep_head() {
	atf_set "descr" "Tests for package deps: pkg depends on itself"
}

incorrect_dep_body() {
	mkdir some_repo
	mkdir -p pkg_B/usr/bin
	echo "B-1.0_1" > pkg_B/usr/bin/foo
	cd some_repo
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "B>=0" ../pkg_B
	atf_check_equal $? 0

	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -dy B
	atf_check_equal $? 0
}

atf_test_case incorrect_dep_vpkg

incorrect_dep_vpkg_body() {
	mkdir some_repo
	mkdir -p pkg_A/usr/bin pkg_B/usr/bin
	echo "A-1.0_1" > pkg_A/usr/bin/foo
	echo "B-1.0_1" > pkg_B/usr/bin/foo
	cd some_repo
	xbps-create -A noarch -n A-10.1.1_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "A>=7.11_1" --provides "A-331.67_1" ../pkg_B
	atf_check_equal $? 0

	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -dy A
	atf_check_equal $? 0
	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -dy B
	atf_check_equal $? 0
}

atf_test_case incorrect_dep_issue45

incorrect_dep_issue45_head() {
	atf_set "descr" "Test for package deps: pkg depends on itself (issue #45: https://github.com/voidlinux/xbps/issues/45)"
}

incorrect_dep_issue45_body() {
	mkdir some_repo
	mkdir -p pkg_A/usr/bin pkg_B/usr/bin pkg_C/usr/bin pkg_D/usr/bin
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" --dependencies "A>=0" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "A>=0" ../pkg_B
	atf_check_equal $? 0

	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -dy B
	atf_check_equal $? 0
	xbps-query -C empty.conf -r root -l
	atf_check_equal $? 0
	xbps-pkgdb -C empty.conf -r root -a
	atf_check_equal $? 0
}

atf_test_case incorrect_dep_dups

incorrect_dep_dups_head() {
	atf_set "descr" "Test for package deps: duplicated deps in fulldeptree"
}

incorrect_dep_dups_body() {
	mkdir some_repo
	mkdir -p pkg_A/usr/bin pkg_B/usr/bin
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --dependencies "A>=0 A>=0" ../pkg_B
	atf_check_equal $? 0

	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -dy B
	atf_check_equal $? 0

	xbps-query -C empty.conf -r root --fulldeptree -x B
	out=$(xbps-query -C empty.conf -r root --fulldeptree -x B)
	set -- $out
	atf_check_equal $# 2
	atf_check_equal "$1 $2" "B-1.0_1 A-1.0_1"
}

atf_init_test_cases() {
	atf_add_test_case incorrect_dep
	atf_add_test_case incorrect_dep_vpkg
	atf_add_test_case incorrect_dep_issue45
	atf_add_test_case incorrect_dep_dups
}
