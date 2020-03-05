#!/usr/bin/env atf-sh

atf_test_case issue_245

issue_245_head() {
	atf_set "descr" "Tests update of package with removed and uptodate revdeps"
}

issue_245_body() {
	mkdir some_repo
	mkdir destdir

	cd some_repo
	xbps-create -A noarch -n libX-1_1 -s "libX pkg" ../destdir
	atf_check_equal $? 0
	xbps-create -A noarch -n cmdA-1_1 -s "cmdA pkg" --dependencies "libX>=1_1" ../destdir
	atf_check_equal $? 0
	xbps-create -A noarch -n cmdB-1_1 -s "cmdB pkg" --dependencies "libX>=1_1" ../destdir
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	xbps-install -r root --repository=$PWD/some_repo -yd cmdA cmdB
	atf_check_equal $? 0

	cd some_repo
	xbps-create -A noarch -n libX-1_2 -s "libX pkg" --replaces "cmdA>=1_1" ../destdir
	atf_check_equal $? 0
	xbps-rindex -d -a $PWD/*.xbps
	atf_check_equal $? 0
	cd ..

	# was failing here with <libX-1_2 in transaction breaks installed pkg `cmdB-1_1'>
	xbps-install -r root --repository=$PWD/some_repo -uyd
	atf_check_equal $? 0

	xbps-query -r root cmdA
	atf_check_equal $? 2

	xbps-query -r root libX-1_2
	atf_check_equal $? 0
}

atf_init_test_cases() {
	atf_add_test_case issue_245
}
