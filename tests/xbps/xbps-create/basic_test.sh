#! /usr/bin/env atf-sh
# Test that xbps-create(1) works as expected.

atf_test_case hardlinks_size

hardlinks_size_head() {
	atf_set "descr" "xbps-create(1): installed-size behaviour with hardlinks"
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
	atf_set "descr" "xbps-create(1): relative symlinks in destdir must be absolute"
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

atf_test_case symlink_sanitize

symlink_sanitize_head() {
	atf_set "descr" "xbps-create(1): symlinks must be properly sanitized"
}

symlink_sanitize_body() {
	mkdir -p repo pkg_A/usr/bin
	ln -s //usr/bin pkg_A/bin

	cd repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	cd ..
	xbps-rindex -d -a repo/*.xbps
	atf_check_equal $? 0
	result="$(xbps-query -r root --repository=repo -f foo|tr -d '\n')"
	expected="/bin -> /usr/bin"
	rv=0
	if [ "$result" != "$expected" ]; then
		echo "result: $result"
		echo "expected: $expected"
		rv=1
	fi
	atf_check_equal $rv 0
}

atf_test_case symlink_relative_target_cwd

symlink_relative_target_cwd_head() {
	atf_set "descr" "xbps-create(1): relative symlinks to cwd in destdir must be absolute"
}

symlink_relative_target_cwd_body() {
	mkdir -p repo pkg_A/usr/include/gsm
	touch -f pkg_A/usr/include/gsm/gsm.h
	cd pkg_A/usr/include
	ln -s ./gsm/gsm.h gsm.h
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

atf_test_case restore_mtime

restore_mtime_head() {
	atf_set "descr" "xbps-create(1): restore file mtime as user"
}

restore_mtime_body() {
	mkdir -p repo pkg_A/usr/include/gsm
	touch -f pkg_A/usr/include/gsm/gsm.h

	cd repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" ../pkg_A
	atf_check_equal $? 0
	cd ..
	xbps-rindex -d -a repo/*.xbps
	atf_check_equal $? 0
	xbps-install -r root --repository=repo -yvd foo

	expected=$(stat --printf='%Y' pkg_A/usr/include/gsm/gsm.h)
	result=$(stat --printf='%Y' root/usr/include/gsm/gsm.h)

	atf_check_equal "$expected" "$result"
}

atf_test_case reproducible_pkg

reproducible_pkg_head() {
	atf_set "descr" "xbps-create(1): generate identical packages"
}

reproducible_pkg_body() {
	mkdir -p repo pkg_A/usr/include/gsm

	# identical content
	echo QWERTY > pkg_A/usr/include/gsm/gsm.h
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" pkg_A
	atf_check_equal $? 0
	mv foo-1.0_1.noarch.xbps foo-1.0_1.noarch.xbps.orig
	atf_check_equal $? 0
	sleep 1
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" pkg_A
	atf_check_equal $? 0
	cmp -s foo-1.0_1.noarch.xbps foo-1.0_1.noarch.xbps.orig
	atf_check_equal $? 0

	# modified content
	echo QWERTZ > pkg_A/usr/include/gsm/gsm.h
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" pkg_A
	atf_check_equal $? 0
	cmp -s foo-1.0_1.noarch.xbps foo-1.0_1.noarch.xbps.orig
	atf_check_equal $? 1
}

atf_test_case fifo_file

reject_fifo_file_head() {
	atf_set "descr" "xbps-create(1): reject fifo file"
}

reject_fifo_file_body() {
	mkdir -p repo pkg_a
	mkfifo pkg_a/fifo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" pkg_a
	atf_check_equal $? 1
}

atf_test_case set_abi

set_abi_head() {
	atf_set "descr" "xbps-create(1): create package with abi field"
}

set_abi_body() {
	mkdir -p repo pkg_A/usr/include/gsm
	touch -f pkg_A/usr/include/gsm/gsm.h
	cd repo
	xbps-create -A noarch -n foo-1.0_1 -s "foo pkg" -a 1 ../pkg_A
	atf_check_equal $? 0
	cd ..
	xbps-rindex -d -a repo/*.xbps
	atf_check_equal $? 0
	rv="$(xbps-query -r root --repository=repo -p abi foo)"
	atf_check_equal $rv 1
}

atf_init_test_cases() {
	atf_add_test_case hardlinks_size
	atf_add_test_case symlink_relative_target
	atf_add_test_case symlink_relative_target_cwd
	atf_add_test_case symlink_sanitize
	atf_add_test_case restore_mtime
	atf_add_test_case reproducible_pkg
	atf_add_test_case reject_fifo_file
	atf_add_test_case set_abi
}
