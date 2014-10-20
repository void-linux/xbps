#! /usr/bin/env atf-sh

atf_test_case instmode

instmode_head() {
	atf_set "descr" "installation mode: basic test"
}

instmode_body() {
	mkdir some_repo
	mkdir -p pkg_a/usr/bin pkg_b/usr/bin
	touch -f pkg_a/usr/bin/foo pkg_b/usr/bin/blah

	cd some_repo
	xbps-create -A noarch -n a-1.0_1 -s "foo pkg" ../pkg_a
	atf_check_equal $? 0
	xbps-create -A noarch -n b-1.0_1 -s "foo pkg" --dependencies "a>=0" ../pkg_b
	atf_check_equal $? 0
	xbps-rindex -a *.xbps
	atf_check_equal $? 0

	cd ..
	xbps-install -r root -c null.conf --repository=$PWD/some_repo -yd b
	atf_check_equal $? 0

	out=$(xbps-query -r root --property=automatic-install a)
	atf_check_equal $out yes
	out=$(xbps-query -r root --property=automatic-install b)
	rv=0
	if [ "$out" != "" -a "$out" != "no" ]; then
		rv=1
	fi
	atf_check_equal $rv 0
}

atf_test_case instmode_auto

instmode_auto_head() {
	atf_set "descr" "installation mode: basic test for automatic mode"
}

instmode_auto_body() {
	mkdir some_repo
	mkdir -p pkg_a/usr/bin pkg_b/usr/bin
	touch -f pkg_a/usr/bin/foo pkg_b/usr/bin/blah

	cd some_repo
	xbps-create -A noarch -n a-1.0_1 -s "foo pkg" ../pkg_a
	atf_check_equal $? 0
	xbps-create -A noarch -n b-1.0_1 -s "foo pkg" --dependencies "a>=0" ../pkg_b
	atf_check_equal $? 0
	xbps-rindex -a *.xbps
	atf_check_equal $? 0

	cd ..
	xbps-install -r root -c null.conf --repository=$PWD/some_repo -Ayd b
	atf_check_equal $? 0

	out=$(xbps-query -r root --property=automatic-install a)
	atf_check_equal $out yes
	out=$(xbps-query -r root --property=automatic-install b)
	atf_check_equal $out yes
}

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
	atf_add_test_case instmode
	atf_add_test_case instmode_auto
	atf_add_test_case instmode_update
	atf_add_test_case instmode_reinstall
}
