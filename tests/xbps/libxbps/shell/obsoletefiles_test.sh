#! /usr/bin/env atf-sh

# 1- find obsolete files on reinstall (and downgrades).
atf_test_case reinstall_obsoletes

reinstall_obsoletes_update_head() {
	atf_set "descr" "Test that obsolete files are removed on reinstall"
}

reinstall_obsoletes_body() {
	#
	# Simulate a pkg downgrade and make sure that obsolete files
	# are found and removed properly.
	#
	mkdir some_repo
	mkdir -p pkg_A/usr/bin pkg_B/usr/sbin
	touch -f pkg_A/usr/bin/foo pkg_A/usr/bin/blah
	touch -f pkg_B/usr/sbin/baz

	cd some_repo
	xbps-create -A noarch -n A-1.1_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -a *.xbps
	atf_check_equal $? 0

	cd ..
	xbps-install -r root -C null.conf --repository=$PWD/some_repo -yv A-1.1_1
	atf_check_equal $? 0

	rm -f some_repo/*
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "foo pkg" ../pkg_B
	atf_check_equal $? 0
	xbps-rindex -a *.xbps
	atf_check_equal $? 0

	cd ..
	xbps-install -r root -C null.conf --repository=$PWD/some_repo -yvf A-1.0_1
	atf_check_equal $? 0

	rv=0
	if [ ! -f root/usr/sbin/baz ]; then
		rv=1
	fi
	for f in usr/bin/foo usr/bin/blah; do
		if [ -f root/$f ]; then
			rv=1
		fi
	done
	atf_check_equal $rv 0
}

atf_init_test_cases() {
	atf_add_test_case reinstall_obsoletes
}
