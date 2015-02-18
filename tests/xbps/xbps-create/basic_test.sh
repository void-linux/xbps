#! /usr/bin/env atf-sh
# Test that xbps-create(8) works as expected.

atf_test_case hardlinks_size

hardlinks_size_head() {
	atf_set "descr" "xbps-create(8): installed-size behaviour with hardlinks"
}

hardlinks_size_body() {
	mkdir -p repo pkg_A
	echo 123456789 > pkg_A/file00
	ln pkg_A/file00 pkg_A/file01
	ln pkg_A/file00 pkg_A/file02
	ln pkg_A/file00 pkg_A/file03
	cd repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	cd ..
	xbps-rindex -d -a repo/*.xbps
	atf_check_equal $? 0
	result="$(xbps-query -r root --repository=repo -p installed_size foo)"
	expected="10B"
	rv=0
	if [ "$result" != "$expected" ]; then
		echo "result: $result"
		echo "expected: $expected"
		rv=1
	fi
	atf_check_equal $rv 0
}

atf_test_case symlink_relative_target

symlink_relative_target_head() {
	atf_set "descr" "xbps-create(8): relative symlinks in destdir must be absolute"
}

symlink_relative_target_body() {
	mkdir -p repo pkg_A/usr/include/gsm
	touch -f pkg_A/usr/include/gsm/gsm.h
	cd pkg_A/usr/include
	ln -s gsm/gsm.h gsm.h
	cd ../../../repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	cd ..
	xbps-rindex -d -a repo/*.xbps
	atf_check_equal $? 0
	result="$(xbps-query -r root --repository=repo -f foo|tr -d '\n')"
	expected="/usr/include/gsm/gsm.h/usr/include/gsm.h -> /usr/include/gsm/gsm.h"
	rv=0
	if [ "$result" != "$expected" ]; then
		echo "result: $result"
		echo "expected: $expected"
		rv=1
	fi
	atf_check_equal $rv 0
}

atf_init_test_cases() {
	atf_add_test_case hardlinks_size
	atf_add_test_case symlink_relative_target
}
