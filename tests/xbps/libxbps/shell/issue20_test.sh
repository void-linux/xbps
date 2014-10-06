#! /usr/bin/env atf-sh

# xbps issue #20.
# How to reproduce it:
#	Create pkg a-0.1_1 containing 1 file and 1 symlink:
#
#		/foo
#		/blah -> foo
#
#	Create pkg a-0.2_1 containing 1 file and 1 symlink (inverted):
#
#		/foo -> blah
#		/blah
#
#	Upgrade pkg a to 0.2.

atf_test_case issue20

issue20_head() {
	atf_set "descr" "xbps issue #20 (https://github.com/xtraeme/xbps/issues/18)"
}

issue20_body() {
	mkdir repo
	cd repo
	mkdir pkg_a
	touch pkg_a/foo
	ln -s foo pkg_a/blah
	xbps-create -A noarch -n a-0.1_1 -s "pkg a" pkg_a
	atf_check_equal $? 0
	rm -rf pkg_a
	xbps-rindex -a *.xbps
	atf_check_equal $? 0
	xbps-install -C null.conf -r rootdir --repository=$PWD -yvd a
	atf_check_equal $? 0

	mkdir pkg_a
	touch pkg_a/blah
	ln -s blah pkg_a/foo
	xbps-create -A noarch -n a-0.2_1 -s "pkg a" pkg_a
	atf_check_equal $? 0
	rm -rf pkg_a
	xbps-rindex -a *.xbps
	atf_check_equal $? 0
	xbps-install -C null.conf -r rootdir --repository=$PWD -yuvd
	atf_check_equal $? 0
	tgt=$(readlink rootdir/foo)
	rval=1
	if [ -f rootdir/blah -a "$tgt" = "blah" ]; then
		rval=0
	fi
	atf_check_equal $rval 0
}

atf_init_test_cases() {
	atf_add_test_case issue20
}
