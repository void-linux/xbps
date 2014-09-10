#! /usr/bin/env atf-sh

# 1- preserve installation mode on updates
atf_test_case instmode_update

instmode_update_head() {
	atf_set "descr" "Installation mode: preserve on update"
}

instmode_update_body() {
	mkdir some_repo
	mkdir -p pkg_A/usr/bin pkg_B/usr/bin
	touch -f pkg_A/usr/bin/foo pkg_B/usr/bin/blah

	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -a *.xbps
	atf_check_equal $? 0

	cd ..
	xbps-install -r root -C null.conf --repository=$PWD/some_repo -Ayd A-1.0_1
	atf_check_equal $? 0

	cd some_repo
	xbps-create -A noarch -n A-1.1_1 -s "foo pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -a *.xbps
	atf_check_equal $? 0

	cd ..
	xbps-install -r root -C null.conf --repository=$PWD/some_repo -yud
	atf_check_equal $? 0
	out=$(xbps-query -r root --property=automatic-install A)
	atf_check_equal $out yes
}

# 2- preserve installation mode on reinstall
atf_test_case instmode_reinstall

instmode_reinstall_head() {
	atf_set "descr" "Installation mode: preserve on reinstall"
}

instmode_reinstall_body() {
	mkdir some_repo
	mkdir -p pkg_A/usr/bin
	touch -f pkg_A/usr/bin/foo

	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -a *.xbps
	atf_check_equal $? 0
	cd ..
	xbps-install -r root -C null.conf --repository=$PWD/some_repo -Ayd A-1.0_1
	atf_check_equal $? 0
	xbps-install -r root -C null.conf --repository=$PWD/some_repo -yfd A-1.0_1
	atf_check_equal $? 0
	out=$(xbps-query -r root --property=automatic-install A)
	atf_check_equal $out yes
}

atf_init_test_cases() {
	atf_add_test_case instmode_update
	atf_add_test_case instmode_reinstall
}
