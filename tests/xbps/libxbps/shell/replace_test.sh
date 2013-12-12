#!/usr/bin/env atf-sh
#
# This test case reproduces the following issue:
#
#	- A-1.0_1 is installed.
#	- B-1.0_1 is going to be installed and it should replace pkg A, but
#		  incorrectly wants to replace A multiple times, i.e
#		  replaces="A>=0 A>=0"
#	- B-1.0_1 is installed.
#	- A-1.0_1 is registered in pkgdb, while it should not.

atf_test_case replace_dups

replace_dups_head() {
	atf_set "descr" "Tests for package replace: verify dup matches"
}

replace_dups_body() {
	mkdir some_repo root
	mkdir -p pkg_A/usr/bin pkg_B/usr/bin
	echo "A-1.0_1" > pkg_A/usr/bin/foo
	echo "B-1.0_1" > pkg_B/usr/bin/foo
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-create -A noarch -n B-1.0_1 -s "B pkg" --replaces "A>=0 A>=0" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -a *.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -y A
	atf_check_equal $? 0
	xbps-install -C empty.conf -r root --repository=$PWD/some_repo -y B
	atf_check_equal $? 0
	result=$(xbps-query -C empty.conf -r root -l|wc -l)
	atf_check_equal $result 1
}

atf_init_test_cases() {
	atf_add_test_case replace_dups
}
