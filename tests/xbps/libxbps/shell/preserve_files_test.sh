#!/usr/bin/env atf-sh

atf_test_case tc1

tc1_head() {
	atf_set "descr" "Tests for pkg install/upgrade with preserved files: preserve on-disk files with globs"
}

tc1_body() {
	mkdir some_repo
	mkdir -p pkg_A/usr/bin
	echo "blahblah" > pkg_A/usr/bin/blah
	echo "foofoo" > pkg_A/usr/bin/foo
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a *.xbps
	atf_check_equal $? 0
	cd ..

	mkdir -p root/usr/bin
	mkdir -p root/xbps.d
	echo "modified blahblah" > root/usr/bin/blah
	echo "modified foofoo" > root/usr/bin/foo

	echo "preserve=/usr/bin/*" > root/xbps.d/foo.conf

	xbps-install -C xbps.d -r root --repository=$PWD/some_repo -yd A
	atf_check_equal $? 0

	rv=1
	if [ "$(cat root/usr/bin/blah)" = "modified blahblah" -a "$(cat root/usr/bin/foo)" = "modified foofoo" ]; then
		rv=0
	fi
	atf_check_equal $rv 0
}

atf_test_case tc2

tc2_head() {
	atf_set "descr" "Tests for pkg install/upgrade with preserved files: preserve on-disk files without globs"
}

tc2_body() {
	mkdir some_repo
	mkdir -p pkg_A/usr/bin
	echo "blahblah" > pkg_A/usr/bin/blah
	echo "foofoo" > pkg_A/usr/bin/foo
	cd some_repo
	xbps-create -A noarch -n A-1.0_1 -s "A pkg" ../pkg_A
	atf_check_equal $? 0
	xbps-rindex -d -a *.xbps
	atf_check_equal $? 0
	cd ..

	mkdir -p root/usr/bin
	mkdir -p root/xbps.d
	echo "modified blahblah" > root/usr/bin/blah
	echo "modified foofoo" > root/usr/bin/foo

	printf "preserve=/usr/bin/blah\npreserve=/usr/bin/foo\n" > root/xbps.d/foo.conf

	echo "foo.conf" >&2
	cat foo.conf >&2

	xbps-install -C xbps.d -r root --repository=$PWD/some_repo -yd A
	atf_check_equal $? 0

	rv=1
	if [ "$(cat root/usr/bin/blah)" = "modified blahblah" -a "$(cat root/usr/bin/foo)" = "modified foofoo" ]; then
		rv=0
	fi

	echo "root/usr/bin/blah" >&2
	cat root/usr/bin/blah >&2
	echo "root/usr/bin/foo" >&2
	cat root/usr/bin/foo >&2

	atf_check_equal $rv 0
}

atf_init_test_cases() {
	atf_add_test_case tc1
	atf_add_test_case tc2
}
