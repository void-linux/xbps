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
	result="$(xbps-query -r root -C empty.conf --repository=$PWD/repo -p installed_size foo)"
	expected="10B"
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
}
