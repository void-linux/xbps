#! /usr/bin/env atf-sh

# 1st test: modified configuration file on disk, unmodified on upgrade.
#	result: keep file as is on disk.
atf_test_case tc1

tc1_head() {
	atf_set "descr" "Tests for configuration file handling: on-disk modified, upgrade unmodified"
}

tc1_body() {
	mkdir repo
	cd repo
	mkdir pkg_a
	echo "fooblah" > pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.1_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	rm -rf pkg_a
	xbps-rindex -a *.xbps
	atf_check_equal $? 0
	xbps-install -C null.conf -r rootdir --repository=$PWD -yv a
	atf_check_equal $? 0

	sed -e 's,fooblah,blahfoo,' -i rootdir/cf1.conf
	mkdir pkg_a
	echo "fooblah" > pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.2_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	xbps-rindex -a *.xbps
	rm -rf pkg_a
	atf_check_equal $? 0
	xbps-install -C null.conf -r rootdir --repository=$PWD -yuv
	atf_check_equal $? 0
	result="$(cat rootdir/cf1.conf)"
	rval=1
	if [ "${result}" = "blahfoo" ]; then
		rval=0
	fi
	atf_check_equal $rval 0
}

# 2nd test: modified configuration file on disk, modified on upgrade.
#	result: install new file as "<conf_file>.new-<version>".
atf_test_case tc2

tc2_head() {
	atf_set "descr" "Tests for configuration file handling: on-disk modified, upgrade modified"
}

tc2_body() {
	mkdir repo
	cd repo
	mkdir pkg_a
	echo "fooblah" > pkg_a/cf1.conf
	chmod 755 pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.1_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	rm -rf pkg_a
	xbps-rindex -a *.xbps
	atf_check_equal $? 0
	xbps-install -C null.conf -r rootdir --repository=$PWD -yv a
	atf_check_equal $? 0

	sed -e 's,fooblah,blahfoo,' -i rootdir/cf1.conf
	chmod 644 rootdir/cf1.conf
	mkdir pkg_a
	echo "bazbar" > pkg_a/cf1.conf
	chmod 755 pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.2_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	xbps-rindex -a *.xbps
	rm -rf pkg_a
	atf_check_equal $? 0
	xbps-install -C null.conf -r rootdir --repository=$PWD -yuv
	atf_check_equal $? 0
	result="$(cat rootdir/cf1.conf)"
	rval=1
	if [ "${result}" = "blahfoo" ]; then
		rval=0
	fi
	atf_check_equal $rval 0
	rval=1
	if [ -s rootdir/cf1.conf.new-0.2_1 ]; then
		rval=0
	fi
	atf_check_equal $rval 0
}

# 3rd test: modified configuration file on disk, unmodified on upgrade.
#	result: keep file on disk as is.
atf_test_case tc3

tc3_head() {
	atf_set "descr" "Tests for configuration file handling: on-disk modified, upgrade unmodified"
}

tc3_body() {
	mkdir repo
	cd repo
	mkdir pkg_a
	echo "fooblah" > pkg_a/cf1.conf
	chmod 755 pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.1_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	rm -rf pkg_a
	xbps-rindex -a *.xbps
	atf_check_equal $? 0
	xbps-install -C null.conf -r rootdir --repository=$PWD -yv a
	atf_check_equal $? 0

	sed -e 's,fooblah,blahfoo,' -i rootdir/cf1.conf
	chmod 644 rootdir/cf1.conf
	mkdir pkg_a
	echo "fooblah" > pkg_a/cf1.conf
	chmod 755 pkg_a/cf1.conf
	xbps-create -A noarch -n a-0.2_1 -s "pkg a" --config-files "/cf1.conf" pkg_a
	atf_check_equal $? 0
	xbps-rindex -a *.xbps
	rm -rf pkg_a
	atf_check_equal $? 0
	xbps-install -C null.conf -r rootdir --repository=$PWD -yuv
	atf_check_equal $? 0
	result="$(cat rootdir/cf1.conf)"
	rval=1
	if [ "${result}" = "blahfoo" ]; then
		rval=0
	fi
	atf_check_equal $rval 0
}

atf_init_test_cases() {
	atf_add_test_case tc1
	atf_add_test_case tc2
	atf_add_test_case tc3
}
